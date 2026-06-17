/*
 * 借/还书界面模块
 * 注意：本文件须 UTF-8 编码
 */
#include "lvgl_port/lvgl.h"
#include "ui_common.h"
#include "ui_borrow.h"
#include "ui_rfid.h"
#include "user_session.h"
#include "local_db.h"
#include "http_client.h"
#include "infrared.h"
#include "wifi/wifi_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG_UI_BORROW = "ui_borrow";

/* LVGL 跨任务锁 */
extern bool lvgl_port_lock(int32_t timeout_ms);
extern void lvgl_port_unlock(void);

/* 借/还书静态变量 */
static int g_is_return_mode = 0;
static char g_last_uid[32] = { 0 };
static lv_obj_t *g_hint_label = NULL;
static lv_timer_t *g_ir_timer = NULL;
static char g_borrow_error[192] = { 0 };
static lv_obj_t *g_debug_label = NULL;  // 调试标签：显示发送/接收信息
static volatile bool g_rfid_busy = false;  // 防止 RFID 回调并发重入

static lv_obj_t *scr_borrow = NULL;
static lv_obj_t *scr_borrow_only = NULL;
static lv_obj_t *scr_return_only = NULL;
static lv_obj_t *lbl_borrow_hint = NULL;
static lv_obj_t *lbl_borrow_only_hint = NULL;
static lv_obj_t *lbl_return_only_hint = NULL;
static lv_obj_t *lbl_return_debug = NULL;   /* 红外调试状态标签 */

static lv_event_cb_t s_back_cb = NULL;

/* 延迟若干秒后切回借/还选择页 */
#define SUCCESS_DELAY_MS  2500

/* 异步处理结构体 */
typedef struct {
    char uid[32];
    int is_return;
} rfid_book_async_arg_t;

/* 红外触发后，用于 lv_async_call 携带 UID（避免 free 后访问已释放内存） */
typedef struct {
    char uid[32];
} rfid_return_async_arg_t;

/* 红外触发后，用于 lv_async_call 更新调试状态标签（始终在 LVGL 线程中执行） */
typedef struct {
    char msg[64];
} rfid_return_screen_arg_t;

/* 前向声明 */
static void go_to_borrow_delayed_cb(lv_timer_t *t);
static void ir_polling_cb(lv_timer_t *t);
static void rfid_on_book_async(void *arg);
static void rfid_on_return_ir_cb(void *arg);
static void rfid_on_return_ir_screen_cb(void *arg);
static bool net_borrow_book(const char *book_uid);
static bool net_return_book(const char *book_uid);
static void borrow_btn_cb(lv_event_t *e);
static void return_btn_cb(lv_event_t *e);
static void go_to_borrow_selection(lv_event_t *e);
static void go_from_borrow_back(lv_event_t *e);
static void build_borrow_screen(void);
static void build_borrow_only_screen(void);
static void build_return_only_screen(void);

/* 检查 WiFi 是否已连接 */
static bool is_wifi_connected(void)
{
    char buf[WIFI_CONFIG_SSID_MAX];
    return wifi_config_get_current_ssid(buf, sizeof(buf));
}

