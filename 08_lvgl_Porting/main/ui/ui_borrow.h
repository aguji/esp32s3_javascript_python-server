/*
 * 借/还书界面模块
 * 注意：本文件须 UTF-8 编码
 */
#ifndef UI_BORROW_H
#define UI_BORROW_H

#include "lvgl.h"

/**
 * 借/还书模块初始化（注册 RFID 回调）
 */
void ui_borrow_init(void);

/**
 * 创建借/还书相关界面
 * @back_cb 返回主菜单的回调函数
 */
void ui_borrow_create(lv_event_cb_t back_cb);

/**
 * 获取借/还选择界面指针
 */
lv_obj_t *ui_borrow_get_selection_screen(void);

/**
 * 跳转到借/还选择界面
 */
void ui_borrow_go_to_selection(void);

/**
 * 处理 RFID 扫描到的图书标签
 * @uid_hex RFID 标签 UID（十六进制字符串）
 * @is_return 1=还书，0=借书
 */
void ui_borrow_on_book(const char *uid_hex, int is_return);

/**
 * 刷新主菜单 WiFi 标签（供定时器调用）
 */
void ui_borrow_refresh_mainmenu_wifi(void);

#endif /* UI_BORROW_H */
