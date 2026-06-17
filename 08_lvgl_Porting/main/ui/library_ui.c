/*
 * 图书馆管理系统 - 主界面与各子界面（仿 spi_lcd_touch 效果，800x480 实心按钮）
 * 注意：本文件须 UTF-8 编码
 * 模块化：RFID 逻辑见 ui_rfid.c，密码登录数据见 ui_login.c
 *        WiFi 设置见 ui_wifi.c，借/还书逻辑见 ui_borrow.c
 */
#include "lvgl.h"
#include "ui_common.h"
#include "ui_map.h"
#include "ui_search.h"
#include "ui_rfid.h"
#include "ui_login.h"
#include "ui_wifi.h"
#include "ui_borrow.h"
#include "ui_qr_login.h"  // 扫码登录模块
#include "infrared.h"
#include "user_session.h"
#include "local_db.h"

#include "wifi/wifi_config.h"
#include "pn532/pn532_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include "http_client/http_client.h"

static const char *TAG = "library_ui";

// 来自 lvgl_port（waveshare_rgb_lcd_port 间接提供），用于跨任务安全操作 LVGL
bool lvgl_port_lock(int32_t timeout_ms);
void lvgl_port_unlock(void);

static lv_obj_t *scr_main = NULL;
lv_obj_t *scr_login = NULL;
static lv_obj_t *scr_RFID = NULL;
static lv_obj_t *scr_map = NULL;
static lv_obj_t *scr_search = NULL;
static lv_obj_t *scr_map1 = NULL, *scr_map2 = NULL, *scr_map3 = NULL, *scr_map4 = NULL;
static lv_obj_t *scr_search_input = NULL, *scr_search_category = NULL, *kb_search = NULL;
static lv_obj_t *scr_passwordlogin = NULL;
static lv_obj_t *ta_username = NULL, *ta_password = NULL, *lbl_pwd_error = NULL, *kb_password = NULL;
lv_obj_t *scr_mainmenu = NULL;
static lv_obj_t *scr_personal = NULL;
static lv_obj_t *scr_nav_guide = NULL;
static lv_obj_t *scr_help = NULL;
static lv_obj_t *scr_admin = NULL;
static lv_obj_t *scr_selfcheck = NULL;
lv_obj_t *lbl_mainmenu_welcome = NULL;
static lv_obj_t *lbl_mainmenu_wifi = NULL;
static lv_timer_t *g_wifi_status_timer = NULL;

static lv_obj_t *lbl_selfcheck_rfid = NULL, *lbl_selfcheck_touch = NULL, *lbl_selfcheck_wifi = NULL;
static lv_obj_t *lbl_selfcheck_ir = NULL, *lbl_selfcheck_battery = NULL;
static lv_obj_t *lbl_personal_name = NULL, *lbl_personal_id = NULL;
static lv_obj_t *lbl_personal_cur_count = NULL, *lbl_personal_hist_count = NULL, *lbl_personal_credit = NULL;
static lv_obj_t *cont_personal_cur = NULL, *cont_personal_hist = NULL;
static lv_obj_t *lbl_rfid_hint = NULL;
static lv_obj_t *lbl_rfid_uid = NULL;
static lv_obj_t *lbl_rfid_debug = NULL;  // 调试信息标签

/* 前向声明 */
void refresh_mainmenu_wifi_label(void);

/* WiFi 状态定时刷新（每 5 秒） */
static void wifi_status_timer_cb(lv_timer_t *t)
{
    (void)t;
    refresh_mainmenu_wifi_label();
}

/* RFID 模块回调：在 PN532 任务里被调用，需加锁后更新 UI */
static void rfid_on_status(const char *msg)
{
    if (!msg) return;
    if (lvgl_port_lock(-1)) {
        if (scr_RFID && lbl_rfid_hint) lv_label_set_text(lbl_rfid_hint, msg);
        lvgl_port_unlock();
    }
}

static void rfid_on_login_uid(const char *uid_hex)
{
    if (lvgl_port_lock(-1)) {
        if (lbl_rfid_uid) lv_label_set_text_fmt(lbl_rfid_uid, "UID: %s", uid_hex ? uid_hex : "");
        if (lbl_rfid_debug) lv_label_set_text(lbl_rfid_debug, "Checking local DB...");
        lvgl_port_unlock();
    }
}

static void rfid_on_debug(const char *debug_msg)
{
    if (!debug_msg) return;
    if (lvgl_port_lock(-1)) {
        if (lbl_rfid_debug) lv_label_set_text(lbl_rfid_debug, debug_msg);
        lvgl_port_unlock();
    }
}


