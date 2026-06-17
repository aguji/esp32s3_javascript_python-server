#ifndef UI_MAP_H
#define UI_MAP_H

#include "lvgl.h"

/**
 * @brief 创建楼层选择界面和楼层详情界面
 *
 * @param out_scr_map 输出楼层选择界面指针
 * @param out_scr1~4 输出各楼层详情界面指针
 * @param go_to_main 返回主菜单回调
 * @param go_to_map 返回楼层选择回调
 * @param go_to_map1~4 各楼层按钮回调
 */
void ui_map_create(lv_obj_t **out_scr_map,
                   lv_obj_t **out_scr1, lv_obj_t **out_scr2, lv_obj_t **out_scr3, lv_obj_t **out_scr4,
                   lv_event_cb_t go_to_main, lv_event_cb_t go_to_map,
                   lv_event_cb_t go_to_map1, lv_event_cb_t go_to_map2,
                   lv_event_cb_t go_to_map3, lv_event_cb_t go_to_map4);

/**
 * @brief 设置楼层图片
 *
 * @param floor_index 楼层索引 (0=1F, 1=2F, 2=3F, 3=4F)
 * @param img_dsc LVGL图片描述符指针
 *
 * @note 使用此函数前需要先准备好编译好的图片资源
 */
void ui_map_set_floor_image(int floor_index, const lv_img_dsc_t *img_dsc);

#endif
