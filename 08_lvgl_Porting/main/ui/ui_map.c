/*
 * 图书馆楼层选择 + 各楼层图片显示（800x480）
 * 
 * 图片使用方法：
 * 1. 准备 1F.png, 2F.png, 3F.png, 4F.png 图片（建议尺寸 <= 800x480）
 * 2. 使用 LVGL 在线转换工具 (https://lvgl.io/tools/imageconverter) 
 *    将每张图片转换为 C array，选择:
 *    - Color format: True color with Alpha (LVGL 8)
 *    - Output format: C array
 * 3. 在下方 extern 声明处取消注释，填入转换后的图片变量名
 * 4. 调用 ui_map_set_floor_image() 设置图片
 * 5. 如果没有图片，程序会自动回退到纯文字显示
 */

#include "ui_map.h"
#include "ui_common.h"
#include <stdbool.h>

/* 楼层图数据在 ui_floor_map_assets.c（可用 LVGL 工具替换为高清图） */
extern const lv_img_dsc_t img_1f;
extern const lv_img_dsc_t img_2f;
extern const lv_img_dsc_t img_3f;
extern const lv_img_dsc_t img_4f;

static const lv_img_dsc_t *floor_images[4] = { NULL };

/* 楼层图片是否可用 */
static bool is_floor_image_available(int floor_index)
{
    return (floor_index >= 0 && floor_index < 4 && floor_images[floor_index] != NULL);
}

void ui_map_set_floor_image(int floor_index, const lv_img_dsc_t *img_dsc)
{
    if (floor_index >= 0 && floor_index < 4) {
        floor_images[floor_index] = img_dsc;
    }
}

static lv_obj_t *create_floor_screen(const char *floor_name, int floor_index, lv_event_cb_t go_back_cb)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* 返回按钮 */
    ui_create_btn(scr, 160, 52, "Back", go_back_cb, LV_ALIGN_TOP_LEFT, 32, 24);

    /* 标题显示楼层名称 */
    ui_create_label(scr, floor_name, FONT_TITLE, lv_color_hex(UI_COLOR_TEXT),
                    LV_ALIGN_TOP_MID, 0, 24);

    /* 尝试显示楼层图片 */
    if (is_floor_image_available(floor_index)) {
        lv_obj_t *img = lv_img_create(scr);
        lv_img_set_src(img, floor_images[floor_index]);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 20);
        /* 内置图为 200x120，放大以便预览；换成大图后可改为 256 或删除本行 */
        lv_img_set_zoom(img, 768);
        /* 确保返回按钮在最上层 */
        lv_obj_t *back_btn = lv_obj_get_child(scr, 0);
        if (back_btn) {
            lv_obj_move_foreground(back_btn);
        }
    } else {
        /* 没有图片时显示占位文字和提示 */
        ui_create_label(scr, "[Floor map image]", FONT_CN, lv_color_hex(UI_COLOR_TEXT_MUTE),
                        LV_ALIGN_CENTER, 0, -60);
        ui_create_label(scr, "Image not loaded", FONT_CN, lv_color_hex(0x888888),
                        LV_ALIGN_CENTER, 0, -20);
        ui_create_label(scr, "Add floor images to display", FONT_CN, lv_color_hex(0xaaaaaa),
                        LV_ALIGN_CENTER, 0, 20);
    }

    return scr;
}

void ui_map_create(lv_obj_t **out_scr_map,
                   lv_obj_t **out_scr1, lv_obj_t **out_scr2, lv_obj_t **out_scr3, lv_obj_t **out_scr4,
                   lv_event_cb_t go_to_main, lv_event_cb_t go_to_map,
                   lv_event_cb_t go_to_map1, lv_event_cb_t go_to_map2,
                   lv_event_cb_t go_to_map3, lv_event_cb_t go_to_map4)
{
    static bool floor_src_registered;
    if (!floor_src_registered) {
        ui_map_set_floor_image(0, &img_1f);
        ui_map_set_floor_image(1, &img_2f);
        ui_map_set_floor_image(2, &img_3f);
        ui_map_set_floor_image(3, &img_4f);
        floor_src_registered = true;
    }

    lv_obj_t **out_floors[] = { out_scr1, out_scr2, out_scr3, out_scr4 };
    const char *floor_names[] = { "1F - Floor 1", "2F - Floor 2", "3F - Floor 3", "4F - Floor 4" };
    const char *btn_texts[] = { "1F", "2F", "3F", "4F" };

    lv_obj_t *scr_map = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_map, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_map, 0, 0);
    lv_obj_set_style_pad_all(scr_map, 0, 0);

    ui_create_label(scr_map, "Select floor", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT),
                    LV_ALIGN_TOP_MID, 0, 32);
    ui_create_btn(scr_map, 160, 52, "Back", go_to_main, LV_ALIGN_TOP_LEFT, 32, 24);

    int cy = 120;
    ui_create_btn(scr_map, 190, 56, btn_texts[0], go_to_map1, LV_ALIGN_TOP_MID, -300, cy);
    ui_create_btn(scr_map, 190, 56, btn_texts[1], go_to_map2, LV_ALIGN_TOP_MID, -100, cy);
    ui_create_btn(scr_map, 190, 56, btn_texts[2], go_to_map3, LV_ALIGN_TOP_MID, 100, cy);
    ui_create_btn(scr_map, 190, 56, btn_texts[3], go_to_map4, LV_ALIGN_TOP_MID, 300, cy);

    if (out_scr_map) *out_scr_map = scr_map;
    for (size_t i = 0; i < 4; i++) {
        lv_obj_t *scr = create_floor_screen(floor_names[i], (int)i, go_to_map);
        if (out_floors[i]) *out_floors[i] = scr;
    }
}