/* 同步借书请求：已连 WiFi 时写 MySQL（/borrow），否则仅本地 local_db */
static bool net_borrow_book(const char *book_uid)
{
    g_borrow_error[0] = '\0';

    const char *card_number = user_session_get_id();
    ESP_LOGI(TAG_UI_BORROW, "net_borrow_book: card=%s uid=%s",
             card_number ? card_number : "NULL", book_uid ? book_uid : "NULL");

    if (!card_number || card_number[0] == '\0' || strcmp(card_number, "guest") == 0) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "User not logged in");
        if (g_debug_label) lv_label_set_text(g_debug_label, "Please login first");
        return false;
    }

    if (is_wifi_connected()) {
        ESP_LOGI(TAG_UI_BORROW, "net_borrow_book: wifi connected, calling server...");
        char title[128] = {0};
        char err[160] = {0};
        esp_err_t e = http_client_borrow_book(card_number, book_uid, title, sizeof(title), err, sizeof(err));
        ESP_LOGI(TAG_UI_BORROW, "net_borrow_book: http result=%d title=%s err=%s", e, title, err);
        if (e == ESP_OK) {
            if (g_debug_label) {
                if (title[0]) {
                    lv_label_set_text_fmt(g_debug_label, "borrow ok: %s", title);
                } else {
                    lv_label_set_text(g_debug_label, "borrow ok (server)");
                }
            }
            ESP_LOGI(TAG_UI_BORROW, "server borrow ok: card=%s uid=%s title=%s", card_number, book_uid, title);
            /* 同步更新本地 local_db，使本地库状态与服务器一致 */
            local_db_reader_t reader_local;
            if (local_db_find_reader_by_card_number(card_number, &reader_local)) {
                local_db_book_t book_local;
                if (local_db_find_book_by_rfid(book_uid, &book_local)) {
                    local_db_borrow_book(reader_local.id, book_local.id, DEFAULT_BORROW_DAYS);
                }
            }
            return true;
        }
        snprintf(g_borrow_error, sizeof(g_borrow_error), "%s", err[0] ? err : "borrow failed");
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "%s", g_borrow_error);
        return false;
    }

    ESP_LOGW(TAG_UI_BORROW, "net_borrow_book: wifi NOT connected — offline path");
    /* 离线：仅本地白名单 */
    local_db_reader_t reader;
    if (!local_db_find_reader_by_card_number(card_number, &reader)) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Please login first (current user: %s)", card_number);
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "Please login first: %s", card_number);
        return false;
    }

    local_db_book_t book;
    if (!local_db_find_book_by_rfid(book_uid, &book)) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Book not found");
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "book isn't exisit: %s", book_uid);
        return false;
    }

    if (book.status != BOOK_STATUS_AVAILABLE) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Book already borrowed");
        if (g_debug_label) lv_label_set_text(g_debug_label, "book is already borrowed");
        return false;
    }

    esp_err_t err = local_db_borrow_book(reader.id, book.id, DEFAULT_BORROW_DAYS);
    if (err != ESP_OK) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Borrow failed: %s", esp_err_to_name(err));
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "borrow failed: %s", esp_err_to_name(err));
        return false;
    }

    if (g_debug_label) {
        lv_label_set_text_fmt(g_debug_label, "borrow success: %s", book.title);
    }
    ESP_LOGI(TAG_UI_BORROW, "local borrow success: reader_id=%d, book_id=%d, title=%s",
             reader.id, book.id, book.title);

    return true;
}

