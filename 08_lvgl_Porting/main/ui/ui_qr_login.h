/*
 * 扫码登录界面 - 头文件
 */
#ifndef UI_QR_LOGIN_H
#define UI_QR_LOGIN_H

#include <lvgl.h>

/**
 * @brief 初始化扫码登录界面
 *        必须在 library_ui_create() 中调用
 */
void ui_qrlogin_init(void);

/**
 * @brief 获取扫码登录屏幕对象
 * @return 扫码登录屏幕指针
 */
lv_obj_t *ui_qrlogin_get_screen(void);

/**
 * @brief 进入扫码登录界面
 *        会加载屏幕并开始扫码流程
 */
void ui_qrlogin_enter(void);

/**
 * @brief 停止扫码登录（清理资源）
 *        用于返回时清理
 */
void ui_qrlogin_stop(void);

#endif // UI_QR_LOGIN_H
