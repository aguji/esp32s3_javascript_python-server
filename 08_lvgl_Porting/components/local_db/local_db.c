/**
 * @file local_db.c
 * @brief 本地白名单数据库实现
 */
#include "local_db.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 内部借阅记录存储（运行时） */
static local_db_borrow_t s_borrows[LOCAL_DB_MAX_BORROWS];
static int s_borrow_count = 0;

/* 图书状态镜像（用于追踪借出状态） */
static int s_book_status[LOCAL_DB_MAX_BOOKS];

/**
 * @brief 计算天数
 */
static int calculate_days(int year, int month, int day)
{
    int days = year * 365 + (year / 4 - year / 100 + year / 400);
    int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 1; i < month; i++) {
        days += mdays[i];
    }
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
        days += 1;  /* 闰年二月多一天 */
    }
    days += day;
    return days;
}

/**
 * @brief 日期加天数
 */
static void add_days(int year, int month, int day, int add, int *out_year, int *out_month, int *out_day)
{
    int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int current_days = calculate_days(year, month, day);
    int target_days = current_days + add;

    int y = year;
    while (1) {
        int year_days = 365;
        if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) {
            year_days = 366;
        }
        if (target_days <= year_days) {
            break;
        }
        target_days -= year_days;
        y++;
    }

    mdays[2] = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
    int m = 1;
    while (target_days > mdays[m]) {
        target_days -= mdays[m];
        m++;
    }

    *out_year = y;
    *out_month = m;
    *out_day = target_days;
}

esp_err_t local_db_init(void)
{
    /* 初始化图书状态镜像 */
    for (int i = 0; i < g_book_count && i < LOCAL_DB_MAX_BOOKS; i++) {
        s_book_status[i] = g_books[i].status;
    }

    /* 初始化借阅记录为空 */
    s_borrow_count = 0;

    return ESP_OK;
}

int local_db_get_reader_count(void)
{
    return g_reader_count;
}

int local_db_get_book_count(void)
{
    return g_book_count;
}

bool local_db_find_reader_by_rfid(const char *rfid_uid, local_db_reader_t *out_reader)
{
    if (rfid_uid == NULL) {
        return false;
    }

    ESP_LOGI("LOCAL_DB", "local_db_find_reader_by_rfid: searching for '%s' (case-insensitive)", rfid_uid);

    for (int i = 0; i < g_reader_count; i++) {
        int cmp = strcasecmp(g_readers[i].rfid_uid, rfid_uid);
        ESP_LOGI("LOCAL_DB", "  comparing DB[%d]: '%s' vs '%s' = %d", i, g_readers[i].rfid_uid, rfid_uid, cmp);
        if (g_readers[i].rfid_uid[0] != '\0' && cmp == 0) {
            ESP_LOGI("LOCAL_DB", "  MATCH FOUND at index %d!", i);
            if (out_reader != NULL) {
                memcpy(out_reader, &g_readers[i], sizeof(local_db_reader_t));
            }
            return true;
        }
    }
    ESP_LOGW("LOCAL_DB", "local_db_find_reader_by_rfid: NO MATCH for '%s'", rfid_uid);
    return false;
}

bool local_db_verify_reader(const char *card_number, const char *password, local_db_reader_t *out_reader)
{
    if (card_number == NULL || password == NULL) {
        return false;
    }

    for (int i = 0; i < g_reader_count; i++) {
        if (strcmp(g_readers[i].card_number, card_number) == 0) {
            /* 检查状态是否正常 */
            if (g_readers[i].status != READER_STATUS_ACTIVE) {
                return false;
            }
            /* 检查密码 */
            if (strcmp(g_readers[i].password, password) == 0) {
                if (out_reader != NULL) {
                    memcpy(out_reader, &g_readers[i], sizeof(local_db_reader_t));
                }
                return true;
            }
            return false;
        }
    }
    return false;
}

bool local_db_find_reader_by_card_number(const char *card_number, local_db_reader_t *out_reader)
{
    if (card_number == NULL) {
        return false;
    }

    for (int i = 0; i < g_reader_count; i++) {
        if (strcmp(g_readers[i].card_number, card_number) == 0) {
            /* 检查状态是否正常 */
            if (g_readers[i].status != READER_STATUS_ACTIVE) {
                return false;
            }
            if (out_reader != NULL) {
                memcpy(out_reader, &g_readers[i], sizeof(local_db_reader_t));
            }
            return true;
        }
    }
    return false;
}