/* 同步还书请求：已连 WiFi 时写 MySQL（/return_book），否则仅本地 */
static bool net_return_book(const char *book_uid)
{
    g_borrow_error[0] = '\0';

    const char *card_number = user_session_get_id();
    ESP_LOGI(TAG_UI_BORROW, "net_return_book: card=%s uid=%s",
             card_number ? card_number : "NULL", book_uid ? book_uid : "NULL");

    if (!card_number || card_number[0] == '\0' || strcmp(card_number, "guest") == 0) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "User not logged in");
        ESP_LOGW(TAG_UI_BORROW, "net_return_book: no login");
        if (g_debug_label) lv_label_set_text(g_debug_label, "user didn't login");
        return false;
    }

    if (is_wifi_connected()) {
        ESP_LOGI(TAG_UI_BORROW, "net_return_book: wifi connected, calling server...");
        char err[160] = {0};
        esp_err_t e = http_client_return_book(card_number, book_uid, err, sizeof(err));
        ESP_LOGI(TAG_UI_BORROW, "net_return_book: http result=%d err=%s", e, err);
        if (e == ESP_OK) {
            if (g_debug_label) lv_label_set_text(g_debug_label, "return ok (server)");
            ESP_LOGI(TAG_UI_BORROW, "server return ok: card=%s uid=%s", card_number, book_uid);
            /* 同步更新本地 local_db，下次借书时本地库状态一致 */
            local_db_reader_t reader_local;
            local_db_book_t book_local;
            if (local_db_find_reader_by_card_number(card_number, &reader_local)
                && local_db_find_book_by_rfid(book_uid, &book_local)) {
                local_db_return_book(reader_local.id, book_local.id);
            }
            return true;
        }
        ESP_LOGW(TAG_UI_BORROW, "net_return_book: http failed e=%d err=%s", e, err);
        snprintf(g_borrow_error, sizeof(g_borrow_error), "%s", err[0] ? err : "return failed");
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "%s", g_borrow_error);
        return false;
    }

    ESP_LOGW(TAG_UI_BORROW, "net_return_book: wifi NOT connected — offline path");
    local_db_reader_t reader;
    if (!local_db_find_reader_by_card_number(card_number, &reader)) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Please login first (current user: %s)", card_number);
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "Please login first: %s", card_number);
        return false;
    }

    local_db_book_t book;
    if (!local_db_find_book_by_rfid(book_uid, &book)) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Book not found");
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "book isn't exisit: %s", book_uid);
        return false;
    }

    if (book.status != BOOK_STATUS_BORROWED) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Book not borrowed");
        if (g_debug_label) lv_label_set_text(g_debug_label, "book is not borrowed");
        return false;
    }

    esp_err_t err = local_db_return_book(reader.id, book.id);
    if (err != ESP_OK) {
        snprintf(g_borrow_error, sizeof(g_borrow_error), "Return failed: %s", esp_err_to_name(err));
        if (g_debug_label) lv_label_set_text_fmt(g_debug_label, "return failed: %s", esp_err_to_name(err));
        return false;
    }

    if (g_debug_label) {
        lv_label_set_text_fmt(g_debug_label, "return success: %s", book.title);
    }
    ESP_LOGI(TAG_UI_BORROW, "local return success: reader_id=%d, book_id=%d, title=%s",
             reader.id, book.id, book.title);

    return true;
}

/* 红外触发回调（在红外任务中执行，不能直接操作 LVGL） */
static void on_book_returned_cb(void)
{
    ESP_LOGI(TAG_UI_BORROW, "on_book_returned_cb called! g_last_uid=%s", g_last_uid);

    /* 先更新屏幕状态：显示"检测到信号，处理中..."（通过 lv_async_call 安全更新 UI） */
    rfid_return_screen_arg_t *screen_arg = malloc(sizeof(rfid_return_screen_arg_t));
    if (screen_arg) {
        memset(screen_arg, 0, sizeof(rfid_return_screen_arg_t));
        snprintf(screen_arg->msg, sizeof(screen_arg->msg), "[IR] Detected! Processing...");
        lv_async_call(rfid_on_return_ir_screen_cb, screen_arg);
    }

    /* 将 UID 拷贝到堆内存，通过 lv_async_call 投递到 LVGL 任务安全执行 */
    rfid_return_async_arg_t *arg = malloc(sizeof(rfid_return_async_arg_t));
    if (!arg) {
        ESP_LOGE(TAG_UI_BORROW, "OOM: cannot alloc return async arg");
        rfid_return_screen_arg_t *fail_arg = malloc(sizeof(rfid_return_screen_arg_t));
        if (fail_arg) {
            memset(fail_arg, 0, sizeof(rfid_return_screen_arg_t));
            snprintf(fail_arg->msg, sizeof(fail_arg->msg), "[ERR] OOM!");
            lv_async_call(rfid_on_return_ir_screen_cb, fail_arg);
        }
        return;
    }
    memset(arg, 0, sizeof(rfid_return_async_arg_t));
    strncpy(arg->uid, g_last_uid, sizeof(arg->uid) - 1);
    arg->uid[sizeof(arg->uid) - 1] = '\0';

    infrared_stop();   /* 停止红外轮询任务本身 */
    ui_rfid_request_stop();

    /* lv_async_call 在 LVGL 任务中执行 rfid_on_return_ir_cb，LVGL 锁在回调内部拿 */
    lv_async_call(rfid_on_return_ir_cb, arg);
}

