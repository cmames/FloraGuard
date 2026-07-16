// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "log_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char **log_buffer = NULL;
static size_t max_history_size = 0;
static size_t current_log_count = 0;
static const char *TAG = "LOG_MANAGER";
static uint8_t moisture_history[MOISTURE_HISTORY_SIZE];

esp_err_t log_manager_init(size_t history_size)
{
    memset(moisture_history, 0, sizeof(moisture_history));

    if (history_size == 0) {
        ESP_LOGE(TAG, "Invalid argument, history size");
        return ESP_ERR_INVALID_ARG;
    }

    max_history_size = history_size;

    // Allocate array of pointers (rows)
    log_buffer = (char **)malloc(max_history_size * sizeof(char *));
    if (log_buffer == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    // Allocate memory for each individual log string row
    for (size_t i = 0; i < max_history_size; i++) {
        log_buffer[i] = (char *)malloc(LOG_LINE_MAX_LEN * sizeof(char));
        if (log_buffer[i] == NULL) {
            // Cleanup on partial failure
            for (size_t j = 0; j < i; j++) {
                free(log_buffer[j]);
            }
            free(log_buffer);
            log_buffer = NULL;
            ESP_LOGE(TAG, "Out of memory");
            return ESP_ERR_NO_MEM;
        }
        // Initialize with default placeholder text
        snprintf(log_buffer[i], LOG_LINE_MAX_LEN, "---");
    }

    current_log_count = 0;
    return ESP_OK;
}

const char* log_manager_get_log(size_t index)
{
    if (log_buffer == NULL || index >= max_history_size) {
        return NULL;
    }
    return log_buffer[index];
}

size_t log_manager_get_history_size(void)
{
    return max_history_size;
}

void log_manager_write_history(const char *prefix, const char *format, ...)
{
    if (log_buffer == NULL) {
        return;
    }

    // 1. Shift old logs up to free the last slot (FIFO strategy)
    for (size_t i = 0; i < max_history_size - 1; i++) {
        strncpy(log_buffer[i], log_buffer[i + 1], LOG_LINE_MAX_LEN);
    }

    // 2. Prepare destination buffer pointers
    char *latest_slot = log_buffer[max_history_size - 1];
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