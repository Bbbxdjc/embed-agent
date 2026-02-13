#include <stdio.h>
#include "freertos/FreeRtos.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "DOORBELL";

// GPIO Pin Definitions
#define BUTTON_GPIO     GPIO_NUM_21
#define BUZZER_GPIO     GPIO_NUM_13

// Debounce Configuration
#define DEBOUNCE_TIME_US    50000  // 50ms debounce time

// Queue for interrupt events
static QueueHandle_t gpio_evt_queue = NULL;

// Debounce tracking
static volatile int64_t last_interrupt_time = 0;

// ISR Handler - Captures button state at interrupt time
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int64_t current_time = esp_timer_get_time();
    
    // Debounce check
    if ((current_time - last_interrupt_time) < DEBOUNCE_TIME_US) {
        return;  // Ignore this interrupt (bounce)
    }
    
    last_interrupt_time = current_time;
    
    // Capture the button state at the moment of interrupt
    int button_state = gpio_get_level(BUTTON_GPIO);
    
    // Send button state to queue
    xQueueSendFromISR(gpio_evt_queue, &button_state, NULL);
}

// Task to handle buzzer control
static void buzzer_control_task(void* arg)
{
    int button_state;
    
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &button_state, portMAX_DELAY)) {
            if (button_state == 0) {
                // Button pressed (assuming active LOW with pull-up)
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
    ESP_LOGI(TAG, "ESP32-S3 Doorbell System Starting...");
    
    // Configure Button GPIO (Input with pull-up, interrupt on both edges)
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE  // Trigger on both rising and falling edges
    };
    gpio_config(&button_conf);
    
    // Configure Buzzer GPIO (Output)
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&buzzer_conf);
    
    // Initialize buzzer to OFF state
    gpio_set_level(BUZZER_GPIO, 0);
    
    // Create queue for GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(int));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Attach interrupt handler to button GPIO
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL);
    
    // Create task to handle buzzer control
    xTaskCreate(buzzer_control_task, "buzzer_control_task", 2048, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Doorbell system initialized. Press button on GPIO %d", BUTTON_GPIO);
    ESP_LOGI(TAG, "Buzzer connected to GPIO %d", BUZZER_GPIO);
    
    // Main loop - can be used for other tasks or monitoring
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}