/* 更新屏幕调试标签（通过 lv_async_call 调用，已在 LVGL 线程中，不需要锁） */
static void rfid_on_return_ir_screen_cb(void *arg)
{
    if (!arg) return;
    rfid_return_screen_arg_t *p = (rfid_return_screen_arg_t *)arg;

    if (lbl_return_debug) {
        lv_label_set_text(lbl_return_debug, p->msg);
    }

    free(p);
}

/* 红外确认后，在 LVGL 任务中执行还书 + UI 更新（通过 lv_async_call 调用，已在 LVGL 线程中） */
static void rfid_on_return_ir_cb(void *arg)
{
    if (!arg) return;
    rfid_return_async_arg_t *p = (rfid_return_async_arg_t *)arg;
    char uid[sizeof(p->uid)];
    strncpy(uid, p->uid, sizeof(uid) - 1);
    uid[sizeof(uid) - 1] = '\0';
    free(p);
    p = NULL;

    bool ok = net_return_book(uid);

    if (!ok) {
        g_rfid_busy = false;
        if (g_hint_label) {
            if (g_borrow_error[0]) {
                lv_label_set_text_fmt(g_hint_label, "Return failed: %s", g_borrow_error);
            } else {
                lv_label_set_text(g_hint_label, "Return failed");
            }
        }
    } else {
        g_rfid_busy = false;
        if (g_hint_label) {
            lv_label_set_text(g_hint_label, "Return success!");
        }
        lv_timer_t *tm = lv_timer_create(go_to_borrow_delayed_cb, SUCCESS_DELAY_MS, NULL);
        if (tm) lv_timer_set_repeat_count(tm, 1);
        else if (scr_borrow) lv_scr_load(scr_borrow);
    }
}

/* UID 防抖：忽略 2 秒内同一 UID 的重复回调，防止 PN532 快速轮询导致多次 POST */
static char s_last_seen_uid[32] = { 0 };
static uint32_t s_last_seen_tick = 0;
#define UID_DEBOUNCE_MS 2000

static bool uid_debounce(const char *uid)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (strncmp(uid, s_last_seen_uid, sizeof(s_last_seen_uid) - 1) == 0 &&
        now - s_last_seen_tick < UID_DEBOUNCE_MS) {
        ESP_LOGW(TAG_UI_BORROW, "uid_debounce: ignored duplicate uid=%s (%.1fs ago)",
                 uid, (float)(now - s_last_seen_tick) / 1000.0f);
        return false;  /* 忽略 */
    }
    strncpy(s_last_seen_uid, uid, sizeof(s_last_seen_uid) - 1);
    s_last_seen_uid[sizeof(s_last_seen_uid) - 1] = '\0';
    s_last_seen_tick = now;
    return true;  /* 通过 */
}

/* RFID 检测到书籍回调（异步版本 - 由 lv_async_call 在 LVGL 线程中执行，不需要 lvgl_port_lock）
 * 注意：lv_async_call 的回调始终在 LVGL 主线程中运行，
 * 直接操作 LVGL 对象即可，不需要互斥锁。 */
