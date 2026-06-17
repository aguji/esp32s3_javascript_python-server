/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_log.h"
#include "wave_rgb_lcd/waveshare_esp32_s3_rgb_lcd_init.h"
#include "lvgl_port/lvgl_port.h"
#include "ui/library_ui.h"
#include "ui/ui_qr_login.h"
#include "wifi/wifi_config.h"
#include "http_client/http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

// ============================================
// HTTP 连接测试任务 (用于验证服务器通信；与扫码登录共用 http_client，勿同时启用)
// ============================================
__attribute__((unused)) static void http_test_task(void *pvParameters)
{
    char *server_ip = (char *)pvParameters;
    char token[64];

    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    vTaskDelay(pdMS_TO_TICKS(3000)); // 等待WiFi稳定

    // 初始化HTTP客户端，填入你的Flask服务器IP和端口
    // 例如: "192.168.1.100", 5000
    http_client_test_init(server_ip, 5000);

    ESP_LOGI(TAG, "Starting HTTP connection test...");
    ESP_LOGI(TAG, "Server: %s:5000", server_ip);

    // 测试Ping
    int ping_status = http_client_test_ping();
    if (ping_status > 0) {
        ESP_LOGI(TAG, "✓ Server ping successful (HTTP %d)", ping_status);
    } else {
        ESP_LOGE(TAG, "✗ Server ping failed!");
    }

    // 请求Token测试
    esp_err_t err = http_client_request_token(token, sizeof(token));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✓ Token received: %s", token);

        // 检查Token状态
        char status[32] = {0};
        err = http_client_check_status(token, status, sizeof(status));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✓ Token status: %s", status);
        } else {
            ESP_LOGE(TAG, "✗ Failed to check token status");
        }
    } else {
        ESP_LOGE(TAG, "✗ Failed to request token");
    }

    ESP_LOGI(TAG, "HTTP test complete!");
    vTaskDelete(NULL);
}

void app_main()
{
    wifi_config_init();  /* NVS + WiFi STA; auto-connect from saved config */
    waveshare_esp32_s3_rgb_lcd_init(); // Initialize the Waveshare ESP32-S3 RGB LCD

    ESP_LOGI(TAG, "Display library management UI");
    if (lvgl_port_lock(-1)) {
        library_ui_create();
        //ui_qrlogin_enter();// 进入扫码登录界面（测试用）
        lvgl_port_unlock();
    }

    // ========================================
    // 启动HTTP测试任务
    // 重要: 请修改为你的Flask服务器实际IP地址!
    // ========================================
    // 方式1: 使用统一配置（从 http_client.h）
    // xTaskCreate(http_test_task, "http_test", 4096, HTTP_CLIENT_SERVER_IP, 3, NULL);

    // 方式2: 从WiFi连接获取网关IP (假设服务器在同一网络)
    // 可以通过这种方式自动获取本地网络的服务器IP
    // 注意: 这只是一个示例，你需要根据实际情况修改
    /* 勿与扫码登录并行：http_client 为全局单例，后台测试任务会改 s_server_url / token，导致轮询死锁、第二次 HTTP start failed */
    // xTaskCreatePinnedToCore(http_test_task, "http_test", 16384, test_ip, 3, NULL, 1);
}
