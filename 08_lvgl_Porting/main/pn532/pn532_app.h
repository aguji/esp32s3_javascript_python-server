#pragma once

#include "esp_err.h"
#include <stddef.h>

// UI 回调，用于把 PN532 状态/读卡结果反馈到界面
typedef void (*pn532_status_cb_t)(const char *msg);
typedef void (*pn532_card_cb_t)(const uint8_t *uid, uint8_t uid_len);

typedef struct {
    pn532_status_cb_t on_status; // 状态变化，如“初始化中”“等待刷卡”等
    pn532_card_cb_t   on_card;   // 成功读到卡片时回调 UID
} pn532_app_callbacks_t;

// 初始化 PN532，并进入循环读卡（内部会一直阻塞，不返回）
// 通过 callbacks 把状态和读到的 UID 反馈给上层（可为 NULL）
void pn532_app_run(const pn532_app_callbacks_t *callbacks);

// 请求停止读卡循环，使 pn532_app_run 尽快返回
void pn532_app_request_stop(void);

/**
 * 快速自检：初始化 PN532 并读固件版本后释放，不进入读卡循环。
 * 若读卡任务正在运行会占用 UART，自检可能失败。
 * @param err_msg 失败时写入错误信息，可为 NULL
 * @param err_len err_msg 缓冲区长度
 * @return ESP_OK 成功，否则失败
 */
esp_err_t pn532_app_quick_test(char *err_msg, size_t err_len);