static void rfid_on_book_async(void *arg)
{
    rfid_book_async_arg_t *p = (rfid_book_async_arg_t *)arg;
    if (!p) return;

    /* 必须先拷贝 UID 再 free(p)，否则 uid 指向已释放内存 */
    char uid_stash[sizeof(p->uid)];
    memcpy(uid_stash, p->uid, sizeof(uid_stash));
    uid_stash[sizeof(uid_stash) - 1] = '\0';
    int is_return = p->is_return;
    free(p);
    p = NULL;
    const char *uid_hex = uid_stash;

    /* 防抖：2 秒内忽略同一 UID */
    if (!uid_debounce(uid_hex)) return;

    /* 防并发重入：上一次借/还书流程尚未结束，忽略本次回调 */
    if (g_rfid_busy) {
        ESP_LOGW(TAG_UI_BORROW, "rfid_on_book_async: BUSY — ignored uid=%s", uid_hex);
        return;
    }
    g_rfid_busy = true;
    bool ir_started = false;  /* 还书 IR 流程是否已启动（已启动则由 ir_polling_cb 结束时清 busy） */

    lv_obj_t *hint_lbl = is_return ? lbl_return_only_hint : lbl_borrow_only_hint;
    g_hint_label = hint_lbl;

    if (!uid_hex) {
        if (hint_lbl) lv_label_set_text(hint_lbl, "Invalid UID");
        goto cleanup;
    }

    if (hint_lbl) lv_label_set_text(hint_lbl, "Checking book...");

    /* 去掉UID中的空格 */
    char uid_clean[32] = { 0 };
    int j = 0;
    for (int i = 0; uid_hex[i] && j < (int)sizeof(uid_clean) - 1; i++) {
        if (uid_hex[i] != ' ') {
            uid_clean[j++] = uid_hex[i];
        }
    }

    local_db_book_t book;
    bool book_in_local = local_db_find_book_by_rfid(uid_clean, &book);

    ESP_LOGI(TAG_UI_BORROW, "UID: %s (cleaned: %s) local_book=%d wifi=%d is_return=%d",
             uid_hex, uid_clean, book_in_local ? 1 : 0,
             is_wifi_connected() ? 1 : 0, is_return);

    /* 离线借书：必须本地库有书 */
    if (!is_return && !is_wifi_connected() && !book_in_local) {
        ESP_LOGE(TAG_UI_BORROW, "Book not in local DB (offline): %s", uid_clean);
        if (hint_lbl) lv_label_set_text_fmt(hint_lbl, "Unknown book (UID: %s)", uid_hex);
        goto cleanup;
    }

    /* ==================== 借书模式 ==================== */
    if (!is_return) {
        /* 先停 PN532，防止 HTTP 期间再次读到同一张卡导致重复 POST */
        ui_rfid_request_stop();

        bool ok = net_borrow_book(uid_clean);

        if (!ok) {
            if (hint_lbl) {
                if (g_borrow_error[0]) {
                    lv_label_set_text_fmt(hint_lbl, "%s: %s", "Borrow failed", g_borrow_error);
                } else {
                    lv_label_set_text(hint_lbl, "Borrow failed");
                }
            }
            /* 失败后等 2.5s 自动返回 */
            lv_timer_t *tm = lv_timer_create(go_to_borrow_delayed_cb, SUCCESS_DELAY_MS, NULL);
            if (tm) lv_timer_set_repeat_count(tm, 1);
            else if (scr_borrow) lv_scr_load(scr_borrow);
            g_rfid_busy = false;
            return;
        }

        if (hint_lbl) lv_label_set_text(hint_lbl, "Borrow success!");
        lv_timer_t *tm = lv_timer_create(go_to_borrow_delayed_cb, SUCCESS_DELAY_MS, NULL);
        if (tm) lv_timer_set_repeat_count(tm, 1);
        else if (scr_borrow) lv_scr_load(scr_borrow);
        return;
    }

    /* ==================== 还书模式 ==================== */
    const char *card_ret = user_session_get_id();
    if (!card_ret || card_ret[0] == '\0' || strcmp(card_ret, "guest") == 0) {
        if (hint_lbl) lv_label_set_text(hint_lbl, "Please login first");
        goto cleanup;
    }

    if (is_wifi_connected()) {
        /* 在线：刷卡确认书目后，也必须物理放入还书箱 + 红外触发，才写 MySQL */
        snprintf(g_last_uid, sizeof(g_last_uid), "%s", uid_clean);
        ui_rfid_request_stop();

        if (hint_lbl) {
            lv_label_set_text_fmt(hint_lbl, "UID %s - Please put book into return box", uid_clean);
        }
        /* 红外只初始化一次；启动 FreeRTOS 任务做 GPIO 轮询 */
        infrared_init();
        infrared_start(NULL);
        if (g_ir_timer) lv_timer_del(g_ir_timer);
        g_ir_timer = lv_timer_create(ir_polling_cb, 100, NULL);
        ir_started = true;
        return;
    }

    /* 离线：本地校验后，红外确认书放入还书箱再 local_db 还书 */
    if (!book_in_local) {
        if (hint_lbl) lv_label_set_text_fmt(hint_lbl, "Unknown book (UID: %s)", uid_hex);
        goto cleanup;
    }
    if (book.status != BOOK_STATUS_BORROWED) {
        if (hint_lbl) lv_label_set_text(hint_lbl, "Book not borrowed");
        goto cleanup;
    }
    {
        local_db_reader_t reader;
        if (!local_db_find_reader_by_card_number(card_ret, &reader)) {
            if (hint_lbl) lv_label_set_text(hint_lbl, "User not found");
            goto offline_return_start_ir;
        }
        local_db_borrow_t borrows[10];
        int borrow_count = local_db_get_current_borrows(reader.id, borrows, 10);
        bool book_borrowed_by_user = false;
        for (int i = 0; i < borrow_count; i++) {
            if (borrows[i].book_id == book.id && borrows[i].status == BORROW_STATUS_BORROWED) {
                book_borrowed_by_user = true;
                break;
            }
        }
        if (!book_borrowed_by_user) {
            if (hint_lbl) lv_label_set_text(hint_lbl, "Book not borrowed by you");
            goto offline_return_start_ir;
        }
    }

offline_return_start_ir:
    if (hint_lbl && !ir_started) {
        lv_label_set_text_fmt(hint_lbl, "UID %s - Please put book into return box", uid_clean);
    }
    infrared_init();
    infrared_start(NULL);
    if (g_ir_timer) lv_timer_del(g_ir_timer);
    g_ir_timer = lv_timer_create(ir_polling_cb, 100, NULL);
    ir_started = true;
    return;  /* IR 流程已启动，由 ir_polling_cb 完成时清 g_rfid_busy */

cleanup:
    if (!ir_started) g_rfid_busy = false;
}

