/*
 * 扫码登录界面 - 独立模块
 * 包含二维码生成和HTTP轮询登录逻辑
 */
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "http_client/http_client.h"
#include "user_session.h"
#include "ui_common.h"
#include "src/extra/libs/qrcode/lv_qrcode.h"

/* 外部UI元素和函数（由library_ui.c提供） */
extern lv_obj_t *scr_login;
extern lv_obj_t *scr_mainmenu;
extern lv_obj_t *lbl_mainmenu_welcome;
extern void refresh_mainmenu_wifi_label(void);

/* 轮询在独立任务中跑，更新 UI 时必须加锁（与 lvgl_port 一致） */
extern bool lvgl_port_lock(int32_t timeout_ms);
extern void lvgl_port_unlock(void);

/* 服务器配置（统一从 http_client.h 读取，一处改全局生效） */
#define QR_SERVER_IP   HTTP_CLIENT_SERVER_IP
#define QR_SERVER_PORT HTTP_CLIENT_SERVER_PORT

/* 轮询配置 */
#define QR_POLL_INTERVAL_MS  2000   // 轮询间隔
#define QR_POLL_MAX_COUNT    90     // 最多轮询90次（约3分钟）
#define QR_RETRY_DELAY_MS    3000   // 重试延迟
#define QR_POLL_TASK_STACK   8192
#define QR_POLL_TASK_PRIO    5

/* ==================== 静态变量 ==================== */
static const char *TAG = "ui_qr_login";

static lv_obj_t *s_scr_QRlogin = NULL;   // 扫码登录屏幕
static lv_obj_t *s_qr_code_obj = NULL;   // 二维码对象
static lv_obj_t *s_lbl_status = NULL;     // 状态标签
static lv_obj_t *s_lbl_token = NULL;     // Token标签（调试用）
static TaskHandle_t s_poll_task_handle = NULL;
static int s_poll_count = 0;             // 轮询计数器
static bool s_stopping = false;         // 停止标志，防止回调在停止后继续执行
static bool s_login_success = false;     // 登录成功标志，防止重复跳转

/* ==================== 内部函数声明 ==================== */
static void poll_task(void *pvParameters);
static void qr_poll_delete_task_if_any(void);
static void back_cb(lv_event_t *e);
static void start_login(void);

/* ==================== 公共函数 ==================== */

/**
 * @brief 获取扫码登录屏幕对象
 */
lv_obj_t *ui_qrlogin_get_screen(void)
{
    return s_scr_QRlogin;
}

/**
 * @brief 进入扫码登录界面
 */
void ui_qrlogin_enter(void)
{
    if (!s_scr_QRlogin) return;

    /* 由 LVGL 任务内的事件触发，已在 lvgl_port 互斥保护下，勿再套 lvgl_port_lock */
    lv_scr_load(s_scr_QRlogin);
    start_login();
}

/**
 * @brief 停止扫码登录（清理资源）
 */
void ui_qrlogin_stop(void)
{
    s_stopping = true;
    s_login_success = false;
    qr_poll_delete_task_if_any();
    s_poll_count = 0;
    http_client_reset_login();
}

/**
 * @brief 初始化扫码登录界面
 */
