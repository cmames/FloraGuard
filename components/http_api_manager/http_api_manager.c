// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "http_api_manager.h"
#include "soil_moisture.h"
#include "bme280_manager.h"
#include "actuator_manager.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "log_manager.h"
#include <esp_http_server.h>

static const char *TAG = "API_MANAGER";

static httpd_handle_t server_handle = NULL;

// HTTP GET handler for URI: /api/status
static esp_err_t status_get_handler(httpd_req_t *req)
{
    actuator_set_led_blue(false);
    
    int raw_adc = soil_moisture_get_raw();
    float percentage = soil_moisture_get_percentage();
    
    float temp = 0.0f, hum = 0.0f, press = 0.0f;
    bme280_manager_read(&temp, &hum, &press);
    
    // Allocate a buffer safe enough to hold metrics + the 5 formatted log lines
    #define JSON_BUFFER_SIZE (MOISTURE_HISTORY_SIZE * 4 + 1024)
    static char json_response[JSON_BUFFER_SIZE];
    
    // Format core sensor values
    int written = snprintf(json_response, JSON_BUFFER_SIZE,
             "{\"soil\":{\"raw\":%d,\"moisture_pct\":%.2f},"
             "\"environment\":{\"temperature_c\":%.2f,\"humidity_pct\":%.2f,\"pressure_hpa\":%.2f},"
             "\"moisture_history\":[",
             raw_adc, percentage, temp, hum, press);

    // Append the soil moisture history
    for (size_t i = 0; i < MOISTURE_HISTORY_SIZE; i++) {
        const char *separator = (i == MOISTURE_HISTORY_SIZE - 1) ? "" : ",";
        if (written < JSON_BUFFER_SIZE) {
            written += snprintf(json_response + written, JSON_BUFFER_SIZE - written,
                                "%d%s", log_manager_get_moisture_sample(i), separator);
        }
    }

    // Format logs array
    if (written < JSON_BUFFER_SIZE) {
        written += snprintf(json_response + written, JSON_BUFFER_SIZE - written, "],\"logs\":[");
    }

    // Append the log history strings as a raw JSON array
    size_t log_size = log_manager_get_history_size();
    for (size_t i = 0; i < log_size; i++) {
        const char *line = log_manager_get_log(i);
        if (line != NULL && written < JSON_BUFFER_SIZE) {
            // Check if it's the last element to handle the trailing comma cleanly
            const char *separator = (i == log_size - 1) ? "" : ",";
            written += snprintf(json_response + written, JSON_BUFFER_SIZE - written,
                                "\"%s\"%s", line, separator);
        }
    }

    // Close JSON payload
    if (written < JSON_BUFFER_SIZE) {
        snprintf(json_response + written, JSON_BUFFER_SIZE - written, "]}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    
    actuator_set_led_blue(true);
    
    return ESP_OK;
}

// HTTP GET handler for URI: / (HTML Web Dashboard)
static esp_err_t root_get_handler(httpd_req_t *req)
{
    actuator_set_led_blue(false);

    // Open the static HTML file from our mounted partition
    FILE *f = fopen("/spiffs/index.html", "r");
    if (f == NULL) {
        LOG_ERROR(TAG, "Failed to open index.html from storage.");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Dashboard File Not Found");
        actuator_set_led_blue(true);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");

    // Stream the file chunk by chunk to maintain a near-zero RAM footprint
    char chunk[256];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            fclose(f);
            LOG_ERROR(TAG, "Abort: Critical error during HTML chunk streaming.");
            actuator_set_led_blue(true);
            return ESP_FAIL;
        }
    }
    
    // Close file descriptor and signal end of response chunks
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    
    actuator_set_led_blue(true);
    return ESP_OK;
}

// URI structure mapping for the HTTP server
static const httpd_uri_t status_uri = {
    .uri       = HTTP_API_URI,
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

// Route structure mapping for the HTML Root address
static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

// Start the lightweight HTTP server
esp_err_t api_webserver_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.server_port=HTTP_API_PORT;

    LOG_INFO(TAG, "Starting HTTP server on port: '%d'", HTTP_API_PORT);
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle, &status_uri);
        LOG_INFO(TAG, "API successfully registered on '%s'",HTTP_API_URI);
        if (strcmp(HTTP_API_URI, "/") != 0) {
            httpd_register_uri_handler(server_handle, &root_uri);
            LOG_INFO(TAG, "HTML Web Dashboard successfully registered on '/'");
        } else {
            LOG_WARN(TAG, "HTML Web Dashboard disabled to avoid URI conflict with API on '/'");
        }
        return ESP_OK;
    }

    LOG_ERROR(TAG, "Failed to launch HTTP server.");
    return ESP_FAIL;
}