// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#pragma once

#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Initialize the date time with sntp and configure local timezone.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t initialize_sntp(void);

/**
 * @brief Check if current local time is within daylight hours.
 * @return true if daylight hours, false if nighttime or not sync yet.
 */
bool is_daylight_hours(void);

/**
 * @brief Get the current localized date and time.
 * @return const char* Formatted string "YYYY-MM-DD HH:MM:SS" or default placeholder if unsynced.
 */
const char* sntp_manager_get_datetime(void);