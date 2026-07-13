#include "calibration.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>

static const char *TAG = "MOISTURE_CALIB";

#define SAMPLE_DELAY_MS       500

void run_calibration(void)
{
    // 1. Configure ADC Unit
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = MOISTURE_ADC_UNIT,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2. Configure ADC Channel with 12dB attenuation for full 3.3V range
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // 12 bits resolution (0 - 4095)
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MOISTURE_ADC_CHANNEL, &config));

    ESP_LOGI(TAG, "Moisture sensor calibration tool initialized.");
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "1. Leave the sensor dry in the air to get MAX_VAL (Dry value).");
    ESP_LOGI(TAG, "2. Dip the sensor in water up to the max line to get MIN_VAL (Wet value).");
    ESP_LOGI(TAG, "--------------------------------------------------");

    int raw_output;
    int min=4095;
    int max=0;

    while (1) {
        // Read raw analog value
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, MOISTURE_ADC_CHANNEL, &raw_output));
        
        if (raw_output>max) max=raw_output;
        if (raw_output<min) min=raw_output;

        ESP_LOGI(TAG, "Raw ADC Value: %d [ %d , %d ]", raw_output, min, max);
        
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }

    // Clean up (unreachable here, but good practice)
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}