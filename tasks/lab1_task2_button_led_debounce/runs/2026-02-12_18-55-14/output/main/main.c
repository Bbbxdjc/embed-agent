#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BUZZER_GPIO     GPIO_NUM_13
#define BUTTON_GPIO     GPIO_NUM_21

static const char *TAG = "doorbell";

// ISR handler for button press
static void IRAM_ATTR button_isr_handler(void* arg)
{
    // Read the current state of the button
    int button_state = gpio_get_level(BUTTON_GPIO);
    
    // When button is pressed (LOW due to pull-up), turn buzzer ON
    // When button is released (HIGH), turn buzzer OFF
    if (button_state == 0) {
        gpio_set_level(BUZZER_GPIO, 1);  // Buzzer ON
    } else {
        gpio_set_level(BUZZER_GPIO, 0);  // Buzzer OFF
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 Doorbell System Starting...");
    
    // Configure buzzer GPIO as output
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(BUZZER_GPIO, 0);  // Initialize buzzer to OFF
    
    // Configure button GPIO as input with pull-up and interrupt on both edges
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE  // Trigger on both rising and falling edges
    };
    gpio_config(&button_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Attach the interrupt handler to the button GPIO
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    
    ESP_LOGI(TAG, "Doorbell system initialized");
    ESP_LOGI(TAG, "Button GPIO: %d, Buzzer GPIO: %d", BUTTON_GPIO, BUZZER_GPIO);
    ESP_LOGI(TAG, "Press the button to activate the buzzer");
    
    // Main loop - just keep the task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}