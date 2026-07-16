// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "sntp_manager.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "log_manager.h"

static const char *TAG = "SNTP_MANAGER";

/**
 * @brief Background task responsible for monitoring and retrying NTP sync if it fails.
 */
static void ntp_sync_task(void *pvParameters)
{
    LOG_INFO(TAG, "NTP monitoring task started.");
    
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // If the year is 2026 or greater, synchronization is successful
        if (timeinfo.tm_year >= (2026 - 1900)) {
            char datetime_buf[32];
            strftime(datetime_buf, sizeof(datetime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
            LOG_INFO(TAG, "NTP Synchronization successful! Current time: %s", datetime_buf);
            
            // Task self-destruction to free FreeRTOS resources permanently
            vTaskDelete(NULL);
        }

        // If we reach here, synchronization failed or is still pending
        LOG_WARN(TAG, "NTP synchronization pending or failed. Retrying sync request...");
        
        // Restart the ESP-SNTP service to force a fresh network query
        if (esp_sntp_enabled()) {
            esp_sntp_stop();
        }
        esp_sntp_init();

        // Wait 5 seconds before checking or retrying again
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t initialize_sntp(void)
{
    LOG_INFO(TAG, "Initializing SNTP client targeting pool.ntp.org...");
    
    // Configure operating mode and associate reference server
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    // Configure timezone explicitly for France (Paris) handling DST automatically
    setenv("TZ", SNTP_TIMEZONE, 1);
    tzset();

    // Launch the background resilience task (Stack size: 2560 bytes, low priority)
    xTaskCreate(ntp_sync_task, "ntp_sync_task", 2560, NULL, 3, NULL);

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

    // Agricultural authorization slot: 08:00 to 19:59
    return (timeinfo.tm_hour >= DAYLIGHT_START_HOUR && timeinfo.tm_hour < DAYLIGHT_END_HOUR);
}

const char* sntp_manager_get_datetime(void)
{
    static char datetime_buf[32];
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);

    // If the system year is lower than 2026, NTP synchronization has not occurred yet
    if (timeinfo.tm_year < (2026 - 1900)) {
        return "0000-00-00 00:00:00";
    }

    // strftime handles tm offsets internally and is optimized to bypass snprintf truncation warnings
    strftime(datetime_buf, sizeof(datetime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return datetime_buf;
}