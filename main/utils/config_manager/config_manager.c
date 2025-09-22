#include "../definitions/definitions.h"
#include "../config_manager/config_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "config_manager";
static const char *NVS_NAMESPACE = "power_monitor";



// Global configuration structure
static config_data_t g_config = {
    .log_interval_ms = DEFAULT_LOG_INTERVAL_MS,
    .wifi_ssid = {0},
    .wifi_password = {0}
};

// Initialize configuration manager
esp_err_t init_config_manager(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load configuration from NVS
    load_config();
    
    ESP_LOGI(TAG, "Configuration manager initialized");
    return ESP_OK;
}

// Load configuration from NVS
esp_err_t load_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS handle for reading: %s", esp_err_to_name(ret));
        // Use default values
        strcpy(g_config.wifi_ssid, DEFAULT_WIFI_SSID);
        strcpy(g_config.wifi_password, DEFAULT_WIFI_PASS);
        return ESP_OK;
    }

    // Load log interval
    size_t required_size = sizeof(uint32_t);
    ret = nvs_get_blob(nvs_handle, "log_interval", &g_config.log_interval_ms, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read log_interval from NVS, using default");
        g_config.log_interval_ms = DEFAULT_LOG_INTERVAL_MS;
    }

    // Load WiFi SSID
    required_size = MAX_SSID_LEN;
    ret = nvs_get_str(nvs_handle, "wifi_ssid", g_config.wifi_ssid, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read wifi_ssid from NVS, using default");
        strcpy(g_config.wifi_ssid, DEFAULT_WIFI_SSID);
    }

    // Load WiFi password
    required_size = MAX_PASS_LEN;
    ret = nvs_get_str(nvs_handle, "wifi_password", g_config.wifi_password, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read wifi_password from NVS, using default");
        strcpy(g_config.wifi_password, DEFAULT_WIFI_PASS);
    }

    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Configuration loaded: log_interval=%d, ssid=%s", 
             g_config.log_interval_ms, g_config.wifi_ssid);
    
    return ESP_OK;
}

// Save configuration to NVS
esp_err_t save_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing: %s", esp_err_to_name(ret));
        return ret;
    }

    // Save log interval
    ret = nvs_set_blob(nvs_handle, "log_interval", &g_config.log_interval_ms, sizeof(uint32_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save log_interval: %s", esp_err_to_name(ret));
    }

    // Save WiFi SSID
    ret = nvs_set_str(nvs_handle, "wifi_ssid", g_config.wifi_ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save wifi_ssid: %s", esp_err_to_name(ret));
    }

    // Save WiFi password
    ret = nvs_set_str(nvs_handle, "wifi_password", g_config.wifi_password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save wifi_password: %s", esp_err_to_name(ret));
    }

    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved successfully");
    }
    
    return ret;
}

// Get current configuration
const config_data_t* get_config(void) {
    return &g_config;
}

// Set log interval
esp_err_t set_log_interval(uint32_t interval_ms) {
    if (interval_ms < 100 || interval_ms > 60000) {
        ESP_LOGE(TAG, "Invalid log interval: %d ms (must be 100-60000)", interval_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_config.log_interval_ms = interval_ms;
    ESP_LOGI(TAG, "Log interval set to %d ms", interval_ms);
    return ESP_OK;
}

// Set WiFi credentials
esp_err_t set_wifi_credentials(const char* ssid, const char* password) {
    if (ssid == NULL || password == NULL) {
        ESP_LOGE(TAG, "SSID and password cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(ssid) >= MAX_SSID_LEN) {
        ESP_LOGE(TAG, "SSID too long (max %d chars)", MAX_SSID_LEN - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(password) >= MAX_PASS_LEN) {
        ESP_LOGE(TAG, "Password too long (max %d chars)", MAX_PASS_LEN - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(g_config.wifi_ssid, ssid);
    strcpy(g_config.wifi_password, password);
    
    ESP_LOGI(TAG, "WiFi credentials updated: SSID=%s", ssid);
    return ESP_OK;
}

// Update configuration and save
esp_err_t update_config(const config_data_t* new_config) {
    if (new_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate log interval
    if (new_config->log_interval_ms < 100 || new_config->log_interval_ms > 60000) {
        ESP_LOGE(TAG, "Invalid log interval: %d ms", new_config->log_interval_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate SSID length
    if (strlen(new_config->wifi_ssid) >= MAX_SSID_LEN) {
        ESP_LOGE(TAG, "SSID too long");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate password length
    if (strlen(new_config->wifi_password) >= MAX_PASS_LEN) {
        ESP_LOGE(TAG, "Password too long");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update configuration
    g_config.log_interval_ms = new_config->log_interval_ms;
    strcpy(g_config.wifi_ssid, new_config->wifi_ssid);
    strcpy(g_config.wifi_password, new_config->wifi_password);
    
    // Save to NVS
    esp_err_t ret = save_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration");
        return ret;
    }
    
    ESP_LOGI(TAG, "Configuration updated and saved");
    return ESP_OK;
}
