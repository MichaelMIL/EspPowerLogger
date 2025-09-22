#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Configuration data structure
typedef struct {
    uint32_t log_interval_ms;      // Logging interval in milliseconds
    char wifi_ssid[32];            // WiFi SSID
    char wifi_password[64];        // WiFi password
} config_data_t;

// Initialize configuration manager
esp_err_t init_config_manager(void);

// Load configuration from NVS
esp_err_t load_config(void);

// Save configuration to NVS
esp_err_t save_config(void);

// Get current configuration
const config_data_t* get_config(void);

// Set log interval
esp_err_t set_log_interval(uint32_t interval_ms);

// Set WiFi credentials
esp_err_t set_wifi_credentials(const char* ssid, const char* password);

// Update configuration and save
esp_err_t update_config(const config_data_t* new_config);

#endif // CONFIG_MANAGER_H
