/**
 * @file local_db.h
 * @brief 本地白名单数据库头文件
 *
 * 本模块提供纯本地的读者和图书数据存储，无需网络连接
 * 数据按照 mysql_gradata.md 中的数据库结构设计
 */
#ifndef LOCAL_DB_H
#define LOCAL_DB_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 常量定义 ========== */
/* 读者相关 */
#define LOCAL_DB_MAX_READERS      20      /* 最大读者数量 */
#define LOCAL_DB_CARD_NUMBER_LEN  20      /* 卡号长度 */
#define LOCAL_DB_PASSWORD_LEN     128     /* 密码长度 */
#define LOCAL_DB_NAME_LEN          50      /* 姓名长度 */
#define LOCAL_DB_RFID_UID_LEN      20      /* RFID UID 长度 */
#define LOCAL_DB_PHONE_LEN        20      /* 电话长度 */

/* 图书相关 */
#define LOCAL_DB_MAX_BOOKS        50      /* 最大图书数量 */
#define LOCAL_DB_ISBN_LEN         20      /* ISBN 长度 */
#define LOCAL_DB_TITLE_LEN        100     /* 书名长度 */
#define LOCAL_DB_AUTHOR_LEN       50      /* 作者长度 */
#define LOCAL_DB_PUBLISHER_LEN    50      /* 出版社长度 */
#define LOCAL_DB_LOCATION_LEN     50      /* 位置长度 */

/* 借阅相关 */
#define LOCAL_DB_MAX_BORROWS      100     /* 最大借阅记录数量 */

/* 状态定义（与 mysql_gradata.md 一致） */
#define READER_STATUS_ACTIVE      0       /* 读者状态：正常 */
#define READER_STATUS_DISABLED    1       /* 读者状态：禁用 */

#define BOOK_STATUS_AVAILABLE     0       /* 与 MySQL tb_book.status 一致：可借阅 */
#define BOOK_STATUS_BORROWED      1       /* 与 MySQL tb_book.status 一致：已借出 */

#define BORROW_STATUS_BORROWED    0       /* 借阅状态：借出中 */
#define BORROW_STATUS_RETURNED    1       /* 借阅状态：已归还 */

/* 默认借期天数 */
#define DEFAULT_BORROW_DAYS       14

/* ========== 数据结构 ========== */

/* 读者记录 */
typedef struct {
    int id;                             /* 自增主键 */
    char card_number[LOCAL_DB_CARD_NUMBER_LEN + 1];  /* 读者卡号 */
    char password[LOCAL_DB_PASSWORD_LEN + 1];        /* 登录密码 */
    char name[LOCAL_DB_NAME_LEN + 1];                 /* 姓名 */
    char rfid_uid[LOCAL_DB_RFID_UID_LEN + 1];         /* RFID 卡 UID */
    char phone[LOCAL_DB_PHONE_LEN + 1];               /* 联系电话 */
    int status;                         /* 状态：0=正常，1=禁用 */
    int is_admin;                       /* 是否管理员：0=否，1=是 */
    int created_year;                   /* 创建年份 */
    int created_month;                  /* 创建月份 */
    int created_day;                    /* 创建日 */
} local_db_reader_t;

/* 图书记录 */
typedef struct {
    int id;                             /* 自增主键 */
    char isbn[LOCAL_DB_ISBN_LEN + 1];               /* ISBN */
    char title[LOCAL_DB_TITLE_LEN + 1];            /* 书名 */
    char author[LOCAL_DB_AUTHOR_LEN + 1];          /* 作者 */
    char publisher[LOCAL_DB_PUBLISHER_LEN + 1];    /* 出版社 */
    char location[LOCAL_DB_LOCATION_LEN + 1];       /* 书架位置 */
    char rfid_uid[LOCAL_DB_RFID_UID_LEN + 1];       /* 图书 RFID 标签 UID */
    int status;                         /* 状态：0=可借，1=已借出 */
    int created_year;                   /* 入库年份 */
    int created_month;                  /* 入库月份 */
    int created_day;                    /* 入库日 */
} local_db_book_t;

/* 借阅记录 */
typedef struct {
    int id;                             /* 自增主键 */
    int reader_id;                      /* 读者ID */
    int book_id;                        /* 图书ID */
    int borrow_year;                    /* 借阅年份 */
    int borrow_month;                   /* 借阅月份 */
    int borrow_day;                     /* 借阅日 */
    int borrow_hour;                    /* 借阅小时 */
    int borrow_minute;                  /* 借阅分钟 */
    int due_year;                       /* 应还年份 */
    int due_month;                      /* 应还月份 */
    int due_day;                        /* 应还日 */
    int return_year;                    /* 实际归还年份 */
    int return_month;                   /* 实际归还月份 */
    int return_day;                     /* 实际归还日 */
    int return_hour;                    /* 实际归还小时 */
    int return_minute;                  /* 实际归还分钟 */
    int status;                         /* 状态：0=借出中，1=已归还 */
} local_db_borrow_t;