bool local_db_find_book_by_rfid(const char *rfid_uid, local_db_book_t *out_book)
{
    if (rfid_uid == NULL) {
        ESP_LOGE("LOCAL_DB", "rfid_uid is NULL in local_db_find_book_by_rfid");
        return false;
    }

    ESP_LOGI("LOCAL_DB", "Looking for book with RFID UID: %s (g_book_count=%d)", rfid_uid, g_book_count);
    
    for (int i = 0; i < g_book_count; i++) {
        ESP_LOGI("LOCAL_DB", "Book[%d] RFID UID: %s", i, g_books[i].rfid_uid);
        if (g_books[i].rfid_uid[0] != '\0' &&
            strcasecmp(g_books[i].rfid_uid, rfid_uid) == 0) {
            ESP_LOGI("LOCAL_DB", "Found book: %s (ID: %d)", g_books[i].title, g_books[i].id);
            if (out_book != NULL) {
                memcpy(out_book, &g_books[i], sizeof(local_db_book_t));
                /* 使用运行时状态镜像 */
                out_book->status = s_book_status[i];
            }
            return true;
        }
    }
    
    ESP_LOGE("LOCAL_DB", "Book not found with RFID UID: %s", rfid_uid);
    return false;
}

bool local_db_find_book_by_id(int book_id, local_db_book_t *out_book)
{
    for (int i = 0; i < g_book_count; i++) {
        if (g_books[i].id == book_id) {
            if (out_book != NULL) {
                memcpy(out_book, &g_books[i], sizeof(local_db_book_t));
                /* 使用运行时状态镜像 */
                out_book->status = s_book_status[i];
            }
            return true;
        }
    }
    return false;
}

int local_db_search_books(const char *keyword, local_db_book_t *out_books, int max_books)
{
    if (keyword == NULL || out_books == NULL || max_books <= 0) {
        return 0;
    }

    int found = 0;
    int keyword_len = strlen(keyword);

    for (int i = 0; i < g_book_count && found < max_books; i++) {
        bool match = false;

        /* 匹配书名 */
        if (strstr(g_books[i].title, keyword) != NULL) {
            match = true;
        }
        /* 匹配作者 */
        else if (g_books[i].author[0] != '\0' &&
                 strstr(g_books[i].author, keyword) != NULL) {
            match = true;
        }
        /* 匹配 ISBN */
        else if (g_books[i].isbn[0] != '\0' &&
                 strstr(g_books[i].isbn, keyword) != NULL) {
            match = true;
        }

        if (match) {
            memcpy(&out_books[found], &g_books[i], sizeof(local_db_book_t));
            /* 使用运行时状态镜像 */
            out_books[found].status = s_book_status[i];
            found++;
        }
    }

    return found;
}

