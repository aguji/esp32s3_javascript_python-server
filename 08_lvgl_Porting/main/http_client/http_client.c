/*
 * HTTP Client for ESP-IDF
 * Simple test to verify connection with Flask backend server
 *
 * 使用 esp_http_client 实现
 */
#include "http_client.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "http_client";

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

static char s_server_url[HTTP_CLIENT_SERVER_URL_LEN] =
    "http://" HTTP_CLIENT_SERVER_IP ":" STRINGIFY(HTTP_CLIENT_SERVER_PORT);

// 超时时间设置
#define HTTP_TIMEOUT_MS  10000   // 10秒超时

// 登录状态存储
static char s_current_token[HTTP_CLIENT_TOKEN_MAX_LEN] = {0};
static bool s_token_obtained = false;
static char s_user_id[64] = {0};
static char s_user_name[64] = {0};

//测试用静态变量排查token空问题
static int s_last_content_length = -1;
static int s_last_total_read = -1;
static char s_last_raw_data[64] = {0};  // 保存原始数据前若干字节

void http_client_test_init(const char *server_ip, int server_port)
{
    if (server_ip != NULL && server_port > 0) {
        snprintf(s_server_url, sizeof(s_server_url),
                 "http://%s:%d", server_ip, server_port);
    }
    ESP_LOGI(TAG, "HTTP client initialized, server: %s", s_server_url);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER: %s=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}

