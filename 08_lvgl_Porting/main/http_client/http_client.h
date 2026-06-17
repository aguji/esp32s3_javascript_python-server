/*
 * HTTP Client for ESP-IDF
 * Simple test to verify connection with Flask backend server
 */
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "local_db.h"

/* ============================================================
 * 服务器配置（一处修改，全局生效）
 * 改这里即可，不要在其它文件硬编码 IP
 * ============================================================ */
#define HTTP_CLIENT_SERVER_IP   "192.168.x.x"
#define HTTP_CLIENT_SERVER_PORT 5000
/* ============================================================ */

#define HTTP_CLIENT_SERVER_URL_LEN 128
#define HTTP_CLIENT_TOKEN_MAX_LEN  64
#define HTTP_CLIENT_RESPONSE_MAX_LEN 256

// 登录结果结构体
typedef struct {
    char status[32];      // pending, success, invalid
    char user_id[64];     // 用户ID
    char user_name[64];  // 用户名
} login_result_t;

/**
 * @brief Initialize HTTP client test
 * @param server_ip Server IP address (e.g., "192.168.1.100")
 * @param server_port Server port (e.g., 5000)
 */
void http_client_test_init(const char *server_ip, int server_port);

/**
 * @brief Test simple GET request to server
 * @return HTTP status code, or negative value on error
 */
int http_client_test_ping(void);

/**
 * @brief Request a login token from server
 * @param token_buf Buffer to store token
 * @param buf_len Buffer length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_request_token(char *token_buf, size_t buf_len);

/**
 * @brief Check token login status
 * @param token Token string to check
 * @param status_buf Buffer to store status ("pending", "success", "invalid")
 * @param buf_len Buffer length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_check_status(const char *token, char *status_buf, size_t buf_len);

/**
 * @brief Simple test - request token and check status
 * @param token_out Output token string (caller must free or use static buffer)
 * @return true on success
 */
bool http_client_simple_test(char *token_out);

/**
 * @brief 启动扫码登录流程
 * @param server_ip 服务器IP
 * @param server_port 服务器端口
 * @return true 成功启动
 */
bool http_client_start_login(const char *server_ip, int server_port);

/**
 * @brief 获取当前token
 * @return 当前token字符串
 */
const char* http_client_get_token(void);

/**
 * @brief 检查登录状态
 * @return true 登录成功, false 未登录或检查失败
 */
bool http_client_check_login(void);

/**
 * @brief 检查登录状态并获取用户信息
 * @param result 存储登录结果的结构体指针
 * @return true 登录成功, false 未登录或检查失败
 */
bool http_client_check_login_with_user(login_result_t *result);

/**
 * @brief 获取登录用户的ID
 * @return 用户ID字符串
 */
const char* http_client_get_user_id(void);

/**
 * @brief 获取登录用户的名称
 * @return 用户名字符串
 */
const char* http_client_get_user_name(void);

/**
 * @brief 重置登录状态
 */
void http_client_reset_login(void);

/**
 * @brief 检查是否有有效的token
 * @return true 有有效token, false 没有token或token无效
 */
bool http_client_has_valid_token(void);

////测试排查token空问题
/**
 * @brief 获取最后一次 token 请求的内部状态（用于调试）
 * @param out_content_len 输出 content_length 值
 * @param out_read_len 输出实际读取字节数
 * @param out_buffer 输出前16字节的十六进制字符串（需提供缓冲区）
 * @param buf_size 缓冲区大小
 */
void http_client_get_last_token_status(int *out_content_len, int *out_read_len, char *out_hex, size_t buf_size);

/**
 * @brief 注册token到服务器（ESP32自己生成token方案）
 * @param token 要注册的token
 * @return ESP_OK on success
 */
esp_err_t http_client_register_token(const char *token);

/**
 * @brief 通过 RFID UID 查询服务器登录
 * @param uid RFID卡UID（16进制字符串）
 * @param result 存储登录结果的结构体指针
 * @return ESP_OK 查询成功（不代表找到用户，需判断 result->status）
 */
esp_err_t http_client_check_rfid_login(const char *uid, login_result_t *result);

/**
 * @brief 服务器借书：写入 MySQL tb_borrow，并更新图书状态
 * @param reader_card 当前登录用户卡号（与 tb_reader.card_number 一致）
 * @param book_uid 图书 RFID（与 tb_book.rfid_uid 一致）
 * @param title_out 成功时书名（可为 NULL）
 * @param err_out 失败时错误信息
 */
esp_err_t http_client_borrow_book(const char *reader_card, const char *book_uid,
                                  char *title_out, size_t title_len,
                                  char *err_out, size_t err_len);

/**
 * @brief 服务器还书：更新 tb_borrow 与 tb_book
 */
esp_err_t http_client_return_book(const char *reader_card, const char *book_uid,
                                  char *err_out, size_t err_len);

/* ========== 图书搜索结果 ========== */

/** 搜索结果中的单本图书信息 */
typedef struct {
    int id;
    char isbn[LOCAL_DB_ISBN_LEN + 1];
    char title[LOCAL_DB_TITLE_LEN + 1];
    char author[LOCAL_DB_AUTHOR_LEN + 1];
    char publisher[LOCAL_DB_PUBLISHER_LEN + 1];
    char location[LOCAL_DB_LOCATION_LEN + 1];
    int status;
    char rfid_uid[LOCAL_DB_RFID_UID_LEN + 1];
} http_client_book_t;

/** 搜索结果结构体 */
typedef struct {
    http_client_book_t books[50];
    int count;
} search_result_t;

/**
 * @brief 从服务器搜索图书（支持书名、作者、ISBN模糊搜索）
 * @param keyword 搜索关键词（可为空字符串表示搜索全部）
 * @param result 输出搜索结果结构体
 * @return ESP_OK 成功，其他 错误
 */
esp_err_t http_client_search_books(const char *keyword, search_result_t *result);

/**
 * @brief 按分类精确匹配 tb_book.category（与 /search_by_category 一致）
 */
esp_err_t http_client_search_books_by_category(const char *category, search_result_t *result);

#endif // HTTP_CLIENT_H
