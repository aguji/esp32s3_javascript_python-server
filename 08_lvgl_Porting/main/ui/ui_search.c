/*
 * 图书检索界面（Keyword Search + Category Search）
 * Keyword：GET /search?keyword=...
 * Category：GET /search_by_category?category=（与 tb_book.category 精确匹配）
 */
#include "ui_search.h"
#include "ui_common.h"
#include "http_client/http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG_SEARCH = "ui_search";

#define SEARCH_TASK_STACK  (14 * 1024)
#define SEARCH_TASK_PRIO     5

/* 当前活动屏幕 */
static lv_obj_t *s_scr_search       = NULL;
static lv_obj_t *s_scr_input        = NULL;
static lv_obj_t *s_scr_category     = NULL;
static lv_obj_t *s_scr_cat_detail   = NULL;

/* Keyword 页 */
static lv_obj_t *s_ta_keyword        = NULL;
static lv_obj_t *s_kb_search         = NULL;
static lv_obj_t *s_lbl_search_status = NULL;
static lv_obj_t *s_cont_results      = NULL;

/* Category 详情页 */
static char      s_cat_detail_name[64]    = {0};
static lv_obj_t *s_lbl_catd_status        = NULL;
static lv_obj_t *s_cont_catd_results      = NULL;

static volatile bool s_search_busy;

static void back_to_search_cb(lv_event_t *e);
static void category_btn_cb(lv_event_t *e);

typedef struct {
    esp_err_t err;
    search_result_t result;
} search_ui_payload_t;

/* 将搜索结果画到指定 label + 滚动容器 */
static void apply_search_results_ui(search_ui_payload_t *p, lv_obj_t *lbl_status, lv_obj_t *cont_results)
{
    s_search_busy = false;

    if (!p) {
        return;
    }

    if (cont_results) {
        lv_obj_clean(cont_results);
    }

    if (!lbl_status || !cont_results) {
        free(p);
        return;
    }

    if (p->err != ESP_OK) {
        lv_label_set_text(lbl_status, "Network error. Check WiFi and server IP in http_client.h");
        free(p);
        return;
    }

    if (p->result.count <= 0) {
        lv_label_set_text(lbl_status, "No books found.");
        free(p);
        return;
    }

    lv_label_set_text_fmt(lbl_status, "Found %d book(s)", p->result.count);

    for (int i = 0; i < p->result.count; i++) {
        const http_client_book_t *b = &p->result.books[i];
        const char *st = (b->status == 0) ? "Available" : "Borrowed";

        lv_obj_t *lbl = lv_label_create(cont_results);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_text_font(lbl, FONT_CN, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT), 0);
        lv_obj_set_style_pad_bottom(lbl, 14, 0);

        lv_label_set_text_fmt(lbl,
                              "%d. %s\n"
                              "Author: %s\n"
                              "Location: %s\n"
                              "ISBN: %s  |  Status: %s",
                              i + 1,
                              b->title[0] ? b->title : "-",
                              b->author[0] ? b->author : "-",
                              b->location[0] ? b->location : "-",
                              b->isbn[0] ? b->isbn : "-",
                              st);
    }

    free(p);
}

static void search_apply_ui_cb(void *user_data)
{
    apply_search_results_ui((search_ui_payload_t *)user_data, s_lbl_search_status, s_cont_results);
}

static void category_apply_ui_cb(void *user_data)
{
    apply_search_results_ui((search_ui_payload_t *)user_data, s_lbl_catd_status, s_cont_catd_results);
}

static void search_worker_task(void *arg)
{
    char *keyword = (char *)arg;
    search_ui_payload_t *payload = (search_ui_payload_t *)calloc(1, sizeof(search_ui_payload_t));
    if (!payload) {
        ESP_LOGE(TAG_SEARCH, "search payload alloc failed");
        free(keyword);
        s_search_busy = false;
        vTaskDelete(NULL);
        return;
    }

    payload->err = http_client_search_books(keyword, &payload->result);
    free(keyword);

    if (lv_async_call(search_apply_ui_cb, payload) != LV_RES_OK) {
        ESP_LOGE(TAG_SEARCH, "lv_async_call failed");
        free(payload);
        s_search_busy = false;
    }

    vTaskDelete(NULL);
}

