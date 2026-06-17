/*
 * 登录模块实现：改为通过本地数据库校验账号密码
 */
#include "ui_login.h"
#include "local_db.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG_LOGIN = "ui_login";

int ui_login_check_password(const char *user, const char *pass,
                            char *out_display_name, size_t out_len)
{
    if (!user || !pass || user[0] == '\0' || pass[0] == '\0') {
        ESP_LOGW(TAG_LOGIN, "Empty username or password");
        return 0;
    }

    local_db_reader_t reader;
    bool verified = local_db_verify_reader(user, pass, &reader);
    
    if (verified) {
        ESP_LOGI(TAG_LOGIN, "Login successful for user: %s", user);
        
        // 使用读者姓名作为显示名，如果没有则使用卡号
        const char *display = reader.name[0] != '\0' ? reader.name : user;
        
        if (out_display_name && out_len > 0) {
            strncpy(out_display_name, display, out_len - 1);
            out_display_name[out_len - 1] = '\0';
        }
        return 1;
    } else {
        ESP_LOGW(TAG_LOGIN, "Login failed for user: %s", user);
        return 0;
    }
}