/* RFID 检测到书籍回调（由 PN532 任务调用，需异步到 LVGL 任务） */
static void rfid_on_book(const char *uid_hex, int is_return)
{
    if (!uid_hex) return;

    rfid_book_async_arg_t *arg = malloc(sizeof(rfid_book_async_arg_t));
    if (!arg) {
        ESP_LOGE(TAG_UI_BORROW, "Failed to allocate async arg");
        return;
    }
    memset(arg, 0, sizeof(rfid_book_async_arg_t));
    strncpy(arg->uid, uid_hex, sizeof(arg->uid) - 1);
    arg->uid[sizeof(arg->uid) - 1] = '\0';
    arg->is_return = is_return;

    /* 使用 lv_async_call 将网络操作转移到 LVGL 任务中执行 */
    lv_async_call(rfid_on_book_async, arg);
}

/* 延迟切回选择页回调 */
static void go_to_borrow_delayed_cb(lv_timer_t *t)
{
    (void)t;
    g_rfid_busy = false;
    if (scr_borrow) lv_scr_load(scr_borrow);
}

/* 红外轮询回调（在 LVGL 任务中执行，可安全更新 UI） */
static void ir_polling_cb(lv_timer_t *t)
{
    (void)t;
    static int detect_count = 0;

    /* 更新调试标签：实时显示 GPIO 电平 */
    int level = infrared_is_detected();
    if (lbl_return_debug) {
        if (level) {
            lv_label_set_text(lbl_return_debug, "IR: [BLOCKED] Place book in box");
            lv_obj_set_style_text_color(lbl_return_debug, lv_color_hex(0xFF6600), 0);
        } else {
            lv_label_set_text(lbl_return_debug, "IR: [CLEAR] Waiting for book...");
            lv_obj_set_style_text_color(lbl_return_debug, lv_color_hex(0x00CC44), 0);
        }
    }

    if (level) {
        detect_count++;
        if (detect_count >= 3) {
            detect_count = 0;
            if (g_ir_timer) {
                lv_timer_del(g_ir_timer);
                g_ir_timer = NULL;
            }
            /* 必须写库：仅改界面会导致 MySQL 仍为「在借」，下次借书 409 book already on loan */
            bool ok = net_return_book(g_last_uid);
            if (!ok) {
                if (g_hint_label) {
                    if (g_borrow_error[0]) {
                        lv_label_set_text_fmt(g_hint_label, "Return failed: %s", g_borrow_error);
                    } else {
                        lv_label_set_text(g_hint_label, "Return failed");
                    }
                }
                if (lbl_return_debug) {
                    lv_label_set_text(lbl_return_debug, "IR: detected but DB failed");
                }
                ESP_LOGW(TAG_UI_BORROW, "IR return path: net_return_book failed uid=%s", g_last_uid);
                g_rfid_busy = false;
                g_ir_timer = lv_timer_create(ir_polling_cb, 100, NULL);
            } else {
                if (g_hint_label) {
                    lv_label_set_text_fmt(g_hint_label, "Return success (UID: %s)", g_last_uid);
                }
                if (lbl_return_debug) {
                    lv_label_set_text(lbl_return_debug, "IR: [OK] Saved to server");
                }
                ESP_LOGI(TAG_UI_BORROW, "IR return path: net_return_book ok uid=%s", g_last_uid);
                infrared_stop();
                ui_rfid_request_stop();
                g_rfid_busy = false;
                lv_timer_t *tm = lv_timer_create(go_to_borrow_delayed_cb, SUCCESS_DELAY_MS, NULL);
                if (tm) lv_timer_set_repeat_count(tm, 1);
                else if (scr_borrow) lv_scr_load(scr_borrow);
            }
        }
    } else {
        detect_count = 0;
    }
}

