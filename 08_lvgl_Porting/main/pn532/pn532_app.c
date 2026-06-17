#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "pn532.h"
#include "pn532_driver_hsu.h"
#include "pn532_app.h"

// HSU(UART) 接线（适配 7 寸 RGB 板，避开 LCD/触摸用脚）：
// HSU mode needs only RX/TX pins. RESET pin will be used if valid.
#define HSU_HOST_RX   GPIO_NUM_44    // 接 PN532 TXD
#define HSU_HOST_TX   GPIO_NUM_43    // 接 PN532 RXD
#define RESET_PIN     GPIO_NUM_NC
#define IRQ_PIN       GPIO_NUM_NC
#define HSU_UART_PORT UART_NUM_1
#define HSU_BAUD_RATE 921600

static const char *TAG = "pn532_hsu_app";
static volatile bool s_stop_read = false;

static void notify_status(const pn532_app_callbacks_t *cbs, const char *msg)
{
    if (cbs && cbs->on_status && msg) {
        cbs->on_status(msg);
    }
}

static void notify_card(const pn532_app_callbacks_t *cbs, const uint8_t *uid, uint8_t uid_length)
{
    if (cbs && cbs->on_card && uid && uid_length > 0) {
        cbs->on_card(uid, uid_length);
    }
}

void pn532_app_run(const pn532_app_callbacks_t *callbacks)
{
    pn532_io_t pn532_io;
    esp_err_t err;

    s_stop_read = false;

    // 打开调试日志，方便定位问题
    esp_log_level_set("PN532", ESP_LOG_DEBUG);
    esp_log_level_set("pn532_driver", ESP_LOG_DEBUG);
    esp_log_level_set("pn532_driver_hsu", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "Init PN532 over HSU (UART)...");
    notify_status(callbacks, "Initializing RFID reader...");

    ESP_ERROR_CHECK(pn532_new_driver_hsu(
        HSU_HOST_RX,
        HSU_HOST_TX,
        RESET_PIN,
        IRQ_PIN,
        HSU_UART_PORT,
        HSU_BAUD_RATE,
        &pn532_io));

    // 初始化 PN532 芯片，最多重试 5 次
    #define PN532_INIT_MAX_RETRY 5
    int retry_count = 0;
    do {
        err = pn532_init(&pn532_io);
        if (err != ESP_OK) {
            retry_count++;
            ESP_LOGW(TAG, "PN532 init failed, retry %d/%d...", retry_count, PN532_INIT_MAX_RETRY);
            notify_status(callbacks, "PN532 init failed, retry...");
            pn532_release(&pn532_io);
            if (retry_count >= PN532_INIT_MAX_RETRY) {
                ESP_LOGE(TAG, "PN532 init failed after %d retries, giving up", PN532_INIT_MAX_RETRY);
                notify_status(callbacks, "RFID init failed!");
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while (err != ESP_OK);

    // 读取固件版本，确认芯片工作正常，最多重试 5 次
    uint32_t version_data = 0;
    retry_count = 0;
    do {
        err = pn532_get_firmware_version(&pn532_io, &version_data);
        if (err != ESP_OK) {
            retry_count++;
            ESP_LOGW(TAG, "Didn't find PN53x board, reset and retry %d/%d...", retry_count, PN532_INIT_MAX_RETRY);
            notify_status(callbacks, "Didn't find PN53x, resetting...");
            pn532_reset(&pn532_io);
            if (retry_count >= PN532_INIT_MAX_RETRY) {
                ESP_LOGE(TAG, "PN532 firmware check failed after %d retries, giving up", PN532_INIT_MAX_RETRY);
                notify_status(callbacks, "RFID firmware check failed!");
                pn532_release(&pn532_io);
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while (err != ESP_OK);

    ESP_LOGI(TAG, "Found chip PN5%x", (unsigned int)(version_data >> 24) & 0xFF);
    ESP_LOGI(TAG, "Firmware ver. %d.%d",
             (int)(version_data >> 16) & 0xFF,
             (int)(version_data >> 8) & 0xFF);

    ESP_LOGI(TAG, "Waiting for an ISO14443A Card ...");
    notify_status(callbacks, "RFID ready, place your card...");

    while (!s_stop_read) {
        uint8_t uid[10] = {0};
        uint8_t uid_length = 0;

        // 等待卡片靠近，超时时间设置为 1000ms（1 秒）
        err = pn532_read_passive_target_id(
            &pn532_io,
            PN532_BRTY_ISO14443A_106KBPS,
            uid,
            &uid_length,
            1000);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found an ISO14443A card, UID len = %d", uid_length);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, uid, uid_length, ESP_LOG_INFO);
            notify_card(callbacks, uid, uid_length);
            notify_status(callbacks, "Card detected");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    pn532_release(&pn532_io);
    notify_status(callbacks, "RFID reader stopped");
}

void pn532_app_request_stop(void)
{
    s_stop_read = true;
}

esp_err_t pn532_app_quick_test(char *err_msg, size_t err_len)
{
    pn532_io_t pn532_io;
    esp_err_t err;

    if (err_msg && err_len) err_msg[0] = '\0';

    err = pn532_new_driver_hsu(HSU_HOST_RX, HSU_HOST_TX, RESET_PIN, IRQ_PIN,
                               HSU_UART_PORT, HSU_BAUD_RATE, &pn532_io);
    if (err != ESP_OK) {
        if (err_msg && err_len) snprintf(err_msg, err_len, "UART init fail");
        return err;
    }

    err = pn532_init(&pn532_io);
    if (err != ESP_OK) {
        pn532_release(&pn532_io);
        if (err_msg && err_len) snprintf(err_msg, err_len, "PN532 init fail");
        return err;
    }

    uint32_t version_data = 0;
    err = pn532_get_firmware_version(&pn532_io, &version_data);
    pn532_release(&pn532_io);
    if (err != ESP_OK) {
        if (err_msg && err_len) snprintf(err_msg, err_len, "PN532 no response");
        return err;
    }
    return ESP_OK;
}

