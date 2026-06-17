#ifndef UI_SEARCH_H
#define UI_SEARCH_H

#include "lvgl.h"

void ui_search_create(lv_obj_t **out_scr_search,
                      lv_obj_t **out_scr_search_input,
                      lv_obj_t **out_scr_search_category,
                      lv_obj_t **out_kb_search,
                      lv_event_cb_t go_to_main,
                      lv_event_cb_t go_to_search_input,
                      lv_event_cb_t go_to_search_category,
                      lv_event_cb_t go_back_to_search);

void ui_search_go_input(void);

#endif
