#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO GPIO_NUM_13
#define TOGGLE_PERIOD_MS 500  // 500ms on + 500ms off = 1Hz

static const char *TAG = "led_toggle_demo";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 LED Toggle Demo Starting...");
    ESP_LOGI(TAG, "LED connected to GPIO %d", LED_GPIO);
    ESP_LOGI(TAG, "Toggle frequency: 1 Hz");

    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", LED_GPIO, esp_err_to_name(ret));
        return;
    }

    // Initialize LED to OFF state
    gpio_set_level(LED_GPIO, 0);
    ESP_LOGI(TAG, "GPIO configured successfully. Starting toggle loop...");

    uint32_t toggle_count = 0;
    bool led_state = false;

    // Main toggle loop
    while (1) {
        led_state = !led_state;
        gpio_set_level(LED_GPIO, led_state);
        
        toggle_count++;
        ESP_LOGI(TAG, "LED %s (toggle count: %lu)", 
                 led_state ? "ON" : "OFF", toggle_count);
        
        vTaskDelay(pdMS_TO_TICKS(TOGGLE_PERIOD_MS));
    }
}