#include "utils/definitions/definitions.h"
#include "tasks/screen_task/screen_task.h"
#include "utils/screen_driver/screen_driver.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"

static const char *TAG = "screen_task";

// Screen task variables
static TaskHandle_t screen_task_handle = NULL;
static SemaphoreHandle_t screen_mutex = NULL;
static screen_mode_t current_mode = SCREEN_MODE_WIFI_STATUS;
static screen_mode_t last_mode = SCREEN_MODE_OFF;
static bool screen_power = true;

// Display data
static char wifi_status[64] = "Connecting...";
static float last_voltage = 0.0f;
static float last_current = 0.0f;
static float last_power = 0.0f;

static uint64_t last_update_time = 0;
static uint64_t update_interval = 2000; // 2 seconds

// Screen task function
static void screen_task(void *pvParameters) {
  ESP_LOGI(TAG, "Screen task started");

  // Initialize screen
  esp_err_t ret = tft_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize screen");
    vTaskDelete(NULL);
    return;
  }
  // tft_display_test_pattern();
  // Initial display
  // vTaskDelay(pdMS_TO_TICKS(100000));
  tft_display_wifi_status("Initializing...", "");

  while (1) {
    if (!screen_power) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // Take mutex to protect shared data
    if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      tft_display_draw_status_bar();

      switch (current_mode) {
      case SCREEN_MODE_WIFI_STATUS:
        if (last_mode != SCREEN_MODE_WIFI_STATUS) {
          tft_display_wifi_status(wifi_status, wifi_ip);
          last_mode = SCREEN_MODE_WIFI_STATUS;
        }
        break;

      case SCREEN_MODE_SENSOR_DATA:
        if (last_mode != SCREEN_MODE_SENSOR_DATA) {
          last_mode = SCREEN_MODE_SENSOR_DATA;
          tft_display_clear_screen();
          tft_display_sensor_data_table(false,last_voltage, last_current, last_power,last_voltage, last_current, last_power,last_voltage, last_current, last_power);
          last_update_time = esp_timer_get_time()/1000;

        }
        if ( (esp_timer_get_time()/1000 - last_update_time) >= update_interval) {
          last_update_time = esp_timer_get_time()/1000;
          // tft_display_sensor_data(last_voltage, last_current, last_power);
          tft_display_sensor_data_table(true,last_voltage, last_current, last_power,last_voltage, last_current, last_power,last_voltage, last_current, last_power);
        }
        break;

      case SCREEN_MODE_AP_CONFIG:
        if (last_mode != SCREEN_MODE_AP_CONFIG) {
          tft_display_ap_info(ap_ssid, ap_password, ap_ip);
          last_mode = SCREEN_MODE_AP_CONFIG;
        }
        break;

      case SCREEN_MODE_OFF:
        if (last_mode != SCREEN_MODE_OFF) {
          tft_fill_screen(0x0000); // Black screen
          last_mode = SCREEN_MODE_OFF;
        }
        break;
      }
      xSemaphoreGive(screen_mutex);
    }

    // Update display every 2 seconds
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Initialize screen task
esp_err_t init_screen_task(void) {
  // Create mutex for thread safety
  screen_mutex = xSemaphoreCreateMutex();
  if (screen_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create screen mutex");
    return ESP_FAIL;
  }

  // Create screen task
  BaseType_t ret = xTaskCreate(screen_task, "screen_task", 4096, NULL, 3,
                               &screen_task_handle);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create screen task");
    vSemaphoreDelete(screen_mutex);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Screen task initialized");
  return ESP_OK;
}

// Set display mode
void screen_set_mode(screen_mode_t mode) {
  if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    current_mode = mode;
    xSemaphoreGive(screen_mutex);
  }
}

// Update WiFi status display
void screen_update_wifi_status(const char *status, const char *ip) {
  if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    strncpy(wifi_status, status, sizeof(wifi_status) - 1);
    wifi_status[sizeof(wifi_status) - 1] = '\0';

    if (ip) {
      strncpy(wifi_ip, ip, sizeof(wifi_ip) - 1);
      wifi_ip[sizeof(wifi_ip) - 1] = '\0';
    } else {
      wifi_ip[0] = '\0';
    }
    xSemaphoreGive(screen_mutex);
  }
}

// Update sensor data display
void screen_update_sensor_data(float voltage, float current, float power) {
  if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    last_voltage = voltage;
    last_current = current;
    last_power = power;
    xSemaphoreGive(screen_mutex);
  }
}

// Update AP config display
void screen_update_ap_config(const char *ssid, const char *password,
                             const char *ip) {
  if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (ssid) {
      strncpy(ap_ssid, ssid, sizeof(ap_ssid) - 1);
      ap_ssid[sizeof(ap_ssid) - 1] = '\0';
    }

    if (password) {
      strncpy(ap_password, password, sizeof(ap_password) - 1);
      ap_password[sizeof(ap_password) - 1] = '\0';
    }

    if (ip) {
      strncpy(ap_ip, ip, sizeof(ap_ip) - 1);
      ap_ip[sizeof(ap_ip) - 1] = '\0';
    }
    xSemaphoreGive(screen_mutex);
  }
}

// Turn screen on/off
void screen_set_power(bool on) {
  if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    screen_power = on;
    if (!on) {
      current_mode = SCREEN_MODE_OFF;
    }
    xSemaphoreGive(screen_mutex);
  }
}

// Test display function
void screen_test_display(void) {
  if (xSemaphoreTake(screen_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    tft_display_test_pattern();
    xSemaphoreGive(screen_mutex);
  }
}
