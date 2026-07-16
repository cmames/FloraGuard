// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include "sntp_manager.h"

#define LOG_LINE_MAX_LEN 80

/**
 * @brief Initialize the dynamic log history buffer.
 * @param history_size Maximum number of log lines to retain.
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if allocation fails.
 */
esp_err_t log_manager_init(size_t history_size);

/**
 * @brief Retrieve a pointer to a specific log line from the history.
 * @param index The log index (0 being the oldest, history_size - 1 being the newest).
 * @return const char* Pointer to the log string, or NULL if index out of bounds or empty.
 */
const char* log_manager_get_log(size_t index);

/**
 * @brief Get the configured maximum history size.
 * @return size_t The history size.
 */
size_t log_manager_get_history_size(void);

/**
 * @brief Backend variadic formatting function. Do not call directly.
 */
void log_manager_write_history(const char *prefix, const char *format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief Unified logging macros injecting the dynamic datetime stamp automatically.
 */
#define LOG_ERROR(tag, format, ...) do { \
    ESP_LOGE(tag, "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__); \
    log_manager_write_history("[ERR] ", "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__); \
} while(0)

#define LOG_WARN(tag, format, ...) do { \
    ESP_LOGW(tag, "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__); \
    log_manager_write_history("[WRN] ", "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__); \
} while(0)

#define LOG_INFO(tag, format, ...) do { \
    ESP_LOGI(tag, "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__); \
    log_manager_write_history("[INF] ", "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__); \
} while(0)

#define LOG_DEBUG(tag, format, ...)   ESP_LOGD(tag, "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__)
#define LOG_VERBOSE(tag, format, ...) ESP_LOGV(tag, "[%s] " format, sntp_manager_get_datetime(), ##__VA_ARGS__)