/* 借书按钮回调 */
static void borrow_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    g_is_return_mode = 0;
    if (lbl_borrow_only_hint) lv_label_set_text(lbl_borrow_only_hint, "Please scan book to borrow");
    ui_rfid_set_mode(UI_RFID_MODE_BORROW);
    ui_rfid_start_reader();
    if (scr_borrow_only) lv_scr_load(scr_borrow_only);
}

/* 还书按钮回调 */
static void return_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    g_is_return_mode = 1;
    g_hint_label = lbl_return_only_hint;  /* 设置还书提示标签 */
    if (lbl_return_only_hint) lv_label_set_text(lbl_return_only_hint, "Please scan book to return");
    /* 初始化红外传感器 */
    infrared_init();
    ui_rfid_set_mode(UI_RFID_MODE_RETURN);
    ui_rfid_start_reader();
    if (scr_return_only) lv_scr_load(scr_return_only);
}

/* 跳转到借/还选择页 */
static void go_to_borrow(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !scr_borrow) return;
    lv_scr_load(scr_borrow);
}

/* 从借/还子页返回选择页 */
static void go_to_borrow_selection(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lvgl_port_lock(-1)) {
        if (g_ir_timer) {
            lv_timer_del(g_ir_timer);
            g_ir_timer = NULL;
        }
        ui_rfid_request_stop();
        infrared_stop();
        if (scr_borrow) lv_scr_load(scr_borrow);
        lvgl_port_unlock();
    }
}

/* 从借/还选择页返回主菜单 */
static void go_from_borrow_back(lv_event_t *e)
{
    if (g_ir_timer) {
        lv_timer_del(g_ir_timer);
        g_ir_timer = NULL;
    }
    infrared_stop();
    ui_rfid_request_stop();
    if (s_back_cb) {
        s_back_cb(e);
    }
}

