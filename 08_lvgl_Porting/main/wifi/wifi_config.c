/*
 * WiFi config: NVS + STA connect/scan.
 */
#include "wifi_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wifi_config";

#define NVS_NAMESPACE   "wifi_cfg"
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASS    "pass"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_SCAN_DONE_BIT  BIT1
#define WIFI_FAIL_BIT       BIT2

static EventGroupHandle_t s_wifi_event_group;
static int s_ap_count;
static wifi_ap_record_t s_ap_list[WIFI_CONFIG_AP_LIST_MAX];
static bool s_scan_done_ok;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                break;
            case WIFI_EVENT_SCAN_DONE: {
                wifi_event_sta_scan_done_t *ev = (wifi_event_sta_scan_done_t *)event_data;
                s_scan_done_ok = (ev->status == 0);
                xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_config_load_from_nvs(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t h;
    ssid[0] = pass[0] = '\0';
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = ssid_len;
    if (nvs_get_str(h, NVS_KEY_SSID, ssid, &len) == ESP_OK) {
        len = pass_len;
        nvs_get_str(h, NVS_KEY_PASS, pass, &len);
    }
    nvs_close(h);
}

void wifi_config_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
    s_ap_count = 0;
    s_scan_done_ok = false;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    char ssid[WIFI_CONFIG_SSID_MAX], pass[WIFI_CONFIG_PASS_MAX];
    wifi_config_load_from_nvs(ssid, sizeof(ssid), pass, sizeof(pass));
    if (ssid[0] != '\0') {
        wifi_config_t wcfg = { 0 };
        strncpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid) - 1);
        strncpy((char *)wcfg.sta.password, pass, sizeof(wcfg.sta.password) - 1);
        esp_wifi_set_config(WIFI_IF_STA, &wcfg);
        esp_wifi_connect();
    }
}

bool wifi_config_get_current_ssid(char *buf, size_t len)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        if (len) buf[0] = '\0';
        return false;
    }
    size_t n = sizeof(ap.ssid);
    if (n >= len) n = len - 1;
    memcpy(buf, ap.ssid, n);
    buf[n] = '\0';
    return true;
}

bool wifi_config_scan_start(void)
{
    xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
    s_scan_done_ok = false;
    s_ap_count = 0;
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 100, .max = 300 } },
    };
    if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) return false;
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WIFI_SCAN_DONE_BIT) || !s_scan_done_ok) return false;
    uint16_t count = WIFI_CONFIG_AP_LIST_MAX;
    esp_wifi_scan_get_ap_records(&count, s_ap_list);
    s_ap_count = (int)count;
    return true;
}

void wifi_config_scan_get_results(wifi_config_ap_t *list, size_t max_count, size_t *out_count)
{
    size_t n = (max_count < (size_t)s_ap_count) ? max_count : (size_t)s_ap_count;
    *out_count = n;
    for (size_t i = 0; i < n; i++) {
        size_t copy_len = sizeof(s_ap_list[i].ssid);
        if (copy_len >= WIFI_CONFIG_SSID_MAX) copy_len = WIFI_CONFIG_SSID_MAX - 1;
        memcpy(list[i].ssid, s_ap_list[i].ssid, copy_len);
        list[i].ssid[copy_len] = '\0';
        list[i].rssi = s_ap_list[i].rssi;
    }
}

bool wifi_config_connect(const char *ssid, const char *pass)
{
    wifi_config_t wcfg = { 0 };
    strncpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid) - 1);
    if (pass) strncpy((char *)wcfg.sta.password, pass, sizeof(wcfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    esp_wifi_connect();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

void wifi_config_save(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, NVS_KEY_SSID, ssid);
    nvs_set_str(h, NVS_KEY_PASS, pass ? pass : "");
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi config saved");
}
