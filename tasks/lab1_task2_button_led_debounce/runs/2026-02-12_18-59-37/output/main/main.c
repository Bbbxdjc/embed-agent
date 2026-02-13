#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define BUTTON_GPIO     GPIO_NUM_21
#define BUZZER_GPIO     GPIO_NUM_13
#define DEBOUNCE_TIME_US 50000  // 50ms debounce time

static const char *TAG = "DOORBELL";
static QueueHandle_t gpio_evt_queue = NULL;
static volatile int64_t last_interrupt_time = 0;

// ISR handler for button press
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int64_t current_time = esp_timer_get_time();
    
    // Debounce check
    if ((current_time - last_interrupt_time) > DEBOUNCE_TIME_US) {
        last_interrupt_time = current_time;
        uint32_t gpio_num = (uint32_t) arg;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
}

// Task to handle GPIO events
static void gpio_task(void* arg)
{
    uint32_t io_num;
    int button_state;
    
    while(1) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // Read current button state
            button_state = gpio_get_level(BUTTON_GPIO);
            
            if (button_state == 0) {
                // Button pressed (assuming active low with pull-up)
                ESP_LOGI(TAG, "Button pressed - Buzzer ON");
                gpio_set_level(BUZZER_GPIO, 1);
            } else {
                // Button released
                ESP_LOGI(TAG, "Button released - Buzzer OFF");
                gpio_set_level(BUZZER_GPIO, 0);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Doorbell system initializing...");
    
    // Configure buzzer GPIO as output
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(BUZZER_GPIO, 0);  // Initially off
    
    // Configure button GPIO as input with pull-up and interrupt on both edges
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE  // Trigger on both rising and falling edges
    };
    gpio_config(&button_conf);
    
    // Create queue for GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    
    // Create task to handle GPIO events
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Attach interrupt handler to button GPIO
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);
    
    ESP_LOGI(TAG, "Doorbell system ready. Press button on GPIO %d", BUTTON_GPIO);
    ESP_LOGI(TAG, "Buzzer connected to GPIO %d", BUZZER_GPIO);
    
    // Main loop - can be used for other tasks
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}