// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "log_manager.h"
#include "storage_manager.h"
#include "soil_moisture.h"
#include "bme280_manager.h"
#include "actuator_manager.h"
#include "wifi_manager.h"
#include "http_api_manager.h"
#include "sntp_manager.h"

#ifdef RUN_CALIBRATION_MODE
#include "calibration.h"
#endif  

static const char *TAG = "MAIN";

static bool moisture_sensor_failure_tripped = false;

static void watering(float val) {
    LOG_INFO(TAG, "- moisture %.2f%%, watering for %d ms", val, IRRIGATION_PUMP_ON_MS);
    actuator_set_pump(true);
    vTaskDelay(pdMS_TO_TICKS(IRRIGATION_PUMP_ON_MS)); // watering
    actuator_set_pump(false);
    LOG_INFO(TAG, "- Wait %d seconds for the water to soak into the soil", (IRRIGATION_PUMP_OFF_MS/1000));
    vTaskDelay(pdMS_TO_TICKS(IRRIGATION_PUMP_OFF_MS)); // infiltration
}

static void automation_task(void *pvParameters)
{
    LOG_INFO(TAG, "Control loop automation task started.");

    while (1) {
        is_daylight_hours();
        int raw_moisture = soil_moisture_get_raw();
        float moisture_pct = soil_moisture_get_percentage();

        log_manager_add_moisture_sample((uint8_t)moisture_pct);
        
        // Hardware sensor safety check
        if (raw_moisture <= 0 || raw_moisture >= 4095) {
            LOG_ERROR(TAG, "Critical: Soil moisture sensor fault detected (Raw: %d)!", raw_moisture);
            actuator_set_pump(false);
            actuator_set_led_yellow(false);
            actuator_set_led_red(true);
        } else {
            // Test if previous error
            if (moisture_sensor_failure_tripped) {
                LOG_ERROR(TAG, "Watering is limited due to possible hardware failure. System requires a reset.");
                for (int i=0; i<10; i++) {
                    actuator_set_led_red(false);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    actuator_set_led_red(true);
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }

            // Agricultural control thresholds
            if (moisture_pct > SOIL_MOISTURE_CRITICAL_HIGH) {
                LOG_WARN(TAG, "Warning: Soil moisture anomaly (>%.0f%%): %.2f%%", SOIL_MOISTURE_CRITICAL_HIGH, moisture_pct);
                actuator_set_pump(false);
                actuator_set_led_red(false);
                actuator_set_led_yellow(true);
            } else {
                if (moisture_pct < SOIL_MOISTURE_CRITICAL_LOW) {
                    LOG_WARN(TAG, "Alert: Soil dry (<%.0f%%): %.2f%%", SOIL_MOISTURE_CRITICAL_LOW, moisture_pct);
                    actuator_set_led_red(false);
                    actuator_set_led_yellow(true);

                    // Conditional validation using our sntp_manager API
                    if (is_daylight_hours()) {
                        LOG_INFO(TAG, "Daylight active. Starting a watering cycle...");

                        int safety_counter = 0;
                        const int MAX_WATERING_ATTEMPTS = 10;

                        do {
                            watering(moisture_pct);
                            if (++safety_counter >= MAX_WATERING_ATTEMPTS) {
                                LOG_ERROR(TAG, "Watering safety timeout reached! Locking system.");
                                moisture_sensor_failure_tripped = true;
                                actuator_set_pump(false);
                            }
                            moisture_pct = soil_moisture_get_percentage();
                        } while ((WATER_UNTIL_SATURATION == 1) && (!moisture_sensor_failure_tripped) && (moisture_pct < SOIL_MOISTURE_CRITICAL_HIGH));
                        LOG_INFO(TAG, "Watering %d times", safety_counter);

                    } else {
                        LOG_INFO(TAG, "Nighttime restriction active. Postponing irrigation.");
                        actuator_set_pump(false);
                        vTaskDelay(pdMS_TO_TICKS(1800000));
                    }
                } 
                else {
                    // Safe zone
                    actuator_set_pump(false);
                    actuator_set_led_red(false);
                    actuator_set_led_yellow(false);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void floraguard(void) 
{
    // Initialize the dynamic log buffer to hold the 5 last lines
    ESP_ERROR_CHECK(log_manager_init(5));
    LOG_INFO(TAG, "FloraGuard logs layer dynamic initialization successful.");

    if (storage_init() != ESP_OK) {
        LOG_ERROR(TAG, "Critical: Filesystem storage mount failed. UI offline.");
    }
    
    if (actuator_manager_init() != ESP_OK) {
        LOG_ERROR(TAG, "Aborting: Actuator framework setup failed.");
        return;
    }

    // Power-On Self-Test (POST) visual check
    actuator_set_led_blue(true);
    actuator_set_led_yellow(true);
    actuator_set_led_red(true);
    vTaskDelay(pdMS_TO_TICKS(1000));
    actuator_set_led_blue(false);
    actuator_set_led_yellow(false);
    actuator_set_led_red(false);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (soil_moisture_init() != ESP_OK) {
        LOG_ERROR(TAG, "Aborting: Soil moisture hardware setup failed.");
        return;
    }

    if (bme280_manager_init() != ESP_OK) {
        LOG_WARN(TAG, "BME280 setup failed. Running without atmospheric metrics.");
    }

    if (wifi_start(WIFI_SSID, WIFI_PASSWORD, MDNS_HOSTNAME) != ESP_OK) {
        LOG_ERROR(TAG, "Network layer failed to start. Running local mode.");
    } else {
        actuator_set_led_blue(true);
        initialize_sntp();
        if (api_webserver_start() != ESP_OK) {
            LOG_ERROR(TAG, "Critical: Failed to launch local REST API. Web services offline.");
            for(int i=0;i<5;i++) {
                actuator_set_led_blue(false);
                actuator_set_led_red(true);
                vTaskDelay(pdMS_TO_TICKS(500));
                actuator_set_led_blue(true);
                actuator_set_led_red(false);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }

    LOG_INFO(TAG, "floraguard firmware fully operational. Launching automation scheduler...");

    xTaskCreatePinnedToCore(automation_task, "automation_task", 4096, NULL, 5, NULL, 1);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
#ifdef RUN_CALIBRATION_MODE
    LOG_WARN("ROUTER", "Calibration mode activated.");
    run_calibration();
#else
    LOG_INFO("ROUTER", "Standard application mode starting...");
    floraguard();
#endif
}