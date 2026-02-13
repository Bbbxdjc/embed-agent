#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "DOORBELL";

// GPIO Pin Definitions
#define BUTTON_GPIO     21
#define BUZZER_GPIO     13

// Debounce Configuration
#define DEBOUNCE_TIME_US    50000  // 50ms debounce time

// Queue for interrupt events
static QueueHandle_t gpio_evt_queue = NULL;

// Debounce tracking
static volatile int64_t last_interrupt_time = 0;

// Button state captured in ISR
typedef struct {
    uint32_t gpio_num;
    int level;
} gpio_event_t;

// ISR Handler - Captures state at interrupt time
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    int64_t current_time = esp_timer_get_time();
    
    // Debounce check
    if ((current_time - last_interrupt_time) < DEBOUNCE_TIME_US) {
        return;  // Ignore this interrupt (bounce)
    }
    
    last_interrupt_time = current_time;
    
    // Capture the button state at the moment of interrupt
    gpio_event_t evt;
    evt.gpio_num = gpio_num;
    evt.level = gpio_get_level(gpio_num);
    
    // Send event to queue from ISR
    xQueueSendFromISR(gpio_evt_queue, &evt, NULL);
}

// Task to handle GPIO events
static void gpio_task(void* arg)
{
    gpio_event_t evt;
    
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &evt, portMAX_DELAY)) {
            // Control buzzer based on captured button state
            if (evt.level == 1) {
                // Button pressed (HIGH due to pull-down)
                ESP_LOGI(TAG, "Button pressed - Buzzer ON");
                gpio_set_level(BUZZER_GPIO, 1);
            } else {
                // Button released (LOW)
                ESP_LOGI(TAG, "Button released - Buzzer OFF");
                gpio_set_level(BUZZER_GPIO, 0);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Doorbell System Initializing...");
    
    // Configure Button GPIO (Input with pull-down already external)
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // External pull-down
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
    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_event_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }
    
    // Create task to handle GPIO events
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Attach interrupt handler to button GPIO
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);
    
    ESP_LOGI(TAG, "Doorbell System Ready");
    ESP_LOGI(TAG, "Button GPIO: %d, Buzzer GPIO: %d", BUTTON_GPIO, BUZZER_GPIO);
    
    // Main loop - can be used for other tasks
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}