/* ========== 外部数据（由 local_db_data.c 提供） ========== */
extern const local_db_reader_t g_readers[];
extern const int g_reader_count;
extern const local_db_book_t g_books[];
extern const int g_book_count;

/* ========== API 函数 ========== */

/**
 * @brief 初始化本地数据库
 * @return ESP_OK 成功
 */
esp_err_t local_db_init(void);

/**
 * @brief 获取所有读者数量
 * @return 读者数量
 */
int local_db_get_reader_count(void);

/**
 * @brief 获取所有图书数量
 * @return 图书数量
 */
int local_db_get_book_count(void);

/**
 * @brief 根据 RFID UID 查找读者
 * @param rfid_uid RFID 卡 UID
 * @param out_reader 输出读者信息（可为 NULL）
 * @return true 找到，false 未找到
 */
bool local_db_find_reader_by_rfid(const char *rfid_uid, local_db_reader_t *out_reader);

/**
 * @brief 根据卡号和密码验证读者登录
 * @param card_number 读者卡号
 * @param password 密码
 * @param out_reader 输出读者信息（可为 NULL）
 * @return true 登录成功，false 登录失败
 */
bool local_db_verify_reader(const char *card_number, const char *password, local_db_reader_t *out_reader);

/**
 * @brief 根据卡号查找读者（不需要密码验证）
 * @param card_number 读者卡号
 * @param out_reader 输出读者信息（可为 NULL）
 * @return true 找到，false 未找到
 */
bool local_db_find_reader_by_card_number(const char *card_number, local_db_reader_t *out_reader);

/**
 * @brief 根据图书 RFID UID 查找图书
 * @param rfid_uid 图书 RFID UID
 * @param out_book 输出图书信息（可为 NULL）
 * @return true 找到，false 未找到
 */
bool local_db_find_book_by_rfid(const char *rfid_uid, local_db_book_t *out_book);

/**
 * @brief 根据图书 ID 查找图书
 * @param book_id 图书 ID
 * @param out_book 输出图书信息（可为 NULL）
 * @return true 找到，false 未找到
 */
bool local_db_find_book_by_id(int book_id, local_db_book_t *out_book);

/**
 * @brief 根据关键词搜索图书（书名、作者、ISBN）
 * @param keyword 关键词
 * @param out_books 输出图书数组
 * @param max_books 最大输出数量
 * @return 实际匹配数量
 */
int local_db_search_books(const char *keyword, local_db_book_t *out_books, int max_books);

/**
 * @brief 执行借书操作
 * @param reader_id 读者 ID
 * @param book_id 图书 ID
 * @param due_days 借期天数（默认14天）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t local_db_borrow_book(int reader_id, int book_id, int due_days);

/**
 * @brief 执行还书操作
 * @param reader_id 读者 ID
 * @param book_id 图书 ID
 * @return ESP_OK 成功，其他失败
 */
esp_err_t local_db_return_book(int reader_id, int book_id);

/**
 * @brief 获取读者的当前借阅数量
 * @param reader_id 读者 ID
 * @return 当前借阅数量
 */
int local_db_get_current_borrow_count(int reader_id);

/**
 * @brief 获取读者的借阅历史数量
 * @param reader_id 读者 ID
 * @return 历史借阅总数量
 */
int local_db_get_borrow_history_count(int reader_id);

/**
 * @brief 获取读者的当前借阅记录
 * @param reader_id 读者 ID
 * @param out_borrows 输出借阅记录数组
 * @param max_borrows 最大输出数量
 * @return 实际借阅数量
 */
int local_db_get_current_borrows(int reader_id, local_db_borrow_t *out_borrows, int max_borrows);

/**
 * @brief 获取读者的借阅历史记录
 * @param reader_id 读者 ID
 * @param out_borrows 输出借阅记录数组
 * @param max_borrows 最大输出数量
 * @return 实际记录数量
 */
int local_db_get_borrow_history(int reader_id, local_db_borrow_t *out_borrows, int max_borrows);

/**
 * @brief 获取所有读者的 RFID UID 列表（用于调试显示）
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字符数
 */
int local_db_get_all_readers_debug(char *out_buffer, int buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* LOCAL_DB_H */
