/*
 * 用户会话：登录后记录当前用户，供主菜单、个人中心显示
 */
#ifndef USER_SESSION_H
#define USER_SESSION_H

void user_session_set(const char *user_id, const char *display_name);
const char *user_session_get_id(void);
const char *user_session_get_name(void);
void user_session_clear(void);
int user_session_is_logged_in(void);
/** 是否管理员（user_id 为 "admin" 或以 "admin" 开头视为管理员） */
int user_session_is_admin(void);

#endif
