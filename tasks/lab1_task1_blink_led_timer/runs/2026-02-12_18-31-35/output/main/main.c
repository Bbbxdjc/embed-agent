#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"

static const char *TAG = "DUAL_TIMER_LED";

#define LED1_GPIO 13
#define LED2_GPIO 21

#define TIMER1_INTERVAL_MS 500
#define TIMER2_INTERVAL_MS 1000

// Timer callback for LED1 (500ms)
static bool IRAM_ATTR timer1_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static uint8_t led_state = 0;
    led_state = !led_state;
    gpio_set_level(LED1_GPIO, led_state);
    return false; // Return false to indicate no high priority task woken
}

// Timer callback for LED2 (1000ms)
static bool IRAM_ATTR timer2_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static uint8_t led_state = 0;
    led_state = !led_state;
    gpio_set_level(LED2_GPIO, led_state);
    return false; // Return false to indicate no high priority task woken
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Dual Timer LED Blink");

    // Configure GPIO pins for LEDs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED1_GPIO) | (1ULL << LED2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Initialize both LEDs to OFF
    gpio_set_level(LED1_GPIO, 0);
    gpio_set_level(LED2_GPIO, 0);

    ESP_LOGI(TAG, "LED1 on GPIO %d, LED2 on GPIO %d", LED1_GPIO, LED2_GPIO);

    // Create Timer 1 for LED1 (500ms)
    gptimer_handle_t timer1 = NULL;
    gptimer_config_t timer1_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer1_config, &timer1));

    gptimer_event_callbacks_t timer1_cbs = {
        .on_alarm = timer1_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer1, &timer1_cbs, NULL));

    gptimer_alarm_config_t timer1_alarm_config = {
        .reload_count = 0,
        .alarm_count = TIMER1_INTERVAL_MS * 1000, // Convert ms to us
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer1, &timer1_alarm_config));

    // Create Timer 2 for LED2 (1000ms)
    gptimer_handle_t timer2 = NULL;
    gptimer_config_t timer2_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer2_config, &timer2));

    gptimer_event_callbacks_t timer2_cbs = {
        .on_alarm = timer2_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer2, &timer2_cbs, NULL));

    gptimer_alarm_config_t timer2_alarm_config = {
        .reload_count = 0,
        .alarm_count = TIMER2_INTERVAL_MS * 1000, // Convert ms to us
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer2, &timer2_alarm_config));

    // Enable and start both timers
    ESP_ERROR_CHECK(gptimer_enable(timer1));
    ESP_ERROR_CHECK(gptimer_enable(timer2));
    
    ESP_LOGI(TAG, "Starting timers - LED1: %dms, LED2: %dms", TIMER1_INTERVAL_MS, TIMER2_INTERVAL_MS);
    
    ESP_ERROR_CHECK(gptimer_start(timer1));
    ESP_ERROR_CHECK(gptimer_start(timer2));

    ESP_LOGI(TAG, "Timers started successfully");

    // Main loop - timers handle LED toggling
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}