void ui_qrlogin_init(void)
{
    if (s_scr_QRlogin) return;  // 已初始化

    /* 创建屏幕 */
    s_scr_QRlogin = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_QRlogin, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(s_scr_QRlogin, 0, 0);
    lv_obj_set_style_pad_all(s_scr_QRlogin, 0, 0);

    /* 标题 */
    ui_create_label(s_scr_QRlogin, "QR code login", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 32);

    /* 二维码 */
    lv_color_t fg = lv_color_hex(0x111827);
    lv_color_t bg = lv_color_hex(0xFFFFFF);
    s_qr_code_obj = lv_qrcode_create(s_scr_QRlogin, 220, fg, bg);
    lv_obj_align(s_qr_code_obj, LV_ALIGN_CENTER, 0, -10);

    /* 状态标签 */
    s_lbl_status = ui_create_label(s_scr_QRlogin, "Waiting...", FONT_CN,
                                    lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, 150);
    /* Token标签（调试用） */
    s_lbl_token = ui_create_label(s_scr_QRlogin, "", FONT_CN,
                                  lv_color_hex(UI_COLOR_TEXT_MUTE), LV_ALIGN_CENTER, 0, 175);

    /* 返回按钮 */
    ui_create_btn(s_scr_QRlogin, 160, 52, "Back", back_cb, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    ESP_LOGI(TAG, "QR login screen initialized");
}

/* ==================== 内部函数实现 ==================== */

static void qr_poll_delete_task_if_any(void)
{
    if (s_poll_task_handle == NULL) {
        return;
    }
    vTaskDelete(s_poll_task_handle);
    s_poll_task_handle = NULL;
}

/** HTTP 在独立任务执行；凡操作 LVGL 必须 lvgl_port_lock（非 LVGL 线程） */
static void poll_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "qr poll task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(QR_POLL_INTERVAL_MS));

        if (s_stopping) {
            break;
        }
        if (s_login_success) {
            break;
        }

        s_poll_count++;
        if (s_poll_count > QR_POLL_MAX_COUNT) {
            http_client_reset_login();
            if (lvgl_port_lock(-1)) {
                if (s_lbl_status) {
                    lv_label_set_text(s_lbl_status, "Timeout");
                }
                if (scr_login) {
                    lv_scr_load(scr_login);
                }
                lvgl_port_unlock();
            }
            break;
        }

        if (!http_client_has_valid_token()) {
            if (lvgl_port_lock(-1)) {
                if (s_lbl_status) {
                    lv_label_set_text(s_lbl_status, "Token expired");
                }
                if (scr_login) {
                    lv_scr_load(scr_login);
                }
                lvgl_port_unlock();
            }
            break;
        }

        if (lvgl_port_lock(-1)) {
            if (s_lbl_token) {
                lv_label_set_text_fmt(s_lbl_token, "Poll#%d chk...", s_poll_count);
            }
            lvgl_port_unlock();
        }

        login_result_t result;
        memset(&result, 0, sizeof(result));
        bool success = http_client_check_login_with_user(&result);

        if (s_stopping) {
            break;
        }

        if (lvgl_port_lock(-1)) {
            if (s_lbl_token) {
                lv_label_set_text_fmt(s_lbl_token, "Poll#%d: %s", s_poll_count,
                                      result.status[0] ? result.status : "?");
            }
            if (s_poll_count % 5 == 0 && s_lbl_status && !success) {
                lv_label_set_text(s_lbl_status, "Waiting for scan...");
            }
            lvgl_port_unlock();
        }

        if (success) {
            s_login_success = true;
            if (lvgl_port_lock(-1)) {
                if (s_lbl_status) {
                    lv_label_set_text(s_lbl_status, "Login successful!");
                }
                user_session_set(result.user_id[0] ? result.user_id : "unknown",
                                 result.user_name[0] ? result.user_name : "User");
                if (scr_mainmenu) {
                    if (lbl_mainmenu_welcome) {
                        lv_label_set_text_fmt(lbl_mainmenu_welcome, "Welcome, %s",
                                              user_session_get_name());
                    }
                    refresh_mainmenu_wifi_label();
                    lv_scr_load(scr_mainmenu);
                }
                lvgl_port_unlock();
            }
            break;
        }

        if (!http_client_has_valid_token()) {
            if (lvgl_port_lock(-1)) {
                if (s_lbl_status) {
                    lv_label_set_text(s_lbl_status, "Token expired");
                }
                if (scr_login) {
                    lv_scr_load(scr_login);
                }
                lvgl_port_unlock();
            }
            break;
        }
    }

    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    if (s_poll_task_handle == self) {
        s_poll_task_handle = NULL;
    }
    vTaskDelete(NULL);
}

/**
 * @brief 返回按钮回调
 */
static void back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ESP_LOGI(TAG, "Back button pressed, stopping QR login");

    s_stopping = true;
    s_login_success = false;
    qr_poll_delete_task_if_any();
    s_poll_count = 0;
    http_client_reset_login();

    ESP_LOGI(TAG, "Loading login screen");
    if (scr_login) {
        lv_scr_load(scr_login);
    } else {
        ESP_LOGE(TAG, "scr_login is NULL!");
    }
}

// /**
//  * @brief 开始扫码登录流程
//  */
// static void start_login(void)
// {
//     if (!s_scr_QRlogin) return;

//     /* 清理旧的定时器 */
//     if (s_poll_timer) {
//         lv_timer_del(s_poll_timer);
//         s_poll_timer = NULL;
//     }
//     s_poll_count = 0;

//     /* 显示连接提示 */
//     if (s_lbl_status) lv_label_set_text(s_lbl_status, "Connecting to server...");

//     /* 请求token */
//     if (http_client_start_login(QR_SERVER_IP, QR_SERVER_PORT)) {
//         const char *token = http_client_get_token();
//         if (token && strlen(token) > 0) {
//             /* 生成二维码内容 */
//             char qr_data[256];
//             snprintf(qr_data, sizeof(qr_data), "http://%s:%d/login?token=%s",
//                      QR_SERVER_IP, QR_SERVER_PORT, token);