/* 返回按钮回调 */
static void borrow_back_cb(lv_event_t *e)
{
    go_from_borrow_back(e);
}

/* ========== 界面构建 ========== */

/* 借/还选择页 */
static void build_borrow_screen(void)
{
    scr_borrow = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_borrow, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_borrow, 0, 0);
    lv_obj_set_style_pad_all(scr_borrow, 24, 24);
    ui_create_label(scr_borrow, "Borrow / Return", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 24, 24);
    lbl_borrow_hint = ui_create_label(scr_borrow, "Select Borrow or Return, then scan the book", FONT_CN,
                                      lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, -40);
    ui_create_btn(scr_borrow, 150, 50, "Borrow", borrow_btn_cb, LV_ALIGN_CENTER, -80, 20);
    ui_create_btn(scr_borrow, 150, 50, "Return", return_btn_cb, LV_ALIGN_CENTER, 80, 20);
    ui_create_btn(scr_borrow, 160, 52, "Back", go_from_borrow_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

/* 借书页 */
static void build_borrow_only_screen(void)
{
    scr_borrow_only = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_borrow_only, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_borrow_only, 0, 0);
    lv_obj_set_style_pad_all(scr_borrow_only, 24, 24);
    ui_create_label(scr_borrow_only, "Borrow", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 24, 24);
    lbl_borrow_only_hint = ui_create_label(scr_borrow_only, "Please scan the book", FONT_CN,
                                           lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, -20);
    // 调试标签：显示发送/接收信息
    g_debug_label = ui_create_label(scr_borrow_only, "Debug: waiting...", FONT_CN,
                                    lv_color_hex(0xFF0000), LV_ALIGN_CENTER, 0, 80);
    ui_create_btn(scr_borrow_only, 160, 52, "Back", go_to_borrow_selection, LV_ALIGN_BOTTOM_MID, 0, -24);
}

/* 还书页 */
static void build_return_only_screen(void)
{
    scr_return_only = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_return_only, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_return_only, 0, 0);
    lv_obj_set_style_pad_all(scr_return_only, 24, 24);
    ui_create_label(scr_return_only, "Return", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 24, 24);
    lbl_return_only_hint = ui_create_label(scr_return_only, "Please scan book to return", FONT_CN,
                                            lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, -20);
    g_hint_label = lbl_return_only_hint;
    /* 调试状态标签：显示红外传感器实时状态 */
    lbl_return_debug = ui_create_label(scr_return_only, "IR: --", FONT_CN,
                                       lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, 30);
    ui_create_btn(scr_return_only, 160, 52, "Back", go_to_borrow_selection, LV_ALIGN_BOTTOM_MID, 0, -24);
}

/* ========== 导出接口 ========== */

/* RFID 书籍回调（供 ui_rfid.c 调用） */
void ui_borrow_on_book(const char *uid_hex, int is_return)
{
    rfid_on_book(uid_hex, is_return);
}

/* 模块初始化 - 注册 RFID 回调 */
void ui_borrow_init(void)
{
    /* 借/还书模式使用与登录不同的 RFID 回调，
     * 由 ui_rfid.c 通过函数指针调用 ui_borrow_on_book */
}

/* 创建借/还书界面 */
void ui_borrow_create(lv_event_cb_t back_cb)
{
    s_back_cb = back_cb;
    build_borrow_screen();
    build_borrow_only_screen();
    build_return_only_screen();
}

/* 获取选择界面 */
lv_obj_t *ui_borrow_get_selection_screen(void)
{
    return scr_borrow;
}

/* 跳转到选择界面 */
void ui_borrow_go_to_selection(void)
{
    if (scr_borrow) lv_scr_load(scr_borrow);
}

/* 刷新主菜单 WiFi（供外部定时器调用） */
void ui_borrow_refresh_mainmenu_wifi(void)
{
    /* 该功能需要访问 library_ui.c 中的 lbl_mainmenu_wifi
     * 为简化设计，WiFi 状态刷新保留在 library_ui.c */
}
