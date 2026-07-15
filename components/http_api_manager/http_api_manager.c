// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "http_api_manager.h"
#include "soil_moisture.h"
#include "bme280_manager.h"
#include "actuator_manager.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_http_server.h>

static const char *TAG = "API_MANAGER";

static httpd_handle_t server_handle = NULL;

// HTTP GET handler for URI: /api/status
static esp_err_t status_get_handler(httpd_req_t *req)
{
    actuator_set_led_blue(false);
    char json_response[128];
    
    // Fetch live data from our soil_moisture component API
    int raw_adc = soil_moisture_get_raw();
    float percentage = soil_moisture_get_percentage();
    
    // Fetch weather metrics
    float temp = 0.0f, hum = 0.0f, press = 0.0f;
    bme280_manager_read(&temp, &hum, &press);
    
    // Format data into a standard JSON payload
    snprintf(json_response, sizeof(json_response),
             "{\"soil\":{\"raw\":%d,\"moisture_pct\":%.2f},\"environment\":{\"temperature_c\":%.2f,\"humidity_pct\":%.2f,\"pressure_hpa\":%.2f}}",
             raw_adc, percentage, temp, hum, press);
    // Set HTTP headers and send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    actuator_set_led_blue(true);
    
    return ESP_OK;
}

// HTTP GET handler for URI: / (HTML Web Dashboard)
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const size_t response_buffer_size = 2048;

    actuator_set_led_blue(false);
    
    float percentage = soil_moisture_get_percentage();
    
    float temp = 0.0f, hum = 0.0f, press = 0.0f;
    bme280_manager_read(&temp, &hum, &press);

    // Allocate safe heap buffer size for the dynamic HTML layout (~1.5 KB)
    char *html_response = malloc(response_buffer_size);
    if (html_response == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTML dashboard response.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory Allocation Failed");
        actuator_set_led_blue(true);
        return ESP_ERR_NO_MEM;
    }

    snprintf(html_response, response_buffer_size,
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>floraguard Dashboard</title>"
        "<style>"
        "body { font-family: system-ui, -apple-system, sans-serif; background: #f0f4f8; color: #102a43; padding: 20px; text-align: center; }"
        ".container { display: flex; flex-wrap: wrap; justify-content: center; max-width: 800px; margin: 0 auto; }"
        ".card { background: white; padding: 24px; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); text-align: left; width: 280px; margin: 15px; border-top: 5px solid #243b53; }"
        ".card.soil { border-top-color: #0b69a3; }"
        ".card.env { border-top-color: #0f8066; }"
        "h1 { color: #0f2942; font-weight: 700; margin-bottom: 5px; }"
        "h2 { margin-top: 0; color: #334e68; font-size: 1.3em; }"
        ".metric { font-size: 1.1em; margin: 12px 0; display: flex; justify-content: space-between; }"
        ".val { font-weight: bold; color: #0b69a3; }"
        ".env .val { color: #0f8066; }"
        ".footer { margin-top: 35px; font-size: 0.85em; color: #627d98; }"
        "</style></head><body>"
        "<h1>floraguard Dashboard</h1>"
        "<div class=\"container\">"
        "<div class=\"card soil\">"
        "<h2>Humidit&eacute; du sol</h2>"
        "<div class=\"metric\"><span>Pourcentage :</span><span class=\"val\">%.2f%%</span></div>"
        "</div>"
        "<div class=\"card env\">"
        "<h2>Environnement</h2>"
        "<div class=\"metric\"><span>Temp&eacute;rature :</span><span class=\"val\">%.2f &deg;C</span></div>"
        "<div class=\"metric\"><span>Humidit&eacute; :</span><span class=\"val\">%.2f%%</span></div>"
        "<div class=\"metric\"><span>Pression :</span><span class=\"val\">%.2f hPa</span></div>"
        "</div>"
        "</div>"
        "<div class=\"footer\">floraguard &bull; C. Mames &bull; GPL v3</div>"
        "</body></html>",
        percentage, temp, hum, press);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    
    free(html_response);
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

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", HTTP_API_PORT);
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle, &status_uri);
        ESP_LOGI(TAG, "API successfully registered on '%s'",HTTP_API_URI);
        if (strcmp(HTTP_API_URI, "/") != 0) {
            httpd_register_uri_handler(server_handle, &root_uri);
            ESP_LOGI(TAG, "HTML Web Dashboard successfully registered on '/'");
        } else {
            ESP_LOGW(TAG, "HTML Web Dashboard disabled to avoid URI conflict with API on '/'");
        }
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to launch HTTP server.");
    return ESP_FAIL;
}