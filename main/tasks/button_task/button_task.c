#include "button_task.h"
#include "tasks/data_logger/data_logger.h"
#include "utils/definitions/definitions.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>

static const char *TAG = "BUTTON_TASK";

// Button state variables
static bool button_pressed = false;
static uint32_t button_press_time = 0;
static bool button_debounce_active = false;
static uint32_t last_button_time = 0;

void button_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button task started");
    
    // Configure button GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1,  // Enable pull-up resistor
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "Button configured on GPIO %d", BUTTON_GPIO_PIN);
    
    while (1) {
        bool current_state = !gpio_get_level(BUTTON_GPIO_PIN);
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Debounce logic
        if (current_state != button_pressed) {
            if (current_time - last_button_time > BUTTON_DEBOUNCE_MS) {
                button_pressed = current_state;
                last_button_time = current_time;
                
                if (button_pressed) {
                    button_press_time = current_time;
                    button_debounce_active = false;
                    ESP_LOGI(TAG, "Button pressed");
                } else {
                    // Button released - check press duration
                    if (!button_debounce_active) {
                        uint32_t duration = current_time - button_press_time;
                        
                        if (duration >= BUTTON_SHORT_PRESS_MS && duration < BUTTON_LONG_PRESS_MS) {
                            // Short press - toggle logging
                            ESP_LOGI(TAG, "Short press - toggling logging");
                            set_logging_enabled(!g_logging_enabled);
                            ESP_LOGI(TAG, "Logging %s", g_logging_enabled ? "enabled" : "disabled");
                        } else if (duration >= BUTTON_LONG_PRESS_MS) {
                            // Long press - stop logging and create new file
                            ESP_LOGI(TAG, "Long press detected - stopping logging and creating new file");
                            set_logging_enabled(false);
                            esp_err_t ret = create_new_log_file();
                            if (ret == ESP_OK) {
                                ESP_LOGI(TAG, "New log file created successfully");
                            } else {
                                ESP_LOGE(TAG, "Failed to create new log file");
                            }
                        }
                        if (duration >= BUTTON_REBOOT_PRESS_MS) {
                            // Reboot press - reboot device
                            ESP_LOGI(TAG, "Reboot press detected - rebooting device");
                            esp_restart();
                        }
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // Increased delay to reduce CPU usage
    }
}

void init_button_task(void) {
    ESP_LOGI(TAG, "Initializing button task...");
    
    // Create button task
    BaseType_t ret = xTaskCreate(
        button_task,
        "button_task",
        4096,  // Increased stack size to prevent overflow
        NULL,
        2,     // Priority (lower than monitoring task)
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
    } else {
        ESP_LOGI(TAG, "Button task created successfully");
    }
}

bool is_button_pressed(void) {
    return button_pressed;
}
