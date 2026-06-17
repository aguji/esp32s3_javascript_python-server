/*
 * WiFi 设置界面模块
 * 注意：本文件须 UTF-8 编码
 */
#include "lvgl_port/lvgl.h"
#include "ui_common.h"
#include "ui_wifi.h"
#include "wifi/wifi_config.h"
#include "esp_log.h"

static const char *TAG_UI_WIFI = "ui_wifi";

/* WiFi 界面静态变量 */
static lv_obj_t *scr_wifi = NULL;
static lv_obj_t *lbl_wifi_current = NULL;
static lv_obj_t *cont_wifi_list = NULL;
static lv_obj_t *panel_wifi_connect = NULL;
static lv_obj_t *ta_wifi_pass = NULL;
static lv_obj_t *kb_wifi = NULL;
static lv_obj_t *lbl_wifi_msg = NULL;
static lv_obj_t *lbl_wifi_ssid = NULL;
static char s_selected_ssid[WIFI_CONFIG_SSID_MAX];
static wifi_config_ap_t s_last_scan[WIFI_CONFIG_AP_LIST_MAX];
static size_t s_last_scan_count;
static lv_event_cb_t s_back_cb = NULL;

/* 前向声明 */
static void refresh_wifi_current_label(void);
static void wifi_kb_hide(void);
static void wifi_ap_click_cb(lv_event_t *e);
static void wifi_connect_cb(lv_event_t *e);
static void wifi_connect_cancel_cb(lv_event_t *e);
static void wifi_ta_show_kb_cb(lv_event_t *e);
static void wifi_kb_cancel_cb(lv_event_t *e);
static void wifi_kb_ready_cb(lv_event_t *e);
static void wifi_kb_hide_btn_cb(lv_event_t *e);
static void wifi_scan_cb(lv_event_t *e);
static void wifi_back_cb(lv_event_t *e);

/* 刷新当前连接状态标签 */
static void refresh_wifi_current_label(void)
{
    if (!lbl_wifi_current) return;
    char buf[WIFI_CONFIG_SSID_MAX];
    if (wifi_config_get_current_ssid(buf, sizeof(buf)))
        lv_label_set_text_fmt(lbl_wifi_current, "Status: Connected to %s", buf);
    else
        lv_label_set_text(lbl_wifi_current, "Status: Not connected");
}

/* 刷新状态（外部调用） */
void ui_wifi_refresh_status(void)
{
    refresh_wifi_current_label();
}

/* 获取 WiFi 界面指针 */
lv_obj_t *ui_wifi_get_screen(void)
{
    return scr_wifi;
}

/* WiFi 扫描回调 */
static void wifi_scan_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!cont_wifi_list) return;
    if (lbl_wifi_current) lv_label_set_text(lbl_wifi_current, "Scanning...");
    lv_obj_clean(cont_wifi_list);
    if (!wifi_config_scan_start()) {
        refresh_wifi_current_label();
        return;
    }
    refresh_wifi_current_label();
    wifi_config_scan_get_results(s_last_scan, WIFI_CONFIG_AP_LIST_MAX, &s_last_scan_count);
    for (size_t i = 0; i < s_last_scan_count; i++) {
        lv_obj_t *btn = ui_create_btn(cont_wifi_list, 400, 44, s_last_scan[i].ssid, NULL, LV_ALIGN_TOP_LEFT, 0, (lv_coord_t)(i * 48));
        lv_obj_add_event_cb(btn, wifi_ap_click_cb, LV_EVENT_CLICKED, (void *)i);
    }
}

/* 隐藏键盘 */
static void wifi_kb_hide(void)
{
    if (kb_wifi) {
        lv_keyboard_set_textarea(kb_wifi, NULL);
        lv_obj_add_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 文本框聚焦显示键盘 */
static void wifi_ta_show_kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;
    if (kb_wifi && ta_wifi_pass) {
        lv_keyboard_set_textarea(kb_wifi, ta_wifi_pass);
        lv_obj_clear_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
    }
}

/* AP 点击回调 */
static void wifi_ap_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    if (idx < s_last_scan_count) {
        strncpy(s_selected_ssid, s_last_scan[idx].ssid, WIFI_CONFIG_SSID_MAX - 1);
        s_selected_ssid[WIFI_CONFIG_SSID_MAX - 1] = '\0';
    }
    if (ta_wifi_pass) lv_textarea_set_text(ta_wifi_pass, "");
    if (lbl_wifi_msg) lv_label_set_text(lbl_wifi_msg, "");
    if (lbl_wifi_ssid) lv_label_set_text_fmt(lbl_wifi_ssid, "SSID: %s", s_selected_ssid);
    wifi_kb_hide();
    if (panel_wifi_connect) {
        lv_obj_clear_flag(panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
    }
}

/* WiFi 连接回调 */
static void wifi_connect_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char *pass = ta_wifi_pass ? lv_textarea_get_text(ta_wifi_pass) : "";
    if (!pass) pass = "";
    if (wifi_config_connect(s_selected_ssid, pass)) {
        wifi_config_save(s_selected_ssid, pass);
        if (lbl_wifi_msg) lv_label_set_text(lbl_wifi_msg, "Connected, config saved.");
    } else {
        if (lbl_wifi_msg) lv_label_set_text(lbl_wifi_msg, "Connection failed.");
    }
    wifi_kb_hide();
    if (panel_wifi_connect) lv_obj_add_flag(panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
    refresh_wifi_current_label();
}

/* 连接取消回调 */
static void wifi_connect_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    wifi_kb_hide();
    if (panel_wifi_connect) lv_obj_add_flag(panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);
}

/* 键盘取消回调 */
static void wifi_kb_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CANCEL) return;
    wifi_kb_hide();
}

/* 键盘准备好回调 */
static void wifi_kb_ready_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_READY) return;
    wifi_kb_hide();
}

/* 隐藏键盘按钮回调 */
static void wifi_kb_hide_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    wifi_kb_hide();
}

/* 返回按钮回调 */
static void wifi_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_back_cb) {
        s_back_cb(e);
    }
}

/* 创建 WiFi 设置界面 */
void ui_wifi_create(lv_event_cb_t back_cb)
{
    s_back_cb = back_cb;

    scr_wifi = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_wifi, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_wifi, 0, 0);
    lv_obj_set_style_pad_all(scr_wifi, 16, 16);

    /* 标题 */
    ui_create_label(scr_wifi, "WiFi Settings", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 4);

    /* 连接状态 */
    lbl_wifi_current = ui_create_label(scr_wifi, "Status: (not connected)", FONT_CN,
                                       lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 0, 44);
    lv_obj_set_style_text_font(lbl_wifi_current, &lv_font_montserrat_16, 0);

    /* Scan 按钮 */
    ui_create_btn(scr_wifi, 100, 40, "Scan", wifi_scan_cb, LV_ALIGN_TOP_RIGHT, 0, 40);

    /* WiFi 列表 */
    cont_wifi_list = lv_obj_create(scr_wifi);
    lv_obj_set_size(cont_wifi_list, LV_PCT(100), 120);
    lv_obj_align(cont_wifi_list, LV_ALIGN_TOP_LEFT, 0, 92);
    lv_obj_set_style_bg_color(cont_wifi_list, lv_color_hex(0xf0f4f8), 0);
    lv_obj_set_style_border_width(cont_wifi_list, 1, 0);
    lv_obj_set_style_border_color(cont_wifi_list, lv_color_hex(0xcbd5e1), 0);
    lv_obj_set_style_pad_all(cont_wifi_list, 6, 0);
    lv_obj_set_scroll_dir(cont_wifi_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont_wifi_list, LV_SCROLLBAR_MODE_AUTO);

    /* 连接面板 */
    panel_wifi_connect = lv_obj_create(scr_wifi);
    lv_obj_set_size(panel_wifi_connect, LV_PCT(100), 140);
    lv_obj_align(panel_wifi_connect, LV_ALIGN_TOP_LEFT, 0, 220);
    lv_obj_set_style_bg_color(panel_wifi_connect, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(panel_wifi_connect, 1, 0);
    lv_obj_set_style_radius(panel_wifi_connect, 8, 0);
    lv_obj_set_style_pad_all(panel_wifi_connect, 10, 0);
    lv_obj_add_flag(panel_wifi_connect, LV_OBJ_FLAG_HIDDEN);

    lbl_wifi_ssid = ui_create_label(panel_wifi_connect, "SSID: --", FONT_CN,
                                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 0, 2);
    ui_create_label(panel_wifi_connect, "Password:", FONT_CN,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_LEFT, 0, 28);
    ta_wifi_pass = lv_textarea_create(panel_wifi_connect);
    lv_textarea_set_one_line(ta_wifi_pass, true);
    lv_textarea_set_max_length(ta_wifi_pass, WIFI_CONFIG_PASS_MAX - 1);
    lv_textarea_set_password_mode(ta_wifi_pass, true);
    lv_textarea_set_placeholder_text(ta_wifi_pass, "password");
    lv_obj_set_size(ta_wifi_pass, LV_PCT(100), 36);
    lv_obj_align(ta_wifi_pass, LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_add_event_cb(ta_wifi_pass, wifi_ta_show_kb_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_wifi_pass, wifi_ta_show_kb_cb, LV_EVENT_CLICKED, NULL);

    lbl_wifi_msg = ui_create_label(panel_wifi_connect, "", FONT_CN,
                                   lv_color_hex(0x64748b), LV_ALIGN_TOP_LEFT, 0, 88);

    ui_create_btn(panel_wifi_connect, 90, 38, "Connect", wifi_connect_cb, LV_ALIGN_TOP_RIGHT, 0, 48);
    ui_create_btn(panel_wifi_connect, 90, 38, "Cancel", wifi_connect_cancel_cb, LV_ALIGN_TOP_RIGHT, 96, 48);
    ui_create_btn(panel_wifi_connect, 72, 38, "Hide KB", wifi_kb_hide_btn_cb, LV_ALIGN_TOP_RIGHT, 0, 88);

    kb_wifi = lv_keyboard_create(scr_wifi);
    lv_obj_set_size(kb_wifi, LV_PCT(96), 160);
    lv_obj_align(kb_wifi, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_add_event_cb(kb_wifi, wifi_kb_cancel_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(kb_wifi, wifi_kb_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);

    ui_create_btn(scr_wifi, 140, 44, "Back", wifi_back_cb, LV_ALIGN_BOTTOM_MID, 0, -8);

    /* 初始化时刷新状态 */
    refresh_wifi_current_label();
}
