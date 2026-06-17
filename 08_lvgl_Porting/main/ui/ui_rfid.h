/*
 * RFID 模块：模式、白名单、PN532 读卡任务与回调
 * 登录/借书/还书 的 RFID 逻辑集中在此，便于后期对接数据库
 */
#ifndef UI_RFID_H
#define UI_RFID_H

#include <stddef.h>
#include <stdint.h>

/* 识别场景：登录=人卡白名单；借书/还书=书籍 RFID */
typedef enum {
    UI_RFID_MODE_LOGIN,
    UI_RFID_MODE_BORROW,
    UI_RFID_MODE_RETURN
} ui_rfid_mode_t;

/* 回调：由 UI 层注册，在 LVGL 线程外被调用，需内部加 lvgl_port_lock 再更新界面 */
typedef void (*ui_rfid_status_fn)(const char *msg);
typedef void (*ui_rfid_login_uid_fn)(const char *uid_hex);  /* LOGIN 模式下刷到卡时显示 UID */
typedef void (*ui_rfid_login_ok_fn)(const char *user_id, const char *display_name);
typedef void (*ui_rfid_login_fail_fn)(void);
typedef void (*ui_rfid_book_fn)(const char *uid_hex, int is_return);
typedef void (*ui_rfid_debug_fn)(const char *debug_msg);   /* 调试信息回调 */

typedef struct {
    ui_rfid_status_fn     on_status;
    ui_rfid_login_uid_fn  on_login_uid;
    ui_rfid_login_ok_fn   on_login_ok;
    ui_rfid_login_fail_fn on_login_fail;
    ui_rfid_book_fn       on_book;
    ui_rfid_debug_fn      on_debug;    /* 调试信息 */
} ui_rfid_callbacks_t;

/** 注册回调（在 library_ui_create 里调用） */
void ui_rfid_register_callbacks(const ui_rfid_callbacks_t *cbs);

/** 设置当前模式（登录 / 借书 / 还书） */
void ui_rfid_set_mode(ui_rfid_mode_t mode);

/** 启动读卡任务（已运行则无操作） */
void ui_rfid_start_reader(void);

/** 请求停止读卡任务（登录成功后由模块内部调用，也可由外部调） */
void ui_rfid_request_stop(void);

#endif
