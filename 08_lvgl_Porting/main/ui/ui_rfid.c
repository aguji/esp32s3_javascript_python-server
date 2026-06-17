/*
 * RFID 模块实现：本地白名单优先，服务器兜底
 */
#include "ui_rfid.h"
#include "pn532/pn532_app.h"
#include "local_db.h"
#include "http_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

/* 登录优先级配置：
 * 0 = 优先本地数据库，失败则尝试服务器（本地优先，速度快）
 * 1 = 优先服务器，失败则尝试本地数据库
 * 2 = 只使用本地数据库（离线模式）
 * 3 = 只使用服务器（忽略本地数据库）
 * 注意：现在只使用本地数据库（模式2）
 */
#define RFID_LOGIN_PRIORITY 2

static ui_rfid_mode_t s_mode = UI_RFID_MODE_LOGIN;
static ui_rfid_callbacks_t s_cbs = { 0 };
static TaskHandle_t s_task_handle = NULL;

/* 检查本地数据库，返回匹配的用户信息（如果找到） */
static int check_local_whitelist(const char *uid_hex, char *user_id, char *display_name, int *is_admin)
{
    if (!uid_hex) return 0;

    // 显示所有数据库中的 RFID UID（用于调试）
    char db_uids[256] = {0};
    int len = local_db_get_all_readers_debug(db_uids, sizeof(db_uids));
    ESP_LOGI("RFID", "Looking for UID=%s, count=%d, DB: %s", uid_hex, g_reader_count, db_uids);

    char debug_msg[320];
    snprintf(debug_msg, sizeof(debug_msg), "DB: %d readers", g_reader_count);
    if (s_cbs.on_debug) s_cbs.on_debug(debug_msg);

    local_db_reader_t reader;
    if (local_db_find_reader_by_rfid(uid_hex, &reader)) {
        if (user_id) strncpy(user_id, reader.card_number, 31);
        if (display_name) strncpy(display_name, reader.name, 63);
        if (is_admin) *is_admin = reader.is_admin;
        ESP_LOGI("RFID", "Local database match: %s -> %s (admin=%d)",
                 uid_hex, reader.name, reader.is_admin);
        return 1;
    }

    ESP_LOGW("RFID", "Local database: NO match for UID=%s", uid_hex);
    return 0;
}

/**
 * @brief RFID 登录主入口函数（本地白名单优先，服务器兜底）
 * @param uid_hex 卡片UID（16进制字符串）
 *
 * 登录流程：
 * 1. 先查 ESP32 本地白名单
 *    ✅ 找到了 → 直接登录成功
 * 2. 本地没找到 → 调用服务器 API 查询
 *    ✅ 服务器找到 → 登录成功
 *    ❌ 服务器也没找到 → 登录失败
 */
static void rfid_login_to_server(const char *uid_hex)
{
    printf("[RFID] UID: %s\n", uid_hex);

    // 通知状态
    if (s_cbs.on_status) s_cbs.on_status("Checking local DB...");

    // Step 1: 查本地白名单
    char local_user_id[32] = {0};
    char local_display_name[64] = {0};
    int local_is_admin = 0;
    int found_locally = check_local_whitelist(uid_hex, local_user_id, local_display_name, &local_is_admin);

    if (found_locally) {
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg), "Found: %s", local_display_name);
        if (s_cbs.on_debug) s_cbs.on_debug(debug_msg);

        printf("[RFID] [Local] User found: %s (admin=%d)\n", local_display_name, local_is_admin);
        pn532_app_request_stop();
        if (s_cbs.on_login_ok) {
            s_cbs.on_login_ok(local_user_id, local_display_name);
        }
        return;
    }

    // 通知调试信息：本地没找到
    if (s_cbs.on_debug) s_cbs.on_debug("Not in local DB, trying server...");

    // Step 2: 本地没有，查询服务器
    printf("[RFID] [Local] Card not found, querying server...\n");
    if (s_cbs.on_status) s_cbs.on_status("Querying server...");

    login_result_t result = {0};
    esp_err_t err = http_client_check_rfid_login(uid_hex, &result);

    if (err == ESP_OK && strcmp(result.status, "success") == 0) {
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg), "Server OK: %s", result.user_name);
        if (s_cbs.on_debug) s_cbs.on_debug(debug_msg);

        printf("[RFID] [Server] Login success: %s\n", result.user_name);
        pn532_app_request_stop();
        if (s_cbs.on_login_ok) {
            s_cbs.on_login_ok(result.user_id, result.user_name);
        }
    } else {
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg), "Not found: %s", uid_hex);
        if (s_cbs.on_debug) s_cbs.on_debug(debug_msg);

        printf("[RFID] [Server] Card not found in database\n");
        if (s_cbs.on_login_fail) s_cbs.on_login_fail();
    }
}



static void pn532_status_cb(const char *msg)
{
    if (s_cbs.on_status && msg) s_cbs.on_status(msg);
}

static void pn532_card_cb(const uint8_t *uid, uint8_t uid_len)
{
    if (!uid || uid_len == 0) return;

    char buf[64];
    int pos = 0;
    for (uint8_t i = 0; i < uid_len && pos < (int)sizeof(buf) - 3; i++)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X", uid[i]);
    // 去掉末尾可能存在的空格
    if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = '\0';

    if (s_mode == UI_RFID_MODE_BORROW || s_mode == UI_RFID_MODE_RETURN) {
        if (s_cbs.on_book)
            s_cbs.on_book(buf, (s_mode == UI_RFID_MODE_RETURN) ? 1 : 0);
        return;
    }

    /* LOGIN: 先通知 UI 显示 UID，再调用服务器API验证 */
    if (s_cbs.on_login_uid) s_cbs.on_login_uid(buf);

    // 调用服务器API验证RFID卡
    rfid_login_to_server(buf);
}

static const pn532_app_callbacks_t pn532_cbs = {
    .on_status = pn532_status_cb,
    .on_card   = pn532_card_cb,
};

static void pn532_task(void *arg)
{
    const pn532_app_callbacks_t *cbs = (const pn532_app_callbacks_t *)arg;
    pn532_app_run(cbs);
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

void ui_rfid_register_callbacks(const ui_rfid_callbacks_t *cbs)
{
    if (cbs) s_cbs = *cbs;
}

void ui_rfid_set_mode(ui_rfid_mode_t mode)
{
    s_mode = mode;
}

void ui_rfid_start_reader(void)
{
    if (s_task_handle == NULL)
        xTaskCreate(pn532_task, "pn532_task", 4096, (void *)&pn532_cbs, 5, &s_task_handle);
}

void ui_rfid_request_stop(void)
{
    pn532_app_request_stop();
}
