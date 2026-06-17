/*
 * 登录模块：密码白名单与校验（与 RFID 登录分开，此处仅密码登录数据）
 */
#ifndef UI_LOGIN_H
#define UI_LOGIN_H

#include <stddef.h>

/**
 * 校验账号密码，并返回展示名（用于 user_session_set）
 * @param user 账号
 * @param pass 密码
 * @param out_display_name 输出展示名，可为 NULL
 * @param out_len out_display_name 缓冲区长度
 * @return 1 成功，0 失败
 */
int ui_login_check_password(const char *user, const char *pass,
                            char *out_display_name, size_t out_len);

#endif
