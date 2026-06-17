/*
 * 图书馆管理系统 - 共享 UI 辅助（800x480 实心按钮）
 * 注意：本文件须 UTF-8 编码
 */
#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl.h"

/* 屏幕尺寸 800x480 */
#define UI_SCREEN_W   800
#define UI_SCREEN_H   480

/* 统一配色：白底、深色字、绿色按钮 */
#define UI_COLOR_BG         0xFFFFFF
#define UI_COLOR_TEXT       0x1a1a1a
#define UI_COLOR_TEXT_MUTE  0x475569
#define UI_COLOR_BTN        0x22c55e
#define UI_COLOR_BTN_PRESS  0x15803d
#define UI_COLOR_BTN_BORDER 0x16a34a
#define UI_COLOR_BTN_TEXT   0xFFFFFF

#define FONT_CN       (&lv_font_montserrat_16)
#define FONT_TITLE    (&lv_font_montserrat_20)

/* 语言切换标志：true=中文，false=英文 */
extern bool g_use_chinese;

/* 获取当前语言的文本（中文时返回中文，英文时返回英文） */
const char *ui_text(const char *cn, const char *en);

/* 切换语言 */
void ui_toggle_language(void);

/** 创建标签 */
lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, lv_color_t color,
                          lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);

/** 创建实心按钮（填充背景、白字、按下变深），click_cb 可为 NULL */
lv_obj_t *ui_create_btn(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                        const char *text, lv_event_cb_t click_cb,
                        lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);

/** 创建双语按钮（中英切换） */
lv_obj_t *ui_create_btn_bilingual(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                                  const char *cn_text, const char *en_text,
                                  lv_event_cb_t click_cb,
                                  lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);

#endif /* UI_COMMON_H */
