#ifndef SCREEN_TASK_H
#define SCREEN_TASK_H

#include "esp_err.h"
#include <stdbool.h>
// Display modes
typedef enum {
    SCREEN_MODE_WIFI_STATUS,
    SCREEN_MODE_SENSOR_DATA,
    SCREEN_MODE_AP_CONFIG,
    SCREEN_MODE_OFF
} screen_mode_t;

// Initialize screen task
esp_err_t init_screen_task(void);

// Set display mode
void screen_set_mode(screen_mode_t mode);

// Update WiFi status display
void screen_update_wifi_status(const char* status, const char* ip);

// Update sensor data display
void screen_update_sensor_data(float voltage, float current, float power);

// Update AP config display
void screen_update_ap_config(const char* ssid, const char* password, const char* ip);

// Turn screen on/off
void screen_set_power(bool on);

// Test display function
void screen_test_display(void);

#endif // SCREEN_TASK_H
