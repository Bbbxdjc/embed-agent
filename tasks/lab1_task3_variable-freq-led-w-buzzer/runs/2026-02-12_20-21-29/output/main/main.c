#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"

#define BUTTON_GPIO     21
#define BUZZER_GPIO     13
#define LED_GPIO        9

#define BUZZER_DURATION_MS  100

static const char *TAG = "BUTTON_LED_BUZZER";

// Global variables
static gptimer_handle_t led_timer = NULL;
static uint8_t button_press_count = 0;
static volatile uint32_t last_interrupt_time = 0;
static const uint32_t debounce_time_ms = 50;

// LED timer frequencies (in microseconds for alarm period)
static const uint64_t timer_periods[] = {
    500000,   // 1 Hz (500ms for toggle = 1Hz blink)
    250000,   // 2 Hz (250ms for toggle = 2Hz blink)
    125000,   // 4 Hz (125ms for toggle = 4Hz blink)
    0         // Timer stopped
};

// LED timer callback
static bool IRAM_ATTR led_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static int led_state = 0;
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state);
    return false;
}

// Function to configure and start LED timer
static void configure_led_timer(uint8_t mode)
{
    if (led_timer != NULL) {
        gptimer_stop(led_timer);
        gptimer_disable(led_timer);
        gptimer_del_timer(led_timer);
        led_timer = NULL;
        gpio_set_level(LED_GPIO, 0);
    }

    if (mode < 3) {  // modes 0, 1, 2 (1Hz, 2Hz, 4Hz)
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1000000,  // 1MHz, 1 tick = 1us
        };
        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &led_timer));

        gptimer_event_callbacks_t cbs = {
            .on_alarm = led_timer_callback,
        };
        ESP_ERROR_CHECK(gptimer_register_event_callbacks(led_timer, &cbs, NULL));

        gptimer_alarm_config_t alarm_config = {
            .reload_count = 0,
            .alarm_count = timer_periods[mode],
            .flags.auto_reload_on_alarm = true,
        };
        ESP_ERROR_CHECK(gptimer_set_alarm_action(led_timer, &alarm_config));

        ESP_ERROR_CHECK(gptimer_enable(led_timer));
        ESP_ERROR_CHECK(gptimer_start(led_timer));
        
        ESP_LOGI(TAG, "LED timer started at mode %d", mode);
    } else {
        ESP_LOGI(TAG, "LED timer stopped");
    }
}

// Function to activate buzzer
static void activate_buzzer(void)
{
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(BUZZER_DURATION_MS));
    gpio_set_level(BUZZER_GPIO, 0);
}

// Button ISR handler
static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    
    // Debouncing
    if ((current_time - last_interrupt_time) > debounce_time_ms) {
        last_interrupt_time = current_time;
        
        // Increment button press count
        button_press_count = (button_press_count + 1) % 4;
        
        // Send notification to task
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        static TaskHandle_t main_task_handle = NULL;
        if (main_task_handle == NULL) {
            main_task_handle = xTaskGetHandle("main_task");
        }
        if (main_task_handle != NULL) {
            vTaskNotifyGiveFromISR(main_task_handle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// Main task
void main_task(void *pvParameters)
{
    while (1) {
        // Wait for button press notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Activate buzzer
        activate_buzzer();
        
        // Configure LED timer based on button press count
        configure_led_timer(button_press_count);
        
        ESP_LOGI(TAG, "Button pressed, count: %d", button_press_count);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Button LED Buzzer Control");

    // Configure LED GPIO
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_GPIO, 0);

    // Configure Buzzer GPIO
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(BUZZER_GPIO, 0);

    // Configure Button GPIO with interrupt
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // External pull-down
        .intr_type = GPIO_INTR_POSEDGE,  // Trigger on rising edge
    };
    gpio_config(&button_conf);

    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

    // Create main task
    xTaskCreate(main_task, "main_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "System initialized and ready");
}