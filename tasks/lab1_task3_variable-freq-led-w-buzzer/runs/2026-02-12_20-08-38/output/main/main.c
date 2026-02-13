#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BUTTON_GPIO     21
#define BUZZER_GPIO     13
#define LED_GPIO        9

#define BUZZER_DURATION_MS  100

static const char *TAG = "BUTTON_LED_BUZZER";

// Timer handle for LED toggling
static TimerHandle_t led_timer = NULL;

// State variables
static volatile uint8_t button_state = 0;  // 0: 1Hz, 1: 2Hz, 2: 4Hz, 3: OFF
static volatile bool led_state = false;

// LED timer callback function
static void led_timer_callback(TimerHandle_t xTimer)
{
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state);
    ESP_LOGI(TAG, "LED toggled: %s", led_state ? "ON" : "OFF");
}

// Button ISR handler
static void IRAM_ATTR button_isr_handler(void *arg)
{
    // Trigger task notification to handle button press in task context
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    TaskHandle_t task_handle = (TaskHandle_t)arg;
    vTaskNotifyGiveFromISR(task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task to handle button press
static void button_task(void *arg)
{
    while (1) {
        // Wait for notification from ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Debounce delay
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Check if button is still pressed
        if (gpio_get_level(BUTTON_GPIO) == 1) {
            ESP_LOGI(TAG, "Button pressed! State: %d", button_state);
            
            // Activate buzzer
            gpio_set_level(BUZZER_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(BUZZER_DURATION_MS));
            gpio_set_level(BUZZER_GPIO, 0);
            
            // Stop current timer if running
            if (led_timer != NULL && xTimerIsTimerActive(led_timer)) {
                xTimerStop(led_timer, 0);
            }
            
            // Update state and configure timer based on button press count
            switch (button_state) {
                case 0:  // 1st press - 1 Hz (500ms period for toggle)
                    ESP_LOGI(TAG, "Setting LED to 1 Hz");
                    xTimerChangePeriod(led_timer, pdMS_TO_TICKS(500), 0);
                    xTimerStart(led_timer, 0);
                    break;
                    
                case 1:  // 2nd press - 2 Hz (250ms period for toggle)
                    ESP_LOGI(TAG, "Setting LED to 2 Hz");
                    xTimerChangePeriod(led_timer, pdMS_TO_TICKS(250), 0);
                    xTimerStart(led_timer, 0);
                    break;
                    
                case 2:  // 3rd press - 4 Hz (125ms period for toggle)
                    ESP_LOGI(TAG, "Setting LED to 4 Hz");
                    xTimerChangePeriod(led_timer, pdMS_TO_TICKS(125), 0);
                    xTimerStart(led_timer, 0);
                    break;
                    
                case 3:  // 4th press - Stop LED
                    ESP_LOGI(TAG, "Stopping LED");
                    gpio_set_level(LED_GPIO, 0);
                    led_state = false;
                    break;
            }
            
            // Increment state and wrap around
            button_state = (button_state + 1) % 4;
            
            // Wait for button release
            while (gpio_get_level(BUTTON_GPIO) == 1) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            vTaskDelay(pdMS_TO_TICKS(50));  // Additional debounce
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Button-LED-Buzzer Control System");
    
    // Configure LED GPIO
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_GPIO, 0);
    
    // Configure Buzzer GPIO
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(BUZZER_GPIO, 0);
    
    // Configure Button GPIO with interrupt on rising edge
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // External pull-down
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&button_conf);
    
    // Create button handling task
    TaskHandle_t button_task_handle = NULL;
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, &button_task_handle);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void *)button_task_handle);
    
    // Create LED timer (initially with 500ms period for 1Hz)
    led_timer = xTimerCreate("led_timer", pdMS_TO_TICKS(500), pdTRUE, NULL, led_timer_callback);
    
    if (led_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create LED timer");
        return;
    }
    
    ESP_LOGI(TAG, "System initialized. Press button to control LED frequency.");
    ESP_LOGI(TAG, "Button: GPIO%d, LED: GPIO%d, Buzzer: GPIO%d", BUTTON_GPIO, LED_GPIO, BUZZER_GPIO);
}