static void rfid_on_login_ok(const char *user_id, const char *display_name)
{
    printf("[UI] rfid_on_login_ok called: user_id=%s, display_name=%s\n", user_id, display_name);

    if (lvgl_port_lock(-1)) {
        if (!display_name) display_name = user_id;
        user_session_set(user_id, display_name);

        printf("[UI] After session_set: is_admin=%d\n", user_session_is_admin());

        if (user_session_is_admin()) {
            printf("[UI] Loading admin screen\n");
            if (scr_admin) lv_scr_load(scr_admin);
        } else {
            printf("[UI] Loading mainmenu screen\n");
            if (scr_mainmenu) {
                if (lbl_mainmenu_welcome)
                    lv_label_set_text_fmt(lbl_mainmenu_welcome, "Welcome, %s", user_session_get_name());
                refresh_mainmenu_wifi_label();
                lv_scr_load(scr_mainmenu);
            }
        }
        lvgl_port_unlock();
    }
}

static void rfid_on_login_fail(void)
{
    if (lvgl_port_lock(-1)) {
        if (lbl_rfid_hint) lv_label_set_text(lbl_rfid_hint, "Card not allowed");
        lvgl_port_unlock();
    }
}

/* ---------- 密码登录 ---------- */
static void pwd_ta_show_kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;
    lv_obj_t *ta = lv_event_get_target(e);
    if (kb_password && ta) {
        lv_keyboard_set_textarea(kb_password, ta);
        lv_obj_clear_flag(kb_password, LV_OBJ_FLAG_HIDDEN);
    }
}

static void pwd_kb_close_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CANCEL) return;
    if (kb_password) {
        lv_keyboard_set_textarea(kb_password, NULL);
        lv_obj_add_flag(kb_password, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 密码登录：校验与跳转（白名单在 ui_login.c） */
static void password_login_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!ta_username || !ta_password || !scr_main) return;
    const char *user = lv_textarea_get_text(ta_username);
    const char *pass = lv_textarea_get_text(ta_password);
    char display_name[32];
    if (ui_login_check_password(user ? user : "", pass ? pass : "", display_name, sizeof(display_name))) {
        if (lbl_pwd_error) lv_label_set_text(lbl_pwd_error, "");
        user_session_set(user, display_name);
        if (user_session_is_admin()) {
            if (scr_admin) lv_scr_load(scr_admin);
        } else {
            if (scr_mainmenu) {
                if (lbl_mainmenu_welcome)
                    lv_label_set_text_fmt(lbl_mainmenu_welcome, "Welcome, %s", user_session_get_name());
                refresh_mainmenu_wifi_label();
                lv_scr_load(scr_mainmenu);
            }
        }
    } else {
        if (lbl_pwd_error) lv_label_set_text(lbl_pwd_error, "Account or password error");
    }
}

/* ---------- 画面切换 ---------- */
static void go_to_map1(lv_event_t *e);
static void go_to_map2(lv_event_t *e);
static void go_to_map3(lv_event_t *e);
static void go_to_map4(lv_event_t *e);
static void go_to_search_input(lv_event_t *e);
static void go_to_search_category(lv_event_t *e);
static void go_back_to_search(lv_event_t *e);
static void go_back_from_sub(lv_event_t *e);
static void go_to_borrow_selection_cb(lv_event_t *e);

static void go_to_login(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_login) lv_scr_load(scr_login);
}

static void go_to_RFID(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_rfid_set_mode(UI_RFID_MODE_LOGIN);
    ui_rfid_start_reader();
    if (scr_RFID) lv_scr_load(scr_RFID);
}

static void go_to_main(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_main) lv_scr_load(scr_main);
}

static void go_back_from_sub(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (user_session_is_logged_in() && scr_mainmenu) {
        if (lbl_mainmenu_welcome)
            lv_label_set_text_fmt(lbl_mainmenu_welcome, "Welcome, %s", user_session_get_name());
        refresh_mainmenu_wifi_label();
        lv_scr_load(scr_mainmenu);
    } else if (scr_main) {
        lv_scr_load(scr_main);
    }
}

static void go_to_map(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!scr_map) {
        ui_map_create(&scr_map, &scr_map1, &scr_map2, &scr_map3, &scr_map4,
                      go_back_from_sub, go_to_map,
                      go_to_map1, go_to_map2, go_to_map3, go_to_map4);
    }
    if (scr_map) lv_scr_load(scr_map);
}

