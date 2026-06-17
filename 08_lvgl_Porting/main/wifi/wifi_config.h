/*
 * WiFi configuration: NVS storage + STA connect/scan.
 * SSID and password saved to NVS; on boot auto-connect from NVS.
 */
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define WIFI_CONFIG_SSID_MAX  32
#define WIFI_CONFIG_PASS_MAX  64
#define WIFI_CONFIG_AP_LIST_MAX  32

typedef struct {
    char ssid[WIFI_CONFIG_SSID_MAX];
    int8_t rssi;
} wifi_config_ap_t;

/** Call once at startup: init NVS, WiFi STA, load from NVS and connect if saved */
void wifi_config_init(void);

/** Get current connected AP SSID; return false if not connected */
bool wifi_config_get_current_ssid(char *buf, size_t len);

/** Start scan; returns when scan done or timeout (e.g. 10s). Returns true if scan completed. */
bool wifi_config_scan_start(void);

/** Get scan results (call after wifi_config_scan_start). *out_count set to number of APs. */
void wifi_config_scan_get_results(wifi_config_ap_t *list, size_t max_count, size_t *out_count);

/** Connect to SSID with password. Blocks until connected or failed (timeout ~15s). Returns true if connected. */
bool wifi_config_connect(const char *ssid, const char *pass);

/** Save SSID and password to NVS (so next boot will auto-connect) */
void wifi_config_save(const char *ssid, const char *pass);

#endif
