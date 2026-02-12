#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO GPIO_NUM_13
#define TOGGLE_PERIOD_MS 500  // 500ms ON + 500ms OFF = 1Hz

static const char *TAG = "LED_TOGGLE";

void app_main(void)
{
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Starting LED toggle demo at 1Hz on GPIO %d", LED_GPIO);

    uint8_t led_state = 0;

    while (1) {
        // Toggle LED state
        led_state = !led_state;
        gpio_set_level(LED_GPIO, led_state);
        
        ESP_LOGI(TAG, "LED state: %s", led_state ? "ON" : "OFF");
        
        // Delay for half period to achieve 1Hz frequency
        vTaskDelay(pdMS_TO_TICKS(TOGGLE_PERIOD_MS));
    }
}