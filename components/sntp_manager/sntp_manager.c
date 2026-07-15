// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "sntp_manager.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "log_manager.h"

static const char *TAG = "SNTP_MANAGER";

esp_err_t initialize_sntp(void)
{
    LOG_INFO(TAG, "Initializing SNTP client targeting pool.ntp.org...");
    
    // Configure operating mode and associate reference server
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Configure timezone explicitly for France (Paris) handling DST automatically
    setenv("TZ", SNTP_TIMEZONE, 1);
    tzset();

    return ESP_OK;
}

bool is_daylight_hours(void)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);

    // If the system year is lower than 2026, NTP synchronization has not occurred yet
    if (timeinfo.tm_year < (2026 - 1900)) {
        LOG_WARN(TAG, "NTP time synchronization pending. Night restriction active by default.");
        return false;
    }

    LOG_INFO(TAG, "Current localized time: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Agricultural authorization slot: 08:00 to 19:59
    return (timeinfo.tm_hour >= DAYLIGHT_START_HOUR && timeinfo.tm_hour < DAYLIGHT_END_HOUR);
}