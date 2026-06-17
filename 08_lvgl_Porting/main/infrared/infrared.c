/*
 * 红外传感（E18）实现：GPIO6 轮询，防抖后回调并停止
 * E18 说明书：有遮挡输出低电平 LED 亮，无遮挡输出高电平 LED 灭
 */
#include "infrared.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>

/* E18 DOUT 接 GPIO6。说明书：有遮挡=低电平 LED 亮，无遮挡=高电平 LED 灭；示例 if(DOUT==0) 为检测到 */
#define INFRARED_GPIO        GPIO_NUM_6
#define INFRARED_ACTIVE_HIGH 0

#define DEBOUNCE_MS      80
#define POLL_INTERVAL_MS 20
#define STABLE_COUNT     (DEBOUNCE_MS / POLL_INTERVAL_MS)
#define LOG_LEVEL_EVERY  50

static const char *TAG = "infrared";
static TaskHandle_t s_task = NULL;
/** 最近一次检测到遮挡并调用了回调（用于屏幕显示，在 infrared_start 时清除） */
static volatile int s_callback_triggered = 0;

static void infrared_task(void *arg)
{
    infrared_detected_fn on_detected = (infrared_detected_fn)arg;
    int count = 0;
    int loop = 0;

    ESP_LOGI(TAG, "task started, polling GPIO%d (active=%s)", (int)INFRARED_GPIO,
             INFRARED_ACTIVE_HIGH ? "high" : "low");

    while (1) {
        int level = gpio_get_level(INFRARED_GPIO);
#if INFRARED_ACTIVE_HIGH
        int active = (level == 1);
#else
        int active = (level == 0);
#endif
        if ((++loop % LOG_LEVEL_EVERY) == 0) {
            ESP_LOGI(TAG, "poll level=%d active=%d", level, active);
        }
        if (active) {
            count++;
            if (count >= STABLE_COUNT) {
                ESP_LOGI(TAG, ">>> INFRARED DETECTED! level=%d, calling callback", level);
                s_callback_triggered = 1;
                if (on_detected) on_detected();
                s_task = NULL;
                vTaskDelete(NULL);
                return;
            }
        } else {
            count = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

void infrared_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << INFRARED_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed %d", err);
        return;
    }
    int level = gpio_get_level(INFRARED_GPIO);
    ESP_LOGI(TAG, "E18 GPIO %d inited, level=%d (0=blocked/1=open when active_low)", (int)INFRARED_GPIO, level);
}

void infrared_start(infrared_detected_fn on_detected)
{
    if (s_task != NULL) {
        ESP_LOGW(TAG, "infrared_start: task already running, skip");
        return;
    }
    if (!on_detected) return;

    s_callback_triggered = 0;
    ESP_LOGI(TAG, "infrared_start: creating task, callback=%p", (void *)on_detected);
    BaseType_t ok = xTaskCreate(infrared_task, "infrared", 2048, (void *)on_detected, 4, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate infrared failed");
    }
}

void infrared_stop(void)
{
    if (s_task == NULL) return;
    TaskHandle_t t = s_task;
    s_task = NULL;
    vTaskDelete(t);
}

int infrared_is_active(void)
{
    return s_task != NULL ? 1 : 0;
}

int infrared_is_detected(void)
{
    int level = gpio_get_level(INFRARED_GPIO);
#if INFRARED_ACTIVE_HIGH
    return (level == 1) ? 1 : 0;
#else
    return (level == 0) ? 1 : 0;  /* E18 有遮挡=低电平 */
#endif
}

int infrared_selfcheck(char *err_msg, size_t err_len)
{
    if (err_msg && err_len) err_msg[0] = '\0';

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << INFRARED_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        if (err_msg && err_len) snprintf(err_msg, err_len, "GPIO%d init fail", INFRARED_GPIO);
        return -1;
    }
    (void)gpio_get_level(INFRARED_GPIO);
    return 0;
}

void infrared_get_status_str(char *buf, size_t len)
{
    if (!buf || len == 0) return;
    int level = gpio_get_level(INFRARED_GPIO);
    int task_on = (s_task != NULL) ? 1 : 0;
    int trig = s_callback_triggered ? 1 : 0;
    if (trig)
        snprintf(buf, len, "L=%d T=%d TRIG", level, task_on);
    else
        snprintf(buf, len, "L=%d T=%d", level, task_on);
}
