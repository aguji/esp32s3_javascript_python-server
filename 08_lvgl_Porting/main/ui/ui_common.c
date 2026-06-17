/*
 * 共享 UI 实现：白底、深色字、绿底白字按钮，适配 800x480
 */
#include "ui_common.h"
#include "esp_log.h"

static const char *TAG = "ui_common";

/* 语言标志：false=英文（默认），true=中文 */
bool g_use_chinese = false;

const char *ui_text(const char *cn, const char *en)
{
    return g_use_chinese ? cn : en;
}

void ui_toggle_language(void)
{
    g_use_chinese = !g_use_chinese;
    ESP_LOGI(TAG, "Language switched to: %s", g_use_chinese ? "Chinese" : "English");
}

lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, lv_color_t color,
                          lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_align(label, align, x_ofs, y_ofs);
    return label;
}

lv_obj_t *ui_create_btn(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                        const char *text, lv_event_cb_t click_cb,
                        lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    /* 绿色实心按钮：不透明底 + 细边框，确保在白底上清晰 */
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_BTN), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_BTN_BORDER), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 4, 0);
    /* 按下态 */
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_BTN_PRESS), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_BTN_PRESS), LV_STATE_PRESSED);
    lv_obj_align(btn, align, x_ofs, y_ofs);
    if (click_cb != NULL) {
        lv_obj_add_event_cb(btn, click_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, FONT_CN, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_BTN_TEXT), 0);
    lv_obj_center(lbl);
    return btn;
}

lv_obj_t *ui_create_btn_bilingual(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                                  const char *cn_text, const char *en_text,
                                  lv_event_cb_t click_cb,
                                  lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    return ui_create_btn(parent, w, h, ui_text(cn_text, en_text), click_cb, align, x_ofs, y_ofs);
}