int http_client_test_ping(void)
{
    char url[256];
    snprintf(url, sizeof(url), "%s/ping", s_server_url);

    ESP_LOGI(TAG, "Ping test to: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return -1;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = -1;

    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Ping response: status=%d", status_code);
        // 读取响应体（即使是小响应也需要读取以正确关闭连接）
        char resp_buf[128];
        int read_len;
        while ((read_len = esp_http_client_read_response(client, resp_buf, sizeof(resp_buf) - 1)) > 0) {
            resp_buf[read_len] = '\0';
            ESP_LOGD(TAG, "Ping response body: %s", resp_buf);
        }
    } else {
        ESP_LOGE(TAG, "Ping failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return status_code;
}

esp_err_t http_client_request_token(char *token_buf, size_t buf_len)
{
    if (token_buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s/request_token", s_server_url);

    ESP_LOGI(TAG, "Request token from: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .buffer_size = 1024,  // 增加缓冲区大小
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            int content_length = esp_http_client_get_content_length(client);
            ESP_LOGI(TAG, "Token response content_length=%d, buf_len=%d", content_length, (int)buf_len);

            // 处理 content_length <= 0 的情况（可能是chunked编码）
            if (content_length <= 0) {
                // 重置调试状态
                s_last_content_length = -1;
                s_last_total_read = -1;
                memset(s_last_raw_data, 0, sizeof(s_last_raw_data));

                // 尝试直接读取响应体
                char temp_buf[256] = {0};
                int total_read = 0;
                int read_len;
                while ((read_len = esp_http_client_read_response(client, temp_buf + total_read, sizeof(temp_buf) - total_read - 1)) > 0) {
                    total_read += read_len;
                    if (total_read >= (int)sizeof(temp_buf) - 1) break;
                }
                temp_buf[total_read] = '\0';
                ESP_LOGI(TAG, "Chunked response, read %d bytes: %s", total_read, temp_buf);

                if (total_read > 0 && total_read < (int)buf_len) {
                    memcpy(token_buf, temp_buf, total_read);
                    token_buf[total_read] = '\0';
                    ESP_LOGI(TAG, "Token hex: %02x %02x %02x %02x %02x %02x %02x %02x",
                             (unsigned char)token_buf[0], (unsigned char)token_buf[1],
                             (unsigned char)token_buf[2], (unsigned char)token_buf[3],
                             (unsigned char)token_buf[4], (unsigned char)token_buf[5],
                             (unsigned char)token_buf[6], (unsigned char)token_buf[7]);
                    // 去除字符串末尾的换行符
                    int len = strlen(token_buf);
                    while (len > 0 && (token_buf[len-1] == '\n' || token_buf[len-1] == '\r')) {
                        token_buf[len-1] = '\0';
                        len--;
                    }
                    ESP_LOGI(TAG, "Token after trim, len=%d: [%s]", len, token_buf);
                    // 记录调试状态
                    s_last_content_length = content_length;
                    s_last_total_read = total_read;
                    memcpy(s_last_raw_data, temp_buf, (total_read < (int)sizeof(s_last_raw_data)-1) ? total_read : (int)sizeof(s_last_raw_data)-1);
                } else {
                    ESP_LOGE(TAG, "total_read=%d, buf_len=%d - not copying", total_read, (int)buf_len);
                    err = ESP_FAIL;
                }
            } else if (content_length > 0 && content_length < (int)buf_len) {
                // 重置调试状态
                s_last_content_length = content_length;
                s_last_total_read = -1;
                memset(s_last_raw_data, 0, sizeof(s_last_raw_data));

                // 使用 esp_http_client_read 直接读取响应体
                int total_read = 0;
                int read_len;
                while (total_read < content_length) {
                    read_len = esp_http_client_read(client, token_buf + total_read, content_length - total_read);
                    if (read_len < 0) {
                        ESP_LOGE(TAG, "Read error: %d", read_len);
                        err = ESP_FAIL;
                        break;
                    } else if (read_len == 0) {
                        // 没有更多数据
                        break;
                    }
                    total_read += read_len;
                }
                token_buf[total_read] = '\0';
                ESP_LOGI(TAG, "Total read %d bytes (expected %d)", total_read, content_length);
                // 打印十六进制前几个字节
                ESP_LOGI(TAG, "Token hex: %02x %02x %02x %02x %02x %02x %02x %02x",
                         (unsigned char)token_buf[0], (unsigned char)token_buf[1],
                         (unsigned char)token_buf[2], (unsigned char)token_buf[3],
                         (unsigned char)token_buf[4], (unsigned char)token_buf[5],
                         (unsigned char)token_buf[6], (unsigned char)token_buf[7]);
                // 记录调试状态
                s_last_total_read = total_read;
                memcpy(s_last_raw_data, token_buf, (total_read < (int)sizeof(s_last_raw_data)-1) ? total_read : (int)sizeof(s_last_raw_data)-1);
                s_last_raw_data[sizeof(s_last_raw_data)-1] = '\0';

                if (total_read > 0) {
                    // 去除末尾换行
                    int len = strlen(token_buf);
                    while (len > 0 && (token_buf[len-1] == '\n' || token_buf[len-1] == '\r')) {
                        token_buf[len-1] = '\0';
                        len--;
                    }
                    ESP_LOGI(TAG, "Token received: '%s'", token_buf);
                    err = ESP_OK;
                } else {
                    ESP_LOGE(TAG, "No data read");
                    err = ESP_FAIL;
                }
            } else {
                ESP_LOGE(TAG, "Invalid content_length: %d", content_length);
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "Request token failed: HTTP %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Request token error: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// ====================== 简单 JSON 解析辅助函数 ======================

static int json_skip_whitespace(const char *p, int len, int pos)
{
    while (pos < len && (p[pos] == ' ' || p[pos] == '\t' || p[pos] == '\n' || p[pos] == '\r')) {
        pos++;
    }
    return pos;
}

static int json_extract_string(const char *p, int len, int start, char *out, size_t out_len)
{
    if (start >= len || p[start] != '"') return -1;

    int i = start + 1;
    int j = 0;

    while (i < len && j < (int)out_len - 1) {
        if (p[i] == '\\' && i + 1 < len) {
            // 转义字符
            i++;
            switch (p[i]) {
            case 'n': out[j++] = '\n'; break;
            case 't': out[j++] = '\t'; break;
            case 'r': out[j++] = '\r'; break;
            case '"': out[j++] = '"'; break;
            case '\\': out[j++] = '\\'; break;
            default: out[j++] = p[i]; break;
            }
        } else if (p[i] == '"') {
            break;
        } else {
            out[j++] = p[i];
        }
        i++;
    }

    out[j] = '\0';
    return i + 1;
}

static int json_find_key_value(const char *json, int json_len, const char *key, char *value, size_t value_len)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    int pattern_len = strlen(pattern);

    for (int i = 0; i <= json_len - pattern_len; i++) {
        if (strncmp(json + i, pattern, pattern_len) == 0) {
            // 找到 key，寻找冒号
            int pos = i + pattern_len;
            pos = json_skip_whitespace(json, json_len, pos);

            if (pos < json_len && json[pos] == ':') {
                pos++;
                pos = json_skip_whitespace(json, json_len, pos);

                if (pos < json_len) {
                    if (json[pos] == '"') {
                        // 字符串值
                        return json_extract_string(json, json_len, pos, value, value_len);
                    } else {
                        // 非字符串值 (true, false, null, 数字)
                        int end = pos;
                        while (end < json_len && json[end] != ',' && json[end] != '}') {
                            end++;
                        }
                        int len = end - pos;
                        if (len >= (int)value_len) len = value_len - 1;
                        strncpy(value, json + pos, len);
                        value[len] = '\0';
                        return end;
                    }
                }
            }
        }
    }
    return -1;
}

// ====================== 解析登录结果 ======================

static esp_err_t parse_login_result(const char *json_response, int json_len, login_result_t *result)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(login_result_t));

    // 解析 status
    json_find_key_value(json_response, json_len, "status", result->status, sizeof(result->status));

    // 解析 user_id
    json_find_key_value(json_response, json_len, "user_id", result->user_id, sizeof(result->user_id));

    // 解析 user_name
    json_find_key_value(json_response, json_len, "user_name", result->user_name, sizeof(result->user_name));

    ESP_LOGI(TAG, "Parsed result: status=%s, user_id=%s, user_name=%s",
             result->status, result->user_id, result->user_name);

    return ESP_OK;
}

// ====================== 重试和错误处理 ======================

// 最大重试次数
#define HTTP_MAX_RETRIES  3
// 重试间隔（毫秒）
#define HTTP_RETRY_DELAY_MS  1000

static esp_err_t perform_with_retry(esp_http_client_config_t *config, int *out_status_code)
{
    esp_err_t last_err = ESP_FAIL;
    int status_code = 0;

    for (int retry = 0; retry < HTTP_MAX_RETRIES; retry++) {
        esp_http_client_handle_t client = esp_http_client_init(config);
        if (client == NULL) {
            ESP_LOGE(TAG, "Retry %d: Failed to init HTTP client", retry);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
            continue;
        }

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            status_code = esp_http_client_get_status_code(client);
            *out_status_code = status_code;
            esp_http_client_cleanup(client);

            if (status_code >= 200 && status_code < 300) {
                return ESP_OK;
            } else if (status_code >= 500) {
                // 服务器错误，可重试
                ESP_LOGW(TAG, "Retry %d: Server error HTTP %d", retry, status_code);
                last_err = ESP_FAIL;
            } else {
                // 客户端错误（4xx），不重试
                ESP_LOGE(TAG, "Client error HTTP %d, not retrying", status_code);
                return ESP_FAIL;
            }
        } else {
            last_err = err;
            ESP_LOGW(TAG, "Retry %d: Request failed: %s", retry, esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        if (retry < HTTP_MAX_RETRIES - 1) {
            ESP_LOGI(TAG, "Waiting %dms before retry...", HTTP_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "All retries failed, last error: %s", esp_err_to_name(last_err));
    return last_err;
}

esp_err_t http_client_check_status(const char *token, char *status_buf, size_t buf_len)
{
    if (token == NULL || status_buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[512];
    char encoded_token[128];

    // URL 编码 token
    int enc_idx = 0;
    for (int i = 0; token[i] != '\0' && enc_idx < (int)sizeof(encoded_token) - 4; i++) {
        char c = token[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') {
            encoded_token[enc_idx++] = c;
        } else {
            enc_idx += snprintf(encoded_token + enc_idx, 4, "%%%02X", (unsigned char)c);
        }
    }
    encoded_token[enc_idx] = '\0';

    ESP_LOGI(TAG, "Original token: [%s], encoded: [%s]", token, encoded_token);
    snprintf(url, sizeof(url), "%s/check_token?token=%s", s_server_url, encoded_token);
    ESP_LOGI(TAG, "Full check_url: %s", url);

    // 使用单个客户端，一次完成请求和读取
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    /*
     * 勿用 esp_http_client_perform + esp_http_client_read_response 组合：
     * 在部分 IDF 版本上 perform 已读完响应体，read_response 恒为 0，导致正文为空（界面 raw=[]）。
     * 改用 open -> fetch_headers -> read 显式读 body。
     */
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Check status: HTTP %d, content_length=%lld", status_code,
             (long long)content_length);

    if (status_code != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int total_read = 0;
    while (total_read < (int)buf_len - 1) {
        int read_len = esp_http_client_read(client, status_buf + total_read,
                                            (int)(buf_len - 1 - (size_t)total_read));
        if (read_len < 0) {
            ESP_LOGE(TAG, "HTTP read error: %d", read_len);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        if (read_len == 0) {
            break;
        }
        total_read += read_len;
    }
    status_buf[total_read] = '\0';
    ESP_LOGI(TAG, "Total read %d bytes: [%s]", total_read, status_buf);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    /* 仅去掉末尾空白 */
    while (total_read > 0) {
        char c = status_buf[total_read - 1];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            total_read--;
            status_buf[total_read] = '\0';
        } else {
            break;
        }
    }

    if (total_read > 0) {
        ESP_LOGI(TAG, "Token status body: %s", status_buf);
        return ESP_OK;
    }

    return ESP_FAIL;
}

bool http_client_simple_test(char *token_out)
{
    char token[HTTP_CLIENT_TOKEN_MAX_LEN] = {0};

    // Step 1: Ping test
    ESP_LOGI(TAG, "=== HTTP Client Simple Test ===");

    int ping_status = http_client_test_ping();
    if (ping_status < 0) {
        ESP_LOGE(TAG, "Ping failed! Server may be unreachable.");
        return false;
    } else if (ping_status != 200) {
        ESP_LOGW(TAG, "Ping returned unexpected status: %d", ping_status);
    }
    ESP_LOGI(TAG, "Ping test: PASS (HTTP %d)", ping_status);

    // Step 2: Request token
    esp_err_t err = http_client_request_token(token, sizeof(token));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Request token failed!");
        return false;
    }
    ESP_LOGI(TAG, "Request token: PASS, token=%s", token);

    // Step 3: Check token status
    login_result_t result;
    err = http_client_check_login_with_user(&result);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Check token status failed!");
        return false;
    }
    ESP_LOGI(TAG, "Token status: %s", result.status);

    // Copy token to output
    if (token_out) {
        strncpy(token_out, token, HTTP_CLIENT_TOKEN_MAX_LEN - 1);
    }

    ESP_LOGI(TAG, "=== HTTP Client Test Complete ===");
    return true;
}

// ============================================
// 以下是用于在LVGL中调用的简化封装
// ============================================

esp_err_t http_client_register_token(const char *token)
{
    char url[512];
    char encoded_token[128];

    // URL 编码 token
    int enc_idx = 0;
    for (int i = 0; token[i] != '\0' && enc_idx < (int)sizeof(encoded_token) - 4; i++) {
        char c = token[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') {
            encoded_token[enc_idx++] = c;
        } else {
            enc_idx += snprintf(encoded_token + enc_idx, 4, "%%%02X", (unsigned char)c);
        }
    }
    encoded_token[enc_idx] = '\0';
    snprintf(url, sizeof(url), "%s/register_token?token=%s", s_server_url, encoded_token);

    ESP_LOGI(TAG, "Register token to: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "http_client_init FAILED (client is NULL)");
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Register token response: err=0x%x(%s), status=%d", err, esp_err_to_name(err), status_code);
    esp_http_client_cleanup(client);
    if (err == ESP_OK && status_code == 200) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "register_token FAILED: err=0x%x, status_code=%d", err, status_code);
    return ESP_FAIL;
}

bool http_client_start_login(const char *server_ip, int server_port)
{
    http_client_test_init(server_ip, server_port);

    // 生成本地 token
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    snprintf(s_current_token, sizeof(s_current_token), "%08lx%08lx", (unsigned long)r1, (unsigned long)r2);

    // 注册到服务器
    esp_err_t err = http_client_register_token(s_current_token);
    if (err == ESP_OK) {
        s_token_obtained = true;
        ESP_LOGI(TAG, "Login started, token: %s", s_current_token);
        return true;
    } else {
        s_token_obtained = false;
        ESP_LOGE(TAG, "Failed to register token");
        return false;
    }
}

const char* http_client_get_token(void)
{
    return s_token_obtained ? s_current_token : NULL;
}

bool http_client_check_login(void)
{
    if (!s_token_obtained) {
        return false;
    }

    char response[256] = {0};
    esp_err_t err = http_client_check_status(s_current_token, response, sizeof(response));

    if (err == ESP_OK) {
        // 检查是否是 JSON 格式
        if (response[0] == '{') {
            login_result_t result;
            parse_login_result(response, strlen(response), &result);

            if (strcmp(result.status, "success") == 0) {
                // 保存用户信息
                strncpy(s_user_id, result.user_id, sizeof(s_user_id) - 1);
                strncpy(s_user_name, result.user_name, sizeof(s_user_name) - 1);
                ESP_LOGI(TAG, "Login SUCCESS! user=%s", s_user_name);
                return true;
            } else if (strcmp(result.status, "pending") == 0) {
                ESP_LOGI(TAG, "Login pending...");
                return false;
            } else {
                ESP_LOGW(TAG, "Login invalid: %s", result.status);
                s_token_obtained = false;
                return false;
            }
        } else {
            // 兼容旧格式（纯文本）
            if (strcmp(response, "success") == 0) {
                ESP_LOGI(TAG, "Login SUCCESS!");
                return true;
            } else if (strcmp(response, "pending") == 0) {
                ESP_LOGI(TAG, "Login pending...");
                return false;
            } else {
                ESP_LOGW(TAG, "Login invalid: %s", response);
                s_token_obtained = false;
                return false;
            }
        }
    }

    return false;
}

bool http_client_check_login_with_user(login_result_t *result)
{
    if (result == NULL) {
        return false;
    }

    if (!s_token_obtained) {
        memset(result, 0, sizeof(login_result_t));
        strcpy(result->status, "invalid");
        return false;
    }

    char response[256] = {0};
    esp_err_t err = http_client_check_status(s_current_token, response, sizeof(response));

    ESP_LOGI(TAG, "http_client_check_status returned: %s, response: [%s]", esp_err_to_name(err), response);

    if (err == ESP_OK) {
        // 检查是否是 JSON 格式
        if (response[0] == '{') {
            parse_login_result(response, strlen(response), result);

            ESP_LOGI(TAG, "JSON Parsed: status=%s, user_id=%s, user_name=%s",
                     result->status, result->user_id, result->user_name);

            if (strcmp(result->status, "success") == 0) {
                // 保存用户信息到全局变量
                strncpy(s_user_id, result->user_id, sizeof(s_user_id) - 1);
                strncpy(s_user_name, result->user_name, sizeof(s_user_name) - 1);
                ESP_LOGI(TAG, "Login SUCCESS! user=%s", s_user_name);
                return true;
            } else if (strcmp(result->status, "pending") == 0) {
                ESP_LOGI(TAG, "Login pending...");
                return false;
            } else {
                ESP_LOGW(TAG, "Login invalid: %s", result->status);
                s_token_obtained = false;
                return false;
            }
        } else {
            // 纯文本响应（服务器简化版）
            ESP_LOGI(TAG, "Plain text response: [%s]", response);
            if (strcmp(response, "success") == 0) {
                strcpy(result->status, "success");
                ESP_LOGI(TAG, "Login SUCCESS!");
                return true;
            } else if (strcmp(response, "pending") == 0) {
                strcpy(result->status, "pending");
                ESP_LOGI(TAG, "Login pending...");
                return false;
            } else {
                ESP_LOGW(TAG, "Unknown response: %s", response);
                s_token_obtained = false;
                return false;
            }
        }
    }

    memset(result, 0, sizeof(login_result_t));
    return false;
}

const char* http_client_get_user_id(void)
{
    if (s_token_obtained && strlen(s_user_id) > 0) {
        return s_user_id;
    }
    return NULL;
}

const char* http_client_get_user_name(void)
{
    if (s_token_obtained && strlen(s_user_name) > 0) {
        return s_user_name;
    }
    return NULL;
}

void http_client_reset_login(void)
{
    s_current_token[0] = '\0';
    s_token_obtained = false;
    s_user_id[0] = '\0';
    s_user_name[0] = '\0';
}

bool http_client_has_valid_token(void)
{
    return s_token_obtained && strlen(s_current_token) > 0;
}

void http_client_get_last_token_status(int *out_content_len, int *out_read_len, char *out_hex, size_t buf_size)
{
    if (out_content_len) *out_content_len = s_last_content_length;
    if (out_read_len) *out_read_len = s_last_total_read;
    if (out_hex && buf_size > 0) {
        // 将 s_last_raw_data 转换为十六进制字符串
        int len = strlen(s_last_raw_data);
        if (len > 16) len = 16;
        for (int i = 0; i < len && i*2+2 < (int)buf_size; i++) {
            sprintf(out_hex + i*2, "%02x", (unsigned char)s_last_raw_data[i]);
        }
        out_hex[len*2] = '\0';
    }
}

esp_err_t http_client_check_rfid_login(const char *uid, login_result_t *result)
{
    if (uid == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/rfid_login?uid=%s", s_server_url, uid);
    ESP_LOGI(TAG, "RFID login request: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    /*
     * 与 http_client_check_status 一致：勿用 perform + read_response，
     * IDF 5.x 上 perform 已消费响应体，read_response 常为 0，JSON 永远为空。
     */
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RFID login HTTP open failed: %s", esp_err_to_name(err));
        memset(result, 0, sizeof(login_result_t));
        strcpy(result->status, "invalid");
        esp_http_client_cleanup(client);
        return err;
    }

    (void)esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    char resp_buf[256] = {0};
    int total_read = 0;
    if (status_code == 200) {
        while (total_read < (int)sizeof(resp_buf) - 1) {
            int n = esp_http_client_read(client, resp_buf + total_read,
                                         (int)(sizeof(resp_buf) - 1 - (size_t)total_read));
            if (n < 0) {
                ESP_LOGE(TAG, "RFID login HTTP read error: %d", n);
                break;
            }
            if (n == 0) {
                break;
            }
            total_read += n;
        }
    }
    resp_buf[total_read] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "RFID login failed: HTTP %d", status_code);
        memset(result, 0, sizeof(login_result_t));
        strcpy(result->status, "invalid");
        return ESP_FAIL;
    }

    if (total_read > 0) {
        ESP_LOGI(TAG, "RFID login response: %s", resp_buf);
        parse_login_result(resp_buf, total_read, result);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "RFID login: HTTP 200 but empty body");
    memset(result, 0, sizeof(login_result_t));
    strcpy(result->status, "invalid");
    return ESP_FAIL;
}

/* ---------- 借书 / 还书：POST JSON ---------- */

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} http_post_resp_accum_t;

static esp_err_t http_post_on_data(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || evt->data_len <= 0 || !evt->data) {
        return ESP_OK;
    }
    http_post_resp_accum_t *acc = (http_post_resp_accum_t *)evt->user_data;
    if (!acc || !acc->buf || acc->cap < 2) {
        return ESP_OK;
    }
    size_t room = (acc->cap > acc->len + 1) ? (acc->cap - acc->len - 1) : 0;
    if (room == 0) {
        return ESP_OK;
    }
    size_t n = (size_t)evt->data_len;
    if (n > room) {
        n = room;
    }
    memcpy(acc->buf + acc->len, evt->data, n);
    acc->len += n;
    acc->buf[acc->len] = '\0';
    return ESP_OK;
}

static void borrow_trim_cstr(char *s)
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

/** 生成 {"reader_card":"...","book_uid":"..."}，需 free；无效返回 NULL */
static char *build_borrow_return_json(const char *reader_card, const char *book_uid)
{
    char rc[80];
    char bu[48];
    if (!reader_card || !book_uid) {
        return NULL;
    }
    strncpy(rc, reader_card, sizeof(rc) - 1);
    rc[sizeof(rc) - 1] = '\0';
    strncpy(bu, book_uid, sizeof(bu) - 1);
    bu[sizeof(bu) - 1] = '\0';
    borrow_trim_cstr(rc);
    borrow_trim_cstr(bu);
    if (rc[0] == '\0' || bu[0] == '\0') {
        return NULL;
    }
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON_AddStringToObject(o, "reader_card", rc);
    cJSON_AddStringToObject(o, "book_uid", bu);
    char *out = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return out;
}

static esp_err_t http_post_json_read_body(const char *path_after_host, const char *json_body,
                                          char *resp_buf, size_t resp_buf_len, int *out_http_code)
{
    if (path_after_host == NULL || json_body == NULL || resp_buf == NULL || resp_buf_len < 4) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s%s", s_server_url, path_after_host);

    int body_len = (int)strlen(json_body);

    memset(resp_buf, 0, resp_buf_len);
    http_post_resp_accum_t acc = { .buf = resp_buf, .cap = resp_buf_len, .len = 0 };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_post_on_data,
        .user_data = &acc,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Connection", "close");
    esp_http_client_set_post_field(client, json_body, body_len);

    esp_err_t err = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    if (out_http_code) {
        *out_http_code = code;
    }

    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t http_client_borrow_book(const char *reader_card, const char *book_uid,
                                  char *title_out, size_t title_len,
                                  char *err_out, size_t err_len)
{
    if (reader_card == NULL || book_uid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (err_out && err_len) {
        err_out[0] = '\0';
    }
    if (title_out && title_len) {
        title_out[0] = '\0';
    }

    char *body = build_borrow_return_json(reader_card, book_uid);
    if (body == NULL) {
        if (err_out && err_len) {
            snprintf(err_out, err_len, "empty reader_card or book_uid");
        }
        return ESP_ERR_INVALID_ARG;
    }

    char resp[512] = {0};
    int http_code = 0;
    esp_err_t e = http_post_json_read_body("/borrow", body, resp, sizeof(resp), &http_code);
    free(body);
    if (e != ESP_OK) {
        if (err_out && err_len) {
            snprintf(err_out, err_len, "network error");
        }
        return e;
    }

    if (http_code != 200) {
        if (err_out && err_len) {
            if (http_code < 0) {
                snprintf(err_out, err_len, "bad HTTP response");
            } else if (http_code == 404) {
                cJSON *er = cJSON_Parse(resp);
                if (er) {
                    cJSON *m = cJSON_GetObjectItem(er, "message");
                    if (cJSON_IsString(m) && m->valuestring) {
                        snprintf(err_out, err_len, "%s", m->valuestring);
                    } else {
                        snprintf(err_out, err_len, "404: no /borrow - restart server.py");
                    }
                    cJSON_Delete(er);
                } else {
                    snprintf(err_out, err_len, "404: no /borrow - restart server.py");
                }
            } else {
                cJSON *er = cJSON_Parse(resp);
                if (er) {
                    cJSON *m = cJSON_GetObjectItem(er, "message");
                    if (cJSON_IsString(m) && m->valuestring) {
                        snprintf(err_out, err_len, "%s", m->valuestring);
                    } else {
                        snprintf(err_out, err_len, "HTTP %d", http_code);
                    }
                    cJSON_Delete(er);
                } else {
                    snprintf(err_out, err_len, "HTTP %d (not JSON)", http_code);
                }
            }
        }
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp);
    if (root == NULL) {
        if (err_out && err_len) {
            snprintf(err_out, err_len, "bad json");
        }
        return ESP_FAIL;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    int ok_val = cJSON_IsTrue(ok) ? 1 : 0;
    if (!ok_val && ok && cJSON_IsNumber(ok)) {
        ok_val = (ok->valueint != 0) ? 1 : 0;
    }

    cJSON *msg = cJSON_GetObjectItem(root, "message");
    cJSON *title = cJSON_GetObjectItem(root, "title");

    if (!ok_val) {
        if (err_out && err_len) {
            if (cJSON_IsString(msg) && msg->valuestring) {
                snprintf(err_out, err_len, "%s", msg->valuestring);
            } else {
                snprintf(err_out, err_len, "failed http %d", http_code);
            }
        }
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (title_out && title_len && cJSON_IsString(title) && title->valuestring) {
        strncpy(title_out, title->valuestring, title_len - 1);
        title_out[title_len - 1] = '\0';
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t http_client_return_book(const char *reader_card, const char *book_uid,
                                  char *err_out, size_t err_len)
{
    if (reader_card == NULL || book_uid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (err_out && err_len) {
        err_out[0] = '\0';
    }

    char *body = build_borrow_return_json(reader_card, book_uid);
    if (body == NULL) {
        if (err_out && err_len) {
            snprintf(err_out, err_len, "empty reader_card or book_uid");
        }
        return ESP_ERR_INVALID_ARG;
    }

    char resp[384] = {0};
    int http_code = 0;
    esp_err_t e = http_post_json_read_body("/return_book", body, resp, sizeof(resp), &http_code);
    free(body);
    if (e != ESP_OK) {
        if (err_out && err_len) {
            snprintf(err_out, err_len, "network error");
        }
        return e;
    }

    if (http_code != 200) {
        if (err_out && err_len) {
            if (http_code < 0) {
                snprintf(err_out, err_len, "bad HTTP response");
            } else if (http_code == 404) {
                cJSON *er = cJSON_Parse(resp);
                if (er) {
                    cJSON *m = cJSON_GetObjectItem(er, "message");
                    if (cJSON_IsString(m) && m->valuestring) {
                        snprintf(err_out, err_len, "%s", m->valuestring);
                    } else {
                        snprintf(err_out, err_len, "404: no /return_book - restart server.py");
                    }
                    cJSON_Delete(er);
                } else {
                    snprintf(err_out, err_len, "404: no /return_book - restart server.py");
                }
            } else {
                cJSON *er = cJSON_Parse(resp);
                if (er) {
                    cJSON *m = cJSON_GetObjectItem(er, "message");
                    if (cJSON_IsString(m) && m->valuestring) {
                        snprintf(err_out, err_len, "%s", m->valuestring);
                    } else {
                        snprintf(err_out, err_len, "HTTP %d", http_code);
                    }
                    cJSON_Delete(er);
                } else {
                    snprintf(err_out, err_len, "HTTP %d (not JSON)", http_code);
                }
            }
        }
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp);
    if (root == NULL) {
        if (err_out && err_len) {
            snprintf(err_out, err_len, "bad json");
        }
        return ESP_FAIL;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    int ok_val = cJSON_IsTrue(ok) ? 1 : 0;
    if (!ok_val && ok && cJSON_IsNumber(ok)) {
        ok_val = (ok->valueint != 0) ? 1 : 0;
    }

    cJSON *msg = cJSON_GetObjectItem(root, "message");

    if (!ok_val) {
        if (err_out && err_len) {
            if (cJSON_IsString(msg) && msg->valuestring) {
                snprintf(err_out, err_len, "%s", msg->valuestring);
            } else {
                snprintf(err_out, err_len, "failed http %d", http_code);
            }
        }
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t http_client_search_books(const char *keyword, search_result_t *result)
{
    if (keyword == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(search_result_t));

    char encoded_keyword[256] = {0};
    int enc_idx = 0;
    for (int i = 0; keyword[i] != '\0' && enc_idx < (int)sizeof(encoded_keyword) - 4; i++) {
        char c = keyword[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == ' ') {
            if (c == ' ') {
                enc_idx += snprintf(encoded_keyword + enc_idx, 4, "%%20");
            } else {
                encoded_keyword[enc_idx++] = c;
            }
        } else {
            enc_idx += snprintf(encoded_keyword + enc_idx, 4, "%%%02X", (unsigned char)c);
        }
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/search?keyword=%s", s_server_url, encoded_keyword);
    ESP_LOGI(TAG, "Search books URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client for search");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed for search: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Search response: HTTP %d, content_length=%lld", status_code, (long long)content_length);

    if (status_code != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    char resp_buf[8192] = {0};
    int total_read = 0;
    int buf_size = sizeof(resp_buf) - 1;
    while (total_read < buf_size) {
        int read_len = esp_http_client_read(client, resp_buf + total_read, buf_size - total_read);
        if (read_len < 0) {
            ESP_LOGE(TAG, "HTTP read error: %d", read_len);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        if (read_len == 0) {
            break;
        }
        total_read += read_len;
    }
    resp_buf[total_read] = '\0';
    ESP_LOGI(TAG, "Search response: %d bytes", total_read);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    cJSON *root = cJSON_Parse(resp_buf);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse search response JSON");
        return ESP_FAIL;
    }

    cJSON *books_array = cJSON_GetObjectItem(root, "books");
    if (!cJSON_IsArray(books_array)) {
        ESP_LOGE(TAG, "Search response: 'books' is not an array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int count = cJSON_GetArraySize(books_array);
    result->count = (count > 50) ? 50 : count;

    for (int i = 0; i < result->count; i++) {
        cJSON *book_obj = cJSON_GetArrayItem(books_array, i);
        if (book_obj == NULL) continue;

        cJSON *id = cJSON_GetObjectItem(book_obj, "id");
        cJSON *isbn = cJSON_GetObjectItem(book_obj, "isbn");
        cJSON *title = cJSON_GetObjectItem(book_obj, "title");
        cJSON *author = cJSON_GetObjectItem(book_obj, "author");
        cJSON *publisher = cJSON_GetObjectItem(book_obj, "publisher");
        cJSON *location = cJSON_GetObjectItem(book_obj, "location");
        cJSON *rfid_uid = cJSON_GetObjectItem(book_obj, "rfid_uid");
        cJSON *status = cJSON_GetObjectItem(book_obj, "status");

        result->books[i].id = cJSON_IsNumber(id) ? id->valueint : 0;
        result->books[i].status = cJSON_IsNumber(status) ? status->valueint : 0;

        if (cJSON_IsString(isbn) && isbn->valuestring) {
            strncpy(result->books[i].isbn, isbn->valuestring, sizeof(result->books[i].isbn) - 1);
        }
        if (cJSON_IsString(title) && title->valuestring) {
            strncpy(result->books[i].title, title->valuestring, sizeof(result->books[i].title) - 1);
        }
        if (cJSON_IsString(author) && author->valuestring) {
            strncpy(result->books[i].author, author->valuestring, sizeof(result->books[i].author) - 1);
        }
        if (cJSON_IsString(publisher) && publisher->valuestring) {
            strncpy(result->books[i].publisher, publisher->valuestring, sizeof(result->books[i].publisher) - 1);
        }
        if (cJSON_IsString(location) && location->valuestring) {
            strncpy(result->books[i].location, location->valuestring, sizeof(result->books[i].location) - 1);
        }
        if (cJSON_IsString(rfid_uid) && rfid_uid->valuestring) {
            strncpy(result->books[i].rfid_uid, rfid_uid->valuestring, sizeof(result->books[i].rfid_uid) - 1);
        }

        ESP_LOGI(TAG, "  [%d] %s by %s (ISBN: %s, Status: %d)",
                 i, result->books[i].title, result->books[i].author,
                 result->books[i].isbn, result->books[i].status);
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Search complete: found %d books", result->count);
    return ESP_OK;
}

esp_err_t http_client_search_books_by_category(const char *category, search_result_t *result)
{
    if (category == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(search_result_t));

    char encoded[256] = {0};
    int enc_idx = 0;
    for (int i = 0; category[i] != '\0' && enc_idx < (int)sizeof(encoded) - 4; i++) {
        char c = category[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == ' ') {
            if (c == ' ') {
                enc_idx += snprintf(encoded + enc_idx, 4, "%%20");
            } else {
                encoded[enc_idx++] = c;
            }
        } else {
            enc_idx += snprintf(encoded + enc_idx, 4, "%%%02X", (unsigned char)c);
        }
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/search_by_category?category=%s", s_server_url, encoded);
    ESP_LOGI(TAG, "Search by category URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client for category search");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed for category search: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    (void)esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Category search response: HTTP %d", status_code);

    if (status_code != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    char resp_buf[8192] = {0};
    int total_read = 0;
    int buf_size = sizeof(resp_buf) - 1;
    while (total_read < buf_size) {
        int read_len = esp_http_client_read(client, resp_buf + total_read, buf_size - total_read);
        if (read_len < 0) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        if (read_len == 0) {
            break;
        }
        total_read += read_len;
    }
    resp_buf[total_read] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    cJSON *root = cJSON_Parse(resp_buf);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse category search JSON");
        return ESP_FAIL;
    }

    cJSON *books_array = cJSON_GetObjectItem(root, "books");
    if (!cJSON_IsArray(books_array)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int count = cJSON_GetArraySize(books_array);
    result->count = (count > 50) ? 50 : count;

    for (int i = 0; i < result->count; i++) {
        cJSON *book_obj = cJSON_GetArrayItem(books_array, i);
        if (book_obj == NULL) {
            continue;
        }

        cJSON *id = cJSON_GetObjectItem(book_obj, "id");
        cJSON *isbn = cJSON_GetObjectItem(book_obj, "isbn");
        cJSON *title = cJSON_GetObjectItem(book_obj, "title");
        cJSON *author = cJSON_GetObjectItem(book_obj, "author");
        cJSON *publisher = cJSON_GetObjectItem(book_obj, "publisher");
        cJSON *location = cJSON_GetObjectItem(book_obj, "location");
        cJSON *rfid_uid = cJSON_GetObjectItem(book_obj, "rfid_uid");
        cJSON *status = cJSON_GetObjectItem(book_obj, "status");

        result->books[i].id = cJSON_IsNumber(id) ? id->valueint : 0;
        result->books[i].status = cJSON_IsNumber(status) ? status->valueint : 0;

        if (cJSON_IsString(isbn) && isbn->valuestring) {
            strncpy(result->books[i].isbn, isbn->valuestring, sizeof(result->books[i].isbn) - 1);
        }
        if (cJSON_IsString(title) && title->valuestring) {
            strncpy(result->books[i].title, title->valuestring, sizeof(result->books[i].title) - 1);
        }
        if (cJSON_IsString(author) && author->valuestring) {
            strncpy(result->books[i].author, author->valuestring, sizeof(result->books[i].author) - 1);
        }
        if (cJSON_IsString(publisher) && publisher->valuestring) {
            strncpy(result->books[i].publisher, publisher->valuestring, sizeof(result->books[i].publisher) - 1);
        }
        if (cJSON_IsString(location) && location->valuestring) {
            strncpy(result->books[i].location, location->valuestring, sizeof(result->books[i].location) - 1);
        }
        if (cJSON_IsString(rfid_uid) && rfid_uid->valuestring) {
            strncpy(result->books[i].rfid_uid, rfid_uid->valuestring, sizeof(result->books[i].rfid_uid) - 1);
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Category search complete: %d books", result->count);
    return ESP_OK;
}