static void category_worker_task(void *arg)
{
    char *category = (char *)arg;
    search_ui_payload_t *payload = (search_ui_payload_t *)calloc(1, sizeof(search_ui_payload_t));
    if (!payload) {
        ESP_LOGE(TAG_SEARCH, "category payload alloc failed");
        free(category);
        s_search_busy = false;
        vTaskDelete(NULL);
        return;
    }

    payload->err = http_client_search_books_by_category(category, &payload->result);
    free(category);

    if (lv_async_call(category_apply_ui_cb, payload) != LV_RES_OK) {
        ESP_LOGE(TAG_SEARCH, "lv_async_call category failed");
        free(payload);
        s_search_busy = false;
    }

    vTaskDelete(NULL);
}

/* 点击分类按钮：记录分类名 → 创建/更新详情页 → 发起搜索 */
static void category_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char *db_cat = (const char *)lv_event_get_user_data(e);
    if (!db_cat) return;

    /* 记录分类名，供详情页创建时使用 */
    strncpy(s_cat_detail_name, db_cat, sizeof(s_cat_detail_name) - 1);
    s_cat_detail_name[sizeof(s_cat_detail_name) - 1] = '\0';

    /* 如果详情页已存在则先销毁重建 */
    if (s_scr_cat_detail) {
        lv_obj_del(s_scr_cat_detail);
        s_scr_cat_detail = NULL;
    }

    /* ---- 详情页 ---- */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    s_scr_cat_detail = scr;

    /* 左上 Back，右上 搜索该分类 */
    ui_create_btn(scr, 140, 44, "Back", back_to_search_cb, LV_ALIGN_TOP_LEFT, 32, 16);

    /* 分类名标题 */
    ui_create_label(scr, s_cat_detail_name, FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 28);

    /* 状态行 */
    s_lbl_catd_status = lv_label_create(scr);
    lv_label_set_long_mode(s_lbl_catd_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_catd_status, 720);
    lv_obj_set_style_text_font(s_lbl_catd_status, FONT_CN, 0);
    lv_obj_set_style_text_color(s_lbl_catd_status, lv_color_hex(UI_COLOR_TEXT_MUTE), 0);
    lv_label_set_text(s_lbl_catd_status, "Searching...");
    lv_obj_align(s_lbl_catd_status, LV_ALIGN_TOP_MID, 0, 84);

    /* 结果列表（可滚动，突出 Location） */
    s_cont_catd_results = lv_obj_create(scr);
    lv_obj_set_size(s_cont_catd_results, 736, 320);
    lv_obj_align(s_cont_catd_results, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_set_style_bg_color(s_cont_catd_results, lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_border_color(s_cont_catd_results, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(s_cont_catd_results, 1, 0);
    lv_obj_set_style_radius(s_cont_catd_results, 8, 0);
    lv_obj_set_style_pad_all(s_cont_catd_results, 10, 0);
    lv_obj_set_scroll_dir(s_cont_catd_results, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_cont_catd_results, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(s_cont_catd_results, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_cont_catd_results, 4, 0);

    lv_scr_load(scr);

    /* 发起网络搜索 */
    size_t n = strlen(db_cat);
    char *copy = (char *)malloc(n + 1);
    if (!copy) {
        lv_label_set_text(s_lbl_catd_status, "Out of memory.");
        return;
    }
    memcpy(copy, db_cat, n + 1);

    s_search_busy = true;
    if (xTaskCreate(category_worker_task, "cat_search", SEARCH_TASK_STACK, copy,
                    SEARCH_TASK_PRIO, NULL) != pdPASS) {
        s_search_busy = false;
        free(copy);
        lv_label_set_text(s_lbl_catd_status, "Cannot start search task.");
        ESP_LOGE(TAG_SEARCH, "xTaskCreate cat_search failed");
    }
}

/* 搜索入口页回调 */
static void goto_keyword_search_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_scr_input) lv_scr_load(s_scr_input);
}

static void goto_category_search_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_scr_category) lv_scr_load(s_scr_category);
}

/* 返回搜索入口页 */
static void back_to_search_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_scr_search) lv_scr_load(s_scr_search);
}

