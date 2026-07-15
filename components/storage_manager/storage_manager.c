// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "storage_manager.h"
#include "log_manager.h"
#include <esp_spiffs.h>

static const char *TAG = "STORAGE";

esp_err_t storage_init(void)
{
    LOG_INFO(TAG, "Mounting storage partition...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            LOG_ERROR(TAG, "Failed to mount or format filesystem.");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            LOG_ERROR(TAG, "Failed to find SPIFFS partition labeled 'storage'.");
        } else {
            LOG_ERROR(TAG, "Failed to initialize SPIFFS (%s).", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        LOG_INFO(TAG, "Partition storage mounted. Size: %d KB, Used: %d KB", total/1024, used/1024);
    }
    return ESP_OK;
}