#include "user_session.h"
#include <string.h>
#include <ctype.h>

#define SESSION_ID_MAX   64
#define SESSION_NAME_MAX 64

static void trim_cstr(char *s)
{
    if (!s || !s[0]) {
        return;
    }
    char *p = s;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        n--;
    }
    s[n] = '\0';
}

static char s_user_id[SESSION_ID_MAX];
static char s_display_name[SESSION_NAME_MAX];
static int s_logged_in = 0;

void user_session_set(const char *user_id, const char *display_name)
{
    s_user_id[0] = '\0';
    s_display_name[0] = '\0';
    if (user_id) {
        strncpy(s_user_id, user_id, SESSION_ID_MAX - 1);
        s_user_id[SESSION_ID_MAX - 1] = '\0';
        trim_cstr(s_user_id);
    }
    if (display_name) {
        strncpy(s_display_name, display_name, SESSION_NAME_MAX - 1);
        s_display_name[SESSION_NAME_MAX - 1] = '\0';
        trim_cstr(s_display_name);
    }
    if (s_display_name[0] == '\0' && s_user_id[0] != '\0') {
        strncpy(s_display_name, s_user_id, SESSION_NAME_MAX - 1);
        s_display_name[SESSION_NAME_MAX - 1] = '\0';
    }
    s_logged_in = (s_user_id[0] != '\0');
}

const char *user_session_get_id(void)
{
    if (s_user_id[0] != '\0') return s_user_id;
    return "guest";
}

const char *user_session_get_name(void)
{
    if (s_display_name[0] != '\0') return s_display_name;
    return "Guest";
}

void user_session_clear(void)
{
    s_user_id[0] = '\0';
    s_display_name[0] = '\0';
    s_logged_in = 0;
}

int user_session_is_logged_in(void)
{
    return s_logged_in ? 1 : 0;
}

int user_session_is_admin(void)
{
    if (!s_logged_in) return 0;
    if (s_user_id[0] != '\0') {
        if (strcmp(s_user_id, "admin") == 0) return 1;
        if (strstr(s_user_id, "admin") == s_user_id) return 1; /* admin1, admin2, ... */
    }
    if (s_display_name[0] != '\0') {
        /* 检查是否包含 admin 或 Admin 或 [管理员] */
        if (strstr(s_display_name, "admin") != NULL ||
            strstr(s_display_name, "Admin") != NULL ||
            strstr(s_display_name, "[管理员]") != NULL)
            return 1;
    }
    return 0;
}