static void go_to_search(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!scr_search) {
        ui_search_create(&scr_search, &scr_search_input, &scr_search_category, &kb_search,
                         go_back_from_sub, go_to_search_input, go_to_search_category, go_back_to_search);
    }
    if (scr_search) lv_scr_load(scr_search);
}

static void go_to_map1(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_map1) lv_scr_load(scr_map1); }
static void go_to_map2(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_map2) lv_scr_load(scr_map2); }
static void go_to_map3(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_map3) lv_scr_load(scr_map3); }
static void go_to_map4(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_map4) lv_scr_load(scr_map4); }
static void go_to_search_input(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_search_input) lv_scr_load(scr_search_input); }
static void go_to_search_category(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_search_category) lv_scr_load(scr_search_category); }
static void go_back_to_search(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_search) lv_scr_load(scr_search); }

static void go_to_passwordlogin(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_passwordlogin) lv_scr_load(scr_passwordlogin);
}

static void go_to_mainmenu(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_mainmenu) {
        if (lbl_mainmenu_welcome)
            lv_label_set_text_fmt(lbl_mainmenu_welcome, "Welcome, %s", user_session_get_name());
        refresh_mainmenu_wifi_label();
        lv_scr_load(scr_mainmenu);
    }
}

static void go_to_admin(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_admin) lv_scr_load(scr_admin);
}

/** Main menu: only admin can enter maintenance */
__attribute__((unused)) static void go_to_maintenance_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!user_session_is_admin()) return;
    if (scr_admin) lv_scr_load(scr_admin);
}

/* 刷新主界面 WiFi 状态显示 */
void refresh_mainmenu_wifi_label(void)
{
    if (!lbl_mainmenu_wifi) return;
    char buf[WIFI_CONFIG_SSID_MAX];
    if (wifi_config_get_current_ssid(buf, sizeof(buf)))
        lv_label_set_text_fmt(lbl_mainmenu_wifi, "WiFi: %s", buf);
    else
        lv_label_set_text(lbl_mainmenu_wifi, "WiFi: Not connected");
}

/* WiFi 设置按钮回调 - 跳转到 WiFi 界面 */
static void go_to_wifi_settings(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t *scr_wifi = ui_wifi_get_screen();
    if (scr_wifi) {
        ui_wifi_refresh_status();
        lv_scr_load(scr_wifi);
    }
}

static void go_to_selfcheck(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lbl_selfcheck_rfid) lv_label_set_text(lbl_selfcheck_rfid, "--");
    if (lbl_selfcheck_touch) lv_label_set_text(lbl_selfcheck_touch, "--");
    if (lbl_selfcheck_wifi) lv_label_set_text(lbl_selfcheck_wifi, "--");
    if (lbl_selfcheck_ir) lv_label_set_text(lbl_selfcheck_ir, "--");
    if (lbl_selfcheck_battery) lv_label_set_text(lbl_selfcheck_battery, "--");
    if (scr_selfcheck) lv_scr_load(scr_selfcheck);
}

#define SELFCHECK_ERR_MAX  64

/* 自检任务参数：在任务内执行检测后更新对应 label */
typedef struct {
    int index;
} selfcheck_task_param_t;

static void selfcheck_task(void *arg)
{
    selfcheck_task_param_t *p = (selfcheck_task_param_t *)arg;
    int index = p->index;
    vPortFree(p);

    lv_obj_t *lbl = NULL;
    char msg[SELFCHECK_ERR_MAX];
    int ok = 0;

    switch (index) {
        case 0: /* RFID Module: 检测 PN532 能否正常通信 */
            lbl = lbl_selfcheck_rfid;
            if (pn532_app_quick_test(msg, sizeof(msg)) == ESP_OK)
                ok = 1;
            break;
        case 3: /* IR Sensor: 检测 E18 GPIO 能否正常 */
            lbl = lbl_selfcheck_ir;
            if (infrared_selfcheck(msg, sizeof(msg)) == 0)
                ok = 1;
            break;
        default:
            return;
    }

    if (lbl && lvgl_port_lock(-1)) {
        if (ok)
            lv_label_set_text(lbl, "OK");
        else
            lv_label_set_text(lbl, msg[0] ? msg : "Error");
        lvgl_port_unlock();
    }
    vTaskDelete(NULL);
}

