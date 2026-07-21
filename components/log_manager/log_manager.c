// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "log_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define LOG_MAX_HISTORY 5

// 1. Static contiguous allocation: 5 rows of 80 chars = exactly 400 bytes in .bss RAM
static char log_buffer[LOG_MAX_HISTORY][LOG_LINE_MAX_LEN];
static uint8_t moisture_history[MOISTURE_HISTORY_SIZE];
static const char *TAG = "LOG_MANAGER";

esp_err_t log_manager_init(size_t history_size)
{
    memset(moisture_history, 0, sizeof(moisture_history));

    // Initialize the static buffer with default placeholders
    for (size_t i = 0; i < LOG_MAX_HISTORY; i++) {
        snprintf(log_buffer[i], LOG_LINE_MAX_LEN, "---");
    }

    ESP_LOGI(TAG, "Log manager initialized with static buffer (%d slots).", LOG_MAX_HISTORY);
    return ESP_OK;
}

const char* log_manager_get_log(size_t index)
{
    if (index >= LOG_MAX_HISTORY) {
        return NULL;
    }
    return log_buffer[index];
}

size_t log_manager_get_history_size(void)
{
    return LOG_MAX_HISTORY;
}

void log_manager_write_history(const char *prefix, const char *format, ...)
{
    // 1. Shift old logs up to free the last slot (FIFO strategy)
    for (size_t i = 0; i < LOG_MAX_HISTORY - 1; i++) {
        strncpy(log_buffer[i], log_buffer[i + 1], LOG_LINE_MAX_LEN);
    }

    // 2. Target the last row directly
    char *latest_slot = log_buffer[LOG_MAX_HISTORY - 1];
    size_t remaining_space = LOG_LINE_MAX_LEN;

    // 3. Apply prefix tag if available (e.g. "[INF] ")
    int prefix_len = 0;
    if (prefix != NULL) {
        prefix_len = snprintf(latest_slot, remaining_space, "%s", prefix);
        if (prefix_len < 0) prefix_len = 0;
        if (prefix_len >= remaining_space) prefix_len = remaining_space - 1;
    }

    // 4. Parse and resolve variadic format payload using va_list
    va_list args;
    va_start(args, format);
    vsnprintf(latest_slot + prefix_len, remaining_space - prefix_len, format, args);
    va_end(args);

    // Enforce definitive null-termination safety boundary
    latest_slot[LOG_LINE_MAX_LEN - 1] = '\0';
}

void log_manager_add_moisture_sample(uint8_t percentage) {
    // Décalage FIFO
    for (size_t i = 0; i < MOISTURE_HISTORY_SIZE - 1; i++) {
        moisture_history[i] = moisture_history[i + 1];
    }
    moisture_history[MOISTURE_HISTORY_SIZE - 1] = percentage;
}

uint8_t log_manager_get_moisture_sample(size_t index) {
    if (index >= MOISTURE_HISTORY_SIZE) return 0;
    return moisture_history[index];
}