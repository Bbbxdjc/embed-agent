#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO 13
#define DOT_DURATION_MS 200
#define DASH_DURATION_MS (DOT_DURATION_MS * 3)
#define SYMBOL_SPACE_MS DOT_DURATION_MS
#define LETTER_SPACE_MS (DOT_DURATION_MS * 3)
#define WORD_SPACE_MS (DOT_DURATION_MS * 7)

static const char *TAG = "SOS_MORSE";

void led_on(void)
{
    gpio_set_level(LED_GPIO, 1);
}

void led_off(void)
{
    gpio_set_level(LED_GPIO, 0);
}

void morse_dot(void)
{
    ESP_LOGI(TAG, "DOT");
    led_on();
    vTaskDelay(pdMS_TO_TICKS(DOT_DURATION_MS));
    led_off();
}

void morse_dash(void)
{
    ESP_LOGI(TAG, "DASH");
    led_on();
    vTaskDelay(pdMS_TO_TICKS(DASH_DURATION_MS));
    led_off();
}

void morse_s(void)
{
    ESP_LOGI(TAG, "Letter: S");
    // S = ... (three dots)
    morse_dot();
    vTaskDelay(pdMS_TO_TICKS(SYMBOL_SPACE_MS));
    morse_dot();
    vTaskDelay(pdMS_TO_TICKS(SYMBOL_SPACE_MS));
    morse_dot();
}

void morse_o(void)
{
    ESP_LOGI(TAG, "Letter: O");
    // O = --- (three dashes)
    morse_dash();
    vTaskDelay(pdMS_TO_TICKS(SYMBOL_SPACE_MS));
    morse_dash();
    vTaskDelay(pdMS_TO_TICKS(SYMBOL_SPACE_MS));
    morse_dash();
}

void morse_sos(void)
{
    ESP_LOGI(TAG, "=== SOS ===");
    
    morse_s();
    vTaskDelay(pdMS_TO_TICKS(LETTER_SPACE_MS));
    
    morse_o();
    vTaskDelay(pdMS_TO_TICKS(LETTER_SPACE_MS));
    
    morse_s();
    vTaskDelay(pdMS_TO_TICKS(WORD_SPACE_MS));
}

void app_main(void)
{
    ESP_LOGI(TAG, "SOS Morse Code Blinker Starting...");
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Ensure LED starts off
    led_off();
    
    ESP_LOGI(TAG, "LED configured on GPIO %d", LED_GPIO);
    ESP_LOGI(TAG, "Dot duration: %d ms, Dash duration: %d ms", DOT_DURATION_MS, DASH_DURATION_MS);
    
    // Continuous SOS loop
    while (1) {
        morse_sos();
    }
}