/* 自检项：0=RFID 1=触摸屏 2=WiFi 3=红外 4=电池。RFID/红外按下去执行真实检测 */
static void selfcheck_btn_cb(lv_event_t *e, int index)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t *lbl = NULL;
    switch (index) {
        case 0: lbl = lbl_selfcheck_rfid;   break;
        case 1: lbl = lbl_selfcheck_touch;  break;
        case 2: lbl = lbl_selfcheck_wifi;   break;
        case 3: lbl = lbl_selfcheck_ir;     break;
        case 4: lbl = lbl_selfcheck_battery; break;
        default: return;
    }
    if (!lbl) return;

    if (index == 0 || index == 3) {
        lv_label_set_text(lbl, "Testing...");
        selfcheck_task_param_t *p = (selfcheck_task_param_t *)pvPortMalloc(sizeof(selfcheck_task_param_t));
        if (!p) {
            if (lvgl_port_lock(-1)) { lv_label_set_text(lbl, "No memory"); lvgl_port_unlock(); }
            return;
        }
        p->index = index;
        if (xTaskCreate(selfcheck_task, "selfcheck", 3072, p, 5, NULL) != pdPASS) {
            vPortFree(p);
            if (lvgl_port_lock(-1)) { lv_label_set_text(lbl, "Task create fail"); lvgl_port_unlock(); }
        }
    } else {
        lv_label_set_text(lbl, "OK");
    }
}

static void selfcheck_rfid_cb(lv_event_t *e)   { selfcheck_btn_cb(e, 0); }
static void selfcheck_touch_cb(lv_event_t *e)  { selfcheck_btn_cb(e, 1); }
static void selfcheck_wifi_cb(lv_event_t *e)  { selfcheck_btn_cb(e, 2); }
static void selfcheck_ir_cb(lv_event_t *e)    { selfcheck_btn_cb(e, 3); }
static void selfcheck_battery_cb(lv_event_t *e) { selfcheck_btn_cb(e, 4); }

/* 个人中心占位数据（后续可改为从后端/用户会话获取） */
static const char *personal_cur_books[] = { "C Programming", "Data Structures" };
static const char *personal_hist_books[] = { "Algorithm Design", "Operating Systems", "Computer Networks" };
#define PERSONAL_CUR_N   (sizeof(personal_cur_books) / sizeof(personal_cur_books[0]))
#define PERSONAL_HIST_N  (sizeof(personal_hist_books) / sizeof(personal_hist_books[0]))

static void personal_fill_lists(void)
{
    if (cont_personal_cur) {
        lv_obj_clean(cont_personal_cur);
        for (size_t i = 0; i < PERSONAL_CUR_N; i++) {
            lv_obj_t *lbl = lv_label_create(cont_personal_cur);
            lv_label_set_text(lbl, personal_cur_books[i]);
            lv_obj_set_style_text_font(lbl, FONT_CN, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT), 0);
            lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, (lv_coord_t)(i * 24));
        }
    }
    if (cont_personal_hist) {
        lv_obj_clean(cont_personal_hist);
        for (size_t i = 0; i < PERSONAL_HIST_N; i++) {
            lv_obj_t *lbl = lv_label_create(cont_personal_hist);
            lv_label_set_text(lbl, personal_hist_books[i]);
            lv_obj_set_style_text_font(lbl, FONT_CN, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT_MUTE), 0);
            lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, (lv_coord_t)(i * 24));
        }
    }
}

static void go_to_personal(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_personal) {
        if (lbl_personal_name) lv_label_set_text_fmt(lbl_personal_name, "Name: %s", user_session_get_name());
        if (lbl_personal_id) lv_label_set_text_fmt(lbl_personal_id, "ID: %s", user_session_get_id());
        if (lbl_personal_cur_count) lv_label_set_text_fmt(lbl_personal_cur_count, "%u", (unsigned)PERSONAL_CUR_N);
        if (lbl_personal_hist_count) lv_label_set_text_fmt(lbl_personal_hist_count, "%u", (unsigned)PERSONAL_HIST_N);
        if (lbl_personal_credit) lv_label_set_text(lbl_personal_credit, "100");
        personal_fill_lists();
        lv_scr_load(scr_personal);
    }
}

static void go_to_nav_guide(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_nav_guide) lv_scr_load(scr_nav_guide);
}

static void go_to_help(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && scr_help) lv_scr_load(scr_help);
}

static void go_to_logout(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    user_session_clear();
    if (scr_login) lv_scr_load(scr_login);
}

/* 中英语言切换回调 */
static void toggle_language_cb(lv_event_t *e)
{
    (void)e;
    ui_toggle_language();
    /* 重新加载主界面以应用新语言 */
    if (scr_main) lv_scr_load(scr_main);
}

/* ---------- 主画面 (800x480) ---------- */
static void build_main_screen(void)
{
    scr_main = lv_scr_act();
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(UI_COLOR_BG), 0);

    ui_create_label(scr_main, "Welcome To Use Library Management System", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 40);
    ui_create_label(scr_main, "Search, borrow and navigate", FONT_CN,
                    lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_MID, 0, 88);

    ui_create_btn(scr_main, 320, 56, "Login", go_to_login, LV_ALIGN_CENTER, 0, -60);
    ui_create_btn(scr_main, 320, 56, "View library layout", go_to_map, LV_ALIGN_CENTER, 0, 20);
    ui_create_btn(scr_main, 320, 56, "Search Book", go_to_search, LV_ALIGN_CENTER, 0, 100);
}

/* ---------- 登录选择 ---------- */
static void go_to_qrlogin_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_qrlogin_enter();
}

static void build_login_screen(void)
{
    scr_login = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_login, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_login, 0, 0);
    lv_obj_set_style_pad_all(scr_login, 0, 0);

    ui_create_label(scr_login, "Choose login method", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 32);

    ui_create_btn(scr_login, 320, 56, "RFID card", go_to_RFID, LV_ALIGN_CENTER, 0, -100);
    ui_create_btn(scr_login, 320, 56, "QR code", go_to_qrlogin_cb, LV_ALIGN_CENTER, 0, -24);
    ui_create_btn(scr_login, 320, 56, "Password", go_to_passwordlogin, LV_ALIGN_CENTER, 0, 52);
    ui_create_btn(scr_login, 200, 52, "Back", go_to_main, LV_ALIGN_CENTER, 0, 140);
}

/* ---------- RFID---------- */
static void build_rfid_stub_screen(void)
{
    scr_RFID = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_RFID, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_RFID, 0, 0);
    lv_obj_set_style_pad_all(scr_RFID, 0, 0);
    ui_create_label(scr_RFID, "RFID login", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 32);

    // 提示文字和 UID 显示，由 PN532 回调动态更新
    lbl_rfid_hint = ui_create_label(scr_RFID, "Initializing RFID reader...", FONT_CN,
                                    lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, -20);
    lbl_rfid_uid = ui_create_label(scr_RFID, "", FONT_CN,
                                   lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, 20);

    // 调试信息标签（显示数据库匹配结果）
    lbl_rfid_debug = ui_create_label(scr_RFID, "", FONT_CN,
                                     lv_color_hex(0x888888), LV_ALIGN_CENTER, 0, 50);

    ui_create_btn(scr_RFID, 160, 52, "Back", go_to_login, LV_ALIGN_BOTTOM_MID, 0, -32);
}

/* ---------- QR 登录屏 ---------- */
/* ---------- 密码登录 ---------- */
static void build_password_login_screen(void)
{
    scr_passwordlogin = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_passwordlogin, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_passwordlogin, 0, 0);
    lv_obj_set_style_pad_all(scr_passwordlogin, 0, 0);

    lv_obj_t *pwd_scroll = lv_obj_create(scr_passwordlogin);
    lv_obj_set_size(pwd_scroll, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(pwd_scroll, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(pwd_scroll, 0, 0);
    lv_obj_set_style_pad_all(pwd_scroll, 24, 24);
    lv_obj_set_style_pad_bottom(pwd_scroll, 220, 0);
    lv_obj_set_scroll_dir(pwd_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(pwd_scroll, LV_SCROLLBAR_MODE_AUTO);

    ui_create_label(pwd_scroll, "Password Login", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 16);
    ui_create_label(pwd_scroll, "Account", FONT_CN, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 0, 72);
    ta_username = lv_textarea_create(pwd_scroll);
    lv_textarea_set_one_line(ta_username, true);
    lv_textarea_set_max_length(ta_username, 32);
    lv_textarea_set_placeholder_text(ta_username, "account");
    lv_obj_set_size(ta_username, LV_PCT(100), 48);
    lv_obj_align(ta_username, LV_ALIGN_TOP_MID, 0, 104);
    lv_obj_add_event_cb(ta_username, pwd_ta_show_kb_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_username, pwd_ta_show_kb_cb, LV_EVENT_CLICKED, NULL);

    ui_create_label(pwd_scroll, "Password", FONT_CN, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 0, 172);
    ta_password = lv_textarea_create(pwd_scroll);
    lv_textarea_set_one_line(ta_password, true);
    lv_textarea_set_password_mode(ta_password, true);
    lv_textarea_set_max_length(ta_password, 32);
    lv_textarea_set_placeholder_text(ta_password, "password");
    lv_obj_set_size(ta_password, LV_PCT(100), 48);
    lv_obj_align(ta_password, LV_ALIGN_TOP_MID, 0, 204);
    lv_obj_add_event_cb(ta_password, pwd_ta_show_kb_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_password, pwd_ta_show_kb_cb, LV_EVENT_CLICKED, NULL);

    lbl_pwd_error = ui_create_label(pwd_scroll, "", FONT_CN,
                                    lv_color_hex(0xb91c1c), LV_ALIGN_TOP_MID, 0, 280);
    ui_create_btn(pwd_scroll, 140, 52, "Back", go_to_login, LV_ALIGN_TOP_LEFT, 0, 320);
    ui_create_btn(pwd_scroll, 140, 52, "Login", password_login_click_cb, LV_ALIGN_TOP_RIGHT, 0, 320);

    kb_password = lv_keyboard_create(scr_passwordlogin);
    lv_obj_set_size(kb_password, LV_PCT(88), LV_PCT(42));
    lv_obj_align(kb_password, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(kb_password, pwd_kb_close_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(kb_password, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- 用户登陆后界面 ---------- */
static void build_mainmenu_screen(void)
{
    scr_mainmenu = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_mainmenu, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_mainmenu, 0, 0);
    lv_obj_set_style_pad_all(scr_mainmenu, 24, 24);

    ui_create_label(scr_mainmenu, "Main Menu", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 24, 24);
    /* WiFi 状态显示 */
    lbl_mainmenu_wifi = ui_create_label(scr_mainmenu, "WiFi: Checking...", FONT_CN,
                    lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 24, 56);
    lv_obj_t *welcome_bar = lv_obj_create(scr_mainmenu);
    lv_obj_set_size(welcome_bar, LV_PCT(100), 48);
    lv_obj_align(welcome_bar, LV_ALIGN_TOP_LEFT, 0, 96);
    lv_obj_set_style_bg_color(welcome_bar, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_color(welcome_bar, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(welcome_bar, 1, 0);
    lbl_mainmenu_welcome = ui_create_label(welcome_bar, "Welcome, visitor", FONT_CN,
                                            lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_RIGHT_MID, -24, 0);

    int row1 = 164, row2 = 236, row3 = 308;
    int col1 = 100, col2 = 500;
    ui_create_btn(scr_mainmenu, 220, 56, "Book search", go_to_search, LV_ALIGN_TOP_LEFT, col1, row1);
    ui_create_btn(scr_mainmenu, 220, 56, "Borrow/Return", go_to_borrow_selection_cb, LV_ALIGN_TOP_LEFT, col2, row1);
    ui_create_btn(scr_mainmenu, 220, 56, "Personal", go_to_personal, LV_ALIGN_TOP_LEFT, col1, row2);
    ui_create_btn(scr_mainmenu, 220, 56, "Navigation", go_to_nav_guide, LV_ALIGN_TOP_LEFT, col2, row2);
    ui_create_btn(scr_mainmenu, 220, 56, "Help", go_to_help, LV_ALIGN_TOP_LEFT, col1, row3);
    ui_create_btn(scr_mainmenu, 220, 56, "Logout", go_to_logout, LV_ALIGN_TOP_LEFT, col2, row3);
}

/* ---------- 管理员：维护模式（系统自检入口） ---------- */
static void build_admin_screen(void)
{
    scr_admin = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_admin, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_admin, 0, 0);
    lv_obj_set_style_pad_all(scr_admin, 24, 24);
    ui_create_label(scr_admin, "Maintenance", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 28);
    ui_create_label(scr_admin, "Maintenance Mode", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_MID, 0, 60);
    ui_create_btn(scr_admin, 280, 56, "System Self-Check", go_to_selfcheck, LV_ALIGN_CENTER, 0, -60);
    ui_create_btn(scr_admin, 280, 56, "WiFi Settings", go_to_wifi_settings, LV_ALIGN_CENTER, 0, 10);
    ui_create_btn(scr_admin, 200, 52, "Back to Menu", go_to_main, LV_ALIGN_CENTER, 0, 90);
}

/* 定时刷新 IR Sensor 状态到屏幕（无串口时用）：L=电平 T=任务 TRIG=刚触发 */
static void selfcheck_ir_status_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!scr_selfcheck || !lbl_selfcheck_ir || lv_scr_act() != scr_selfcheck) return;
    char buf[32];
    infrared_get_status_str(buf, sizeof(buf));
    lv_label_set_text(lbl_selfcheck_ir, buf);
}

/* ---------- 系统自检：五项检测，点击后显示 ✅正常 / ❌异常 ---------- */
static void build_selfcheck_screen(void)
{
    scr_selfcheck = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_selfcheck, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_selfcheck, 0, 0);
    lv_obj_set_style_pad_all(scr_selfcheck, 20, 20);
    ui_create_label(scr_selfcheck, "System Self-Check", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 16);
    int y = 56;
    int col_btn = 80, col_result = 380;
    ui_create_btn(scr_selfcheck, 260, 48, "RFID Module", selfcheck_rfid_cb, LV_ALIGN_TOP_LEFT, col_btn, y);
    lbl_selfcheck_rfid = ui_create_label(scr_selfcheck, "--", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, col_result, y + 14);
    y += 56;
    ui_create_btn(scr_selfcheck, 260, 48, "Touchscreen", selfcheck_touch_cb, LV_ALIGN_TOP_LEFT, col_btn, y);
    lbl_selfcheck_touch = ui_create_label(scr_selfcheck, "--", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, col_result, y + 14);
    y += 56;
    ui_create_btn(scr_selfcheck, 260, 48, "WiFi Module", selfcheck_wifi_cb, LV_ALIGN_TOP_LEFT, col_btn, y);
    lbl_selfcheck_wifi = ui_create_label(scr_selfcheck, "--", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, col_result, y + 14);
    y += 56;
    ui_create_btn(scr_selfcheck, 260, 48, "IR Sensor", selfcheck_ir_cb, LV_ALIGN_TOP_LEFT, col_btn, y);
    lbl_selfcheck_ir = ui_create_label(scr_selfcheck, "--", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, col_result, y + 14);
    y += 56;
    ui_create_btn(scr_selfcheck, 260, 48, "Battery", selfcheck_battery_cb, LV_ALIGN_TOP_LEFT, col_btn, y);
    lbl_selfcheck_battery = ui_create_label(scr_selfcheck, "--", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, col_result, y + 14);
    ui_create_btn(scr_selfcheck, 160, 52, "Back", go_to_admin, LV_ALIGN_BOTTOM_MID, 0, -24);
    /* 约 500ms 刷新一次 IR 状态，便于无串口时在维护页看 L/T/TRIG */
    lv_timer_create(selfcheck_ir_status_timer_cb, 500, NULL);
}

/* ---------- 个人中心（账户名、账号、借阅数、信用度、当前/历史借阅列表） ---------- */
static void build_personal_screen(void)
{
    scr_personal = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_personal, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_personal, 0, 0);
    lv_obj_set_style_pad_all(scr_personal, 20, 20);

    int y = 12;
    ui_create_label(scr_personal, "Personal Center", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, y);
    y += 36;

    /* 账户名、账号 同一行 */
    lbl_personal_name = ui_create_label(scr_personal, "Name: --", FONT_CN, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 20, y);
    lbl_personal_id = ui_create_label(scr_personal, "ID: --", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 320, y);
    y += 28;

    /* 当前借阅数、历史借阅数、信用度 一行 */
    ui_create_label(scr_personal, "Current:", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 20, y);
    lbl_personal_cur_count = ui_create_label(scr_personal, "0", FONT_CN, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 92, y);
    ui_create_label(scr_personal, "History:", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 160, y);
    lbl_personal_hist_count = ui_create_label(scr_personal, "0", FONT_CN, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 232, y);
    ui_create_label(scr_personal, "Credit:", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 320, y);
    lbl_personal_credit = ui_create_label(scr_personal, "--", FONT_CN, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 380, y);
    y += 28;

    /* 当前借阅书籍 */
    ui_create_label(scr_personal, "Current borrowed", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 20, y);
    y += 22;
    cont_personal_cur = lv_obj_create(scr_personal);
    lv_obj_set_size(cont_personal_cur, 760, 88);
    lv_obj_set_style_bg_color(cont_personal_cur, lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_border_color(cont_personal_cur, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(cont_personal_cur, 1, 0);
    lv_obj_set_style_radius(cont_personal_cur, 6, 0);
    lv_obj_set_style_pad_all(cont_personal_cur, 8, 0);
    lv_obj_set_scroll_dir(cont_personal_cur, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont_personal_cur, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_align(cont_personal_cur, LV_ALIGN_TOP_LEFT, 20, y);
    y += 96;

    /* 历史借阅书籍 */
    ui_create_label(scr_personal, "History borrowed", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_TOP_LEFT, 20, y);
    y += 22;
    cont_personal_hist = lv_obj_create(scr_personal);
    lv_obj_set_size(cont_personal_hist, 760, 88);
    lv_obj_set_style_bg_color(cont_personal_hist, lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_border_color(cont_personal_hist, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(cont_personal_hist, 1, 0);
    lv_obj_set_style_radius(cont_personal_hist, 6, 0);
    lv_obj_set_style_pad_all(cont_personal_hist, 8, 0);
    lv_obj_set_scroll_dir(cont_personal_hist, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont_personal_hist, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_align(cont_personal_hist, LV_ALIGN_TOP_LEFT, 20, y);

    ui_create_btn(scr_personal, 160, 52, "Back", go_to_mainmenu, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_nav_guide_screen(void)
{
    scr_nav_guide = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_nav_guide, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_nav_guide, 0, 0);
    lv_obj_set_style_pad_all(scr_nav_guide, 24, 24);
    ui_create_label(scr_nav_guide, "Navigation", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 24, 24);
    ui_create_btn(scr_nav_guide, 320, 56, "View library layout", go_to_map, LV_ALIGN_CENTER, 0, -50);
    ui_create_btn(scr_nav_guide, 320, 56, "Search book location", go_to_search, LV_ALIGN_CENTER, 0, 30);
    ui_create_btn(scr_nav_guide, 160, 52, "Back", go_to_mainmenu, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_help_screen(void)
{
    scr_help = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_help, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_help, 0, 0);
    lv_obj_set_style_pad_all(scr_help, 24, 24);
    ui_create_label(scr_help, "Help", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 24, 24);
    ui_create_btn(scr_help, 160, 52, "Back", go_to_mainmenu, LV_ALIGN_BOTTOM_MID, 0, -24);
}

/* 借书/还书回调 - 转发到 ui_borrow 模块 */
static void rfid_on_book(const char *uid_hex, int is_return)
{
    ui_borrow_on_book(uid_hex, is_return);
}

/* 按钮回调 - 跳转到借/还选择界面（包装 lv_event_cb_t） */
static void go_to_borrow_selection_cb(lv_event_t *e)
{
    (void)e;  /* unused */
    ui_borrow_go_to_selection();
}

/* 返回按钮回调 - 跳转到主菜单 */
static void back_to_mainmenu_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (scr_mainmenu) lv_scr_load(scr_mainmenu);
}

/* ---------- 入口 ---------- */
void library_ui_create(void)
{
    /* 初始化本地数据库 */
    local_db_init();
    
    /* 先初始化借/还书模块（注册 RFID 回调） */
    ui_borrow_init();

    ui_rfid_callbacks_t rfid_cbs = {
        .on_status    = rfid_on_status,
        .on_login_uid = rfid_on_login_uid,
        .on_login_ok  = rfid_on_login_ok,
        .on_login_fail = rfid_on_login_fail,
        .on_book      = rfid_on_book,
        .on_debug     = rfid_on_debug,
    };
    ui_rfid_register_callbacks(&rfid_cbs);
    infrared_init();

    build_main_screen();
    build_login_screen();
    build_rfid_stub_screen();
    ui_qrlogin_init();  // 初始化扫码登录界面
    build_password_login_screen();
    build_mainmenu_screen();
    build_admin_screen();
    build_selfcheck_screen();
    ui_wifi_create(go_to_admin);  /* WiFi 设置界面 */
    build_personal_screen();
    build_nav_guide_screen();
    ui_borrow_create(back_to_mainmenu_cb);  /* 借/还书界面 */
    build_help_screen();

    /* 楼层图在 ui_map_create() 内通过 ui_map_set_floor_image 绑定（见 ui_floor_map_assets.c） */

    /* 启动 WiFi 状态定时刷新（每 5 秒） */
    g_wifi_status_timer = lv_timer_create(wifi_status_timer_cb, 5000, NULL);
}

/* 清理 UI 资源（系统在关机时调用） */
void library_ui_destroy(void)
{
    if (g_wifi_status_timer) {
        lv_timer_del(g_wifi_status_timer);
        g_wifi_status_timer = NULL;
    }
    ui_qrlogin_stop();  // 清理扫码登录
    ui_rfid_request_stop();
    infrared_stop();
}