/* 输入框：点击唤起键盘 */
static void ta_input_focus_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;
    lv_obj_t *ta = lv_event_get_target(e);
    if (s_kb_search && ta) {
        lv_keyboard_set_textarea(s_kb_search, ta);
        lv_obj_clear_flag(s_kb_search, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 键盘关闭按钮 */
static void kb_close_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CANCEL || lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (s_kb_search) {
            lv_keyboard_set_textarea(s_kb_search, NULL);
            lv_obj_add_flag(s_kb_search, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* 搜索按钮回调（右上角） */
static void btn_search_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!s_ta_keyword || !s_lbl_search_status || !s_cont_results) return;

    if (s_search_busy) {
        lv_label_set_text(s_lbl_search_status, "Please wait, searching...");
        return;
    }

    const char *keyword = lv_textarea_get_text(s_ta_keyword);
    if (!keyword) keyword = "";

    while (*keyword == ' ' || *keyword == '\t') keyword++;
    size_t kw_len = strlen(keyword);
    while (kw_len > 0 && (keyword[kw_len - 1] == ' ' || keyword[kw_len - 1] == '\t')) {
        kw_len--;
    }
    if (kw_len == 0) {
        lv_obj_clean(s_cont_results);
        lv_label_set_text(s_lbl_search_status, "Please enter title, author or ISBN.");
        if (s_kb_search) {
            lv_keyboard_set_textarea(s_kb_search, NULL);
            lv_obj_add_flag(s_kb_search, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (s_kb_search) {
        lv_keyboard_set_textarea(s_kb_search, NULL);
        lv_obj_add_flag(s_kb_search, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clean(s_cont_results);
    lv_label_set_text(s_lbl_search_status, "Searching...");

    char *kw_copy = (char *)malloc(kw_len + 1);
    if (!kw_copy) {
        lv_label_set_text(s_lbl_search_status, "Out of memory.");
        return;
    }
    memcpy(kw_copy, keyword, kw_len);
    kw_copy[kw_len] = '\0';

    s_search_busy = true;
    BaseType_t ok = xTaskCreate(search_worker_task, "book_search", SEARCH_TASK_STACK, kw_copy,
                                SEARCH_TASK_PRIO, NULL);
    if (ok != pdPASS) {
        s_search_busy = false;
        free(kw_copy);
        lv_label_set_text(s_lbl_search_status, "Cannot start search task.");
        ESP_LOGE(TAG_SEARCH, "xTaskCreate book_search failed");
    }
}

/* ========== 公开函数 ========== */

void ui_search_go_input(void)
{
    if (s_scr_input) {
        lv_scr_load_anim(s_scr_input, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

void ui_search_create(lv_obj_t **out_scr_search,
                      lv_obj_t **out_scr_search_input,
                      lv_obj_t **out_scr_search_category,
                      lv_obj_t **out_kb_search,
                      lv_event_cb_t go_to_main,
                      lv_event_cb_t go_to_search_input,
                      lv_event_cb_t go_to_search_category,
                      lv_event_cb_t go_back_to_search)
{
    (void)go_to_main;
    (void)go_to_search_input;
    (void)go_to_search_category;
    (void)go_back_to_search;
    (void)out_kb_search;

    /* ---- 搜索入口页 ---- */
    lv_obj_t *scr_search = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_search, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_search, 0, 0);
    lv_obj_set_style_pad_all(scr_search, 0, 0);

    ui_create_label(scr_search, "Book Search", FONT_TITLE, lv_color_hex(UI_COLOR_TEXT),
                    LV_ALIGN_TOP_MID, 0, 28);

    ui_create_btn(scr_search, 160, 52, "Back", go_to_main, LV_ALIGN_TOP_LEFT, 32, 24);
    ui_create_btn(scr_search, 320, 56, "Keyword Search", goto_keyword_search_cb, LV_ALIGN_CENTER, 0, -50);
    ui_create_btn(scr_search, 320, 56, "Category Search", goto_category_search_cb, LV_ALIGN_CENTER, 0, 30);

    if (out_scr_search) *out_scr_search = scr_search;
    s_scr_search = scr_search;

    /* ---- Category 页：与设计图一致四个按钮；DB 中 Science & Tech 存为 Science Tech ---- */
    lv_obj_t *scr_category = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_category, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_category, 0, 0);
    lv_obj_set_style_pad_all(scr_category, 0, 0);

    ui_create_btn(scr_category, 140, 44, "Back", back_to_search_cb, LV_ALIGN_TOP_LEFT, 32, 16);
    ui_create_label(scr_category, "Category", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 28);

    /* 四个分类按钮：按钮文字 / DB 中 category 字段值 */
    static const char *s_cat_labels[] = {
        "Literature",
        "Science & Tech",
        "History",
        "Art",
    };
    static const char *s_cat_db[] = {
        "Literature", "Science Tech", "History", "Art",
    };
    static const lv_coord_t s_cat_y[] = { 88, 156, 224, 292 };

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(scr_category);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 320, 56);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_BTN), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_BTN_BORDER), 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 4, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_BTN_PRESS), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_BTN_PRESS), LV_STATE_PRESSED);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, s_cat_y[i]);
        lv_obj_add_event_cb(btn, category_btn_cb, LV_EVENT_CLICKED, (void *)s_cat_db[i]);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_cat_labels[i]);
        lv_obj_set_style_text_font(lbl, FONT_CN, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_BTN_TEXT), 0);
        lv_obj_center(lbl);
    }

    if (out_scr_search_category) *out_scr_search_category = scr_category;
    s_scr_category = scr_category;

    /* ---- 关键词输入页 ---- */
    lv_obj_t *scr_input = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_input, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(scr_input, 0, 0);
    lv_obj_set_style_pad_all(scr_input, 0, 0);

    ui_create_btn(scr_input, 140, 44, "Back", back_to_search_cb, LV_ALIGN_TOP_LEFT, 32, 16);

    ui_create_label(scr_input, "Keyword Search", FONT_TITLE,
                    lv_color_hex(UI_COLOR_TEXT), LV_ALIGN_TOP_MID, 0, 28);

    ui_create_btn(scr_input, 120, 44, "Search", btn_search_click_cb, LV_ALIGN_TOP_RIGHT, -32, 16);

    s_ta_keyword = lv_textarea_create(scr_input);
    lv_textarea_set_one_line(s_ta_keyword, true);
    lv_textarea_set_max_length(s_ta_keyword, 64);
    lv_textarea_set_placeholder_text(s_ta_keyword, "Input book name or author...");
    lv_obj_set_size(s_ta_keyword, 720, 52);
    lv_obj_align(s_ta_keyword, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_text_font(s_ta_keyword, FONT_CN, 0);
    lv_obj_set_style_bg_color(s_ta_keyword, lv_color_hex(0xf1f5f9), 0);
    lv_obj_set_style_border_color(s_ta_keyword, lv_color_hex(0xcbd5e1), 0);
    lv_obj_set_style_border_width(s_ta_keyword, 2, 0);
    lv_obj_set_style_radius(s_ta_keyword, 8, 0);
    lv_obj_set_style_pad_left(s_ta_keyword, 16, 0);
    lv_obj_add_event_cb(s_ta_keyword, ta_input_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_keyword, ta_input_focus_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_search_status = lv_label_create(scr_input);
    lv_label_set_long_mode(s_lbl_search_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_search_status, 720);
    lv_obj_set_style_text_font(s_lbl_search_status, FONT_CN, 0);
    lv_obj_set_style_text_color(s_lbl_search_status, lv_color_hex(UI_COLOR_TEXT_MUTE), 0);
    lv_label_set_text(s_lbl_search_status, "");
    lv_obj_align(s_lbl_search_status, LV_ALIGN_TOP_MID, 0, 148);

    s_cont_results = lv_obj_create(scr_input);
    lv_obj_set_size(s_cont_results, 736, 232);
    lv_obj_align(s_cont_results, LV_ALIGN_TOP_MID, 0, 188);
    lv_obj_set_style_bg_color(s_cont_results, lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_border_color(s_cont_results, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(s_cont_results, 1, 0);
    lv_obj_set_style_radius(s_cont_results, 8, 0);
    lv_obj_set_style_pad_all(s_cont_results, 10, 0);
    lv_obj_set_scroll_dir(s_cont_results, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_cont_results, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(s_cont_results, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_cont_results, 4, 0);

    s_kb_search = lv_keyboard_create(scr_input);
    lv_obj_set_size(s_kb_search, LV_PCT(88), LV_PCT(42));
    lv_obj_align(s_kb_search, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_text_font(s_kb_search, FONT_CN, 0);
    lv_obj_add_event_cb(s_kb_search, kb_close_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(s_kb_search, LV_OBJ_FLAG_HIDDEN);

    if (out_scr_search_input) *out_scr_search_input = scr_input;
    if (out_kb_search) *out_kb_search = s_kb_search;
    s_scr_input = scr_input;
}