esp_err_t local_db_borrow_book(int reader_id, int book_id, int due_days)
{
    /* 验证读者存在且状态正常 */
    local_db_reader_t reader;
    if (!local_db_find_book_by_id(book_id, NULL)) {
        return ESP_FAIL;  /* 图书不存在 */
    }

    /* 检查图书是否可借 */
    if (s_book_status[book_id - 1] == BOOK_STATUS_BORROWED) {
        return ESP_FAIL;  /* 图书已借出 */
    }

    /* 检查是否已有未还的借阅记录 */
    for (int i = 0; i < s_borrow_count; i++) {
        if (s_borrows[i].reader_id == reader_id &&
            s_borrows[i].book_id == book_id &&
            s_borrows[i].status == BORROW_STATUS_BORROWED) {
            return ESP_FAIL;  /* 已借过此书且未还 */
        }
    }

    /* 获取当前时间（这里简化处理，实际应该从 RTC 或 NTP 获取） */
    /* 使用固定的基准时间，实际项目中应替换为真实时间 */
    int borrow_year = 2024;
    int borrow_month = 1;
    int borrow_day = 1;
    int borrow_hour = 12;
    int borrow_minute = 0;

    /* 计算应还日期 */
    int due_year, due_month, due_day;
    add_days(borrow_year, borrow_month, borrow_day, due_days > 0 ? due_days : DEFAULT_BORROW_DAYS,
             &due_year, &due_month, &due_day);

    /* 创建借阅记录 */
    if (s_borrow_count >= LOCAL_DB_MAX_BORROWS) {
        return ESP_ERR_NO_MEM;  /* 记录已满 */
    }

    local_db_borrow_t *record = &s_borrows[s_borrow_count];
    record->id = s_borrow_count + 1;
    record->reader_id = reader_id;
    record->book_id = book_id;
    record->borrow_year = borrow_year;
    record->borrow_month = borrow_month;
    record->borrow_day = borrow_day;
    record->borrow_hour = borrow_hour;
    record->borrow_minute = borrow_minute;
    record->due_year = due_year;
    record->due_month = due_month;
    record->due_day = due_day;
    record->return_year = 0;
    record->return_month = 0;
    record->return_day = 0;
    record->return_hour = 0;
    record->return_minute = 0;
    record->status = BORROW_STATUS_BORROWED;

    s_borrow_count++;

    /* 更新图书状态 */
    s_book_status[book_id - 1] = BOOK_STATUS_BORROWED;

    return ESP_OK;
}

esp_err_t local_db_return_book(int reader_id, int book_id)
{
    /* 查找未归还的借阅记录 */
    for (int i = 0; i < s_borrow_count; i++) {
        if (s_borrows[i].reader_id == reader_id &&
            s_borrows[i].book_id == book_id &&
            s_borrows[i].status == BORROW_STATUS_BORROWED) {

            /* 更新归还时间（简化处理） */
            s_borrows[i].return_year = 2024;
            s_borrows[i].return_month = 1;
            s_borrows[i].return_day = 15;
            s_borrows[i].return_hour = 12;
            s_borrows[i].return_minute = 0;
            s_borrows[i].status = BORROW_STATUS_RETURNED;

            /* 更新图书状态 */
            if (book_id >= 1 && book_id <= g_book_count) {
                s_book_status[book_id - 1] = BOOK_STATUS_AVAILABLE;
            }

            return ESP_OK;
        }
    }

    return ESP_FAIL;  /* 未找到借阅记录 */
}

int local_db_get_current_borrow_count(int reader_id)
{
    int count = 0;
    for (int i = 0; i < s_borrow_count; i++) {
        if (s_borrows[i].reader_id == reader_id &&
            s_borrows[i].status == BORROW_STATUS_BORROWED) {
            count++;
        }
    }
    return count;
}

int local_db_get_borrow_history_count(int reader_id)
{
    int count = 0;
    for (int i = 0; i < s_borrow_count; i++) {
        if (s_borrows[i].reader_id == reader_id) {
            count++;
        }
    }
    return count;
}

int local_db_get_current_borrows(int reader_id, local_db_borrow_t *out_borrows, int max_borrows)
{
    if (out_borrows == NULL || max_borrows <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < s_borrow_count && count < max_borrows; i++) {
        if (s_borrows[i].reader_id == reader_id &&
            s_borrows[i].status == BORROW_STATUS_BORROWED) {
            memcpy(&out_borrows[count], &s_borrows[i], sizeof(local_db_borrow_t));
            count++;
        }
    }
    return count;
}

int local_db_get_borrow_history(int reader_id, local_db_borrow_t *out_borrows, int max_borrows)
{
    if (out_borrows == NULL || max_borrows <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < s_borrow_count && count < max_borrows; i++) {
        if (s_borrows[i].reader_id == reader_id) {
            memcpy(&out_borrows[count], &s_borrows[i], sizeof(local_db_borrow_t));
            count++;
        }
    }
    return count;
}

int local_db_get_all_readers_debug(char *out_buffer, int buffer_size)
{
    if (out_buffer == NULL || buffer_size <= 0) {
        return 0;
    }

    int pos = 0;
    for (int i = 0; i < g_reader_count && pos < buffer_size - 1; i++) {
        int written = snprintf(out_buffer + pos, buffer_size - pos, "[%d]%s ", i, g_readers[i].rfid_uid);
        if (written > 0) {
            pos += written;
        }
    }
    return pos;
}