//             /* 更新二维码 */
//             if (s_qr_code_obj) {
//                 lv_qrcode_update(s_qr_code_obj, qr_data, strlen(qr_data));
//             }

//             if (s_lbl_status) lv_label_set_text(s_lbl_status, "Scan QR code with your phone");
//             if (s_lbl_token) lv_label_set_text(s_lbl_token, token);

//             ESP_LOGI(TAG, "QR code ready, token: %s", token);

//             /* 启动轮询定时器 */
//             s_poll_timer = lv_timer_create(poll_cb, QR_POLL_INTERVAL_MS, NULL);
//             if (s_poll_timer) {
//                 lv_timer_set_repeat_count(s_poll_timer, 0);  // 无限重复
//             }
//         } else {
//             /* Token为空 */
//             if (s_lbl_status) lv_label_set_text(s_lbl_status, "Error: Empty token");
//             ESP_LOGE(TAG, "Token is empty!");
//             /* 延迟重试 */
//             s_poll_timer = lv_timer_create(retry_start_cb, QR_RETRY_DELAY_MS, NULL);
//             if (s_poll_timer) lv_timer_set_repeat_count(s_poll_timer, 1);
//         }
//     } else {
//         /* 连接失败 */
//         if (s_lbl_status) lv_label_set_text(s_lbl_status, "Server unreachable, retry...");
//         http_client_reset_login();
//         ESP_LOGW(TAG, "Failed to connect to server, retrying...");

//         /* 延迟重试 */
//         s_poll_timer = lv_timer_create(retry_start_cb, QR_RETRY_DELAY_MS, NULL);
//         if (s_poll_timer) lv_timer_set_repeat_count(s_poll_timer, 1);
//     }
// }


static void start_login(void)
{
    if (!s_scr_QRlogin) {
        return;
    }

    qr_poll_delete_task_if_any();

    s_stopping = false;
    s_login_success = false;
    s_poll_count = 0;

    if (s_lbl_status) {
        lv_label_set_text(s_lbl_status, "Connecting...");
    }

    bool started = http_client_start_login(QR_SERVER_IP, QR_SERVER_PORT);
    if (!started) {
        if (s_lbl_status) {
            lv_label_set_text(s_lbl_status, "HTTP start failed");
        }
        if (s_lbl_token) {
            lv_label_set_text(s_lbl_token, "register_token fail");
        }
        ESP_LOGE(TAG, "http_client_start_login failed");
        return;
    }

    const char *token = http_client_get_token();
    if (token == NULL || strlen(token) == 0) {
        if (s_lbl_status) {
            lv_label_set_text(s_lbl_status, "Error: Empty token");
        }
        if (s_lbl_token) {
            lv_label_set_text(s_lbl_token, "");
        }
        return;
    }

    char qr_data[256];
    snprintf(qr_data, sizeof(qr_data), "http://%s:%d/login?token=%s",
             QR_SERVER_IP, QR_SERVER_PORT, token);

    if (s_lbl_token) {
        lv_label_set_text(s_lbl_token, token);
    }
    if (s_lbl_status) {
        lv_label_set_text(s_lbl_status, "Scan QR code");
    }
    if (s_qr_code_obj) {
        lv_qrcode_update(s_qr_code_obj, qr_data, strlen(qr_data));
    }

    if (xTaskCreate(poll_task, "qr_poll", QR_POLL_TASK_STACK, NULL, QR_POLL_TASK_PRIO,
                    &s_poll_task_handle) != pdPASS) {
        s_poll_task_handle = NULL;
        if (s_lbl_token) {
            lv_label_set_text(s_lbl_token, "Task create failed");
        }
        ESP_LOGE(TAG, "xTaskCreate qr_poll failed");
    } else {
        if (s_lbl_token) {
            lv_label_set_text(s_lbl_token, "Poll task running");
        }
        ESP_LOGI(TAG, "qr poll task created");
    }
}

// //先测试没有http能不能生成固定的二维码
// static void start_login(void)
// {
//     if (!s_scr_QRlogin) return;

//     /* 临时硬编码二维码，不依赖网络 */
//     if (s_lbl_status) lv_label_set_text(s_lbl_status, "Test QR code (hardcoded)");
//     // 生成一个固定内容的二维码，比如百度
//     lv_qrcode_update(s_qr_code_obj, "https://www.baidu.com", strlen("https://www.baidu.com"));
//     if (s_lbl_token) lv_label_set_text(s_lbl_token, "Hardcoded: baidu.com");

//     /* 不启动轮询定时器，避免 HTTP 调用 */
// }