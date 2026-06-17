/*
 * WiFi 设置界面模块
 * 注意：本文件须 UTF-8 编码
 */
#ifndef UI_WIFI_H
#define UI_WIFI_H

#include "lvgl.h"

/**
 * WiFi 界面创建完成后可调用的刷新函数
 */
void ui_wifi_refresh_status(void);

/**
 * 创建 WiFi 设置界面
 * @back_cb 返回按钮点击回调，可为 NULL
 */
void ui_wifi_create(lv_event_cb_t back_cb);

/**
 * 获取 WiFi 设置界面指针
 */
lv_obj_t *ui_wifi_get_screen(void);

#endif /* UI_WIFI_H */
