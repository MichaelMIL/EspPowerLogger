#include "utils/definitions/definitions.h"

#include "tasks/data_logger/data_logger.h"
#include "tasks/monitoring_task/monitoring_task.h"
#include "tasks/time_sync/time_sync.h"
#include "utils/sdcard_driver/sdcard_driver.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

static const char *TAG = "data_logger";
static SemaphoreHandle_t g_log_mutex = NULL;
static storage_type_t g_current_storage = STORAGE_SPIFFS;


// Initialize SPIFFS filesystem for logging
esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

// Generate log filename with timestamp
void generate_log_filename(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    const char* base_path = (g_current_storage == STORAGE_SDCARD) ? "/sdcard" : "/spiffs";
    
    // Create directory path: /base_path/YYYY_MM_DD/
    char dir_path[128];
    int dir_result = snprintf(dir_path, sizeof(dir_path), 
                             "%s/%04d%02d%02d",
                             base_path,
                             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    if (dir_result >= sizeof(dir_path)) {
        ESP_LOGE(TAG, "Directory path too long, truncating");
        dir_path[sizeof(dir_path) - 1] = '\0';
    }
    
    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        // Create directories recursively
        char temp_path[128];
        strcpy(temp_path, base_path);
        
        // Create directory
        snprintf(temp_path, sizeof(temp_path), "%s/%04d%02d%02d", base_path,  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        if (stat(temp_path, &st) == -1) {
            mkdir(temp_path, 0755);
        }
    
    }
    
    // Create filename with time: HHMMSS.csv
    int result = snprintf(g_log_filename, sizeof(g_log_filename), 
                         "%s/%02d%02d%02d.csv",
                         dir_path,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    if (result >= sizeof(g_log_filename)) {
        ESP_LOGE(TAG, "Filename too long, truncating");
        g_log_filename[sizeof(g_log_filename) - 1] = '\0';
    }
    
    ESP_LOGI(TAG, "Generated filename: %s (length: %d)", g_log_filename, result);
}

// Initialize data logger
esp_err_t init_data_logger(void) {
    esp_err_t ret;
    
    // Always initialize SPIFFS first as fallback
    ESP_LOGI(TAG, "Initializing SPIFFS as fallback storage...");
    ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS fallback");
        return ret;
    }
    ESP_LOGI(TAG, "SPIFFS initialized successfully");
    
    // Try to initialize SD card
    ESP_LOGI(TAG, "Attempting to initialize SD card...");
    ret = init_sdcard();
    if (ret == ESP_OK && is_sdcard_available()) {
        g_current_storage = STORAGE_SDCARD;
        ESP_LOGI(TAG, "Using SD card for data logging");
    } else {
        // Use SPIFFS as primary storage
        g_current_storage = STORAGE_SPIFFS;
        ESP_LOGI(TAG, "Using SPIFFS for data logging");
    }

    // Create mutex for thread safety
    g_log_mutex = xSemaphoreCreateMutex();
    if (g_log_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create log mutex");
        return ESP_FAIL;
    }

    // Generate initial log filename
    generate_log_filename();

    // Create CSV header
    ESP_LOGI(TAG, "Attempting to create log file: %s", g_log_filename);
    
    // Check if SD card is still available
    if (g_current_storage == STORAGE_SDCARD && !is_sdcard_available()) {
        ESP_LOGE(TAG, "SD card is no longer available, falling back to SPIFFS");
        g_current_storage = STORAGE_SPIFFS;
        generate_log_filename();  // Regenerate filename for SPIFFS
        ESP_LOGI(TAG, "New log file path: %s", g_log_filename);
    }
    
    FILE *f = fopen(g_log_filename, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create log file: %s (errno: %d)", g_log_filename, errno);
        // Try to fallback to SPIFFS if SD card fails
        if (g_current_storage == STORAGE_SDCARD) {
            ESP_LOGW(TAG, "SD card file creation failed, falling back to SPIFFS");
            g_current_storage = STORAGE_SPIFFS;
            generate_log_filename();
            ESP_LOGI(TAG, "Retrying with SPIFFS: %s", g_log_filename);
            f = fopen(g_log_filename, "w");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to create log file in SPIFFS: %s (errno: %d)", g_log_filename, errno);
                return ESP_FAIL;
            }
        } else {
            return ESP_FAIL;
        }
    }

    fprintf(f, "timestamp,datetime,sensor1_bus_voltage,sensor1_shunt_voltage,sensor1_current,sensor1_power,sensor1_raw_bus,sensor1_raw_shunt,sensor1_raw_current,sensor1_raw_power,sensor1_bus_avg,sensor1_shunt_avg,sensor1_current_avg,sensor1_power_avg,sensor2_bus_voltage,sensor2_shunt_voltage,sensor2_current,sensor2_power,sensor2_raw_bus,sensor2_raw_shunt,sensor2_raw_current,sensor2_raw_power,sensor2_bus_avg,sensor2_shunt_avg,sensor2_current_avg,sensor2_power_avg\n");
    fclose(f);

    ESP_LOGI(TAG, "Data logger initialized. Storage: %s, Log file: %s", 
             get_storage_type_string(), g_log_filename);
    return ESP_OK;
}

// Log sensor data to file
void log_sensor_data(const sensor_data_t *data) {
    if (!g_logging_enabled || g_log_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Check if storage has changed and switch if needed
        check_and_switch_storage();
        
        FILE *f = fopen(g_log_filename, "a");
        if (f != NULL) {
            // Get human-readable datetime
            char datetime_str[32];
            get_current_time_string(datetime_str, sizeof(datetime_str));
            
            fprintf(f, "%llu,\"%s\",%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                    data->timestamp,
                    datetime_str,
                    data->sensor1.bus_voltage,
                    data->sensor1.shunt_voltage,
                    data->sensor1.current,
                    data->sensor1.power,
                    data->sensor1.raw_bus,
                    data->sensor1.raw_shunt,
                    data->sensor1.raw_current,
                    data->sensor1.raw_power,
                    data->sensor1.bus_avg,
                    data->sensor1.shunt_avg,
                    data->sensor1.current_avg,
                    data->sensor1.power_avg,
                    data->sensor2.bus_voltage,
                    data->sensor2.shunt_voltage,
                    data->sensor2.current,
                    data->sensor2.power,
                    data->sensor2.raw_bus,
                    data->sensor2.raw_shunt,
                    data->sensor2.raw_current,
                    data->sensor2.raw_power,
                    data->sensor2.bus_avg,
                    data->sensor2.shunt_avg,
                    data->sensor2.current_avg,
                    data->sensor2.power_avg);
            fclose(f);
        }
        xSemaphoreGive(g_log_mutex);
    }
}

// Enable/disable logging
void set_logging_enabled(bool enabled) {
    g_logging_enabled = enabled;
    ESP_LOGI(TAG, "Data logging %s", enabled ? "enabled" : "disabled");
}

// Get logging status
bool is_logging_enabled(void) {
    return g_logging_enabled;
}

// Get current log filename
const char* get_log_filename(void) {
    return g_log_filename;
}

// Get log file size
size_t get_log_file_size(void) {
    if (g_log_mutex == NULL) {
        return 0;
    }

    size_t size = 0;
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        struct stat st;
        if (stat(g_log_filename, &st) == 0) {
            size = st.st_size;
        }
        xSemaphoreGive(g_log_mutex);
    }
    return size;
}

// Clear log file
esp_err_t clear_log_file(void) {
    if (g_log_mutex == NULL) {
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        FILE *f = fopen(g_log_filename, "w");
        if (f != NULL) {
            fprintf(f, "timestamp,datetime,sensor1_bus_voltage,sensor1_shunt_voltage,sensor1_current,sensor1_power,sensor1_raw_bus,sensor1_raw_shunt,sensor1_raw_current,sensor1_raw_power,sensor1_bus_avg,sensor1_shunt_avg,sensor1_current_avg,sensor1_power_avg,sensor2_bus_voltage,sensor2_shunt_voltage,sensor2_current,sensor2_power,sensor2_raw_bus,sensor2_raw_shunt,sensor2_raw_current,sensor2_raw_power,sensor2_bus_avg,sensor2_shunt_avg,sensor2_current_avg,sensor2_power_avg\n");
            fclose(f);
            ESP_LOGI(TAG, "Log file cleared");
        } else {
            ESP_LOGE(TAG, "Failed to clear log file");
            ret = ESP_FAIL;
        }
        xSemaphoreGive(g_log_mutex);
    } else {
        ret = ESP_FAIL;
    }
    return ret;
}

// Create new log file
esp_err_t create_new_log_file(void) {
    if (g_log_mutex == NULL) {
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Check if storage has changed (e.g., SD card inserted)
        check_and_switch_storage();
        
        generate_log_filename();
        FILE *f = fopen(g_log_filename, "w");
        if (f != NULL) {
            fprintf(f, "timestamp,datetime,sensor1_bus_voltage,sensor1_shunt_voltage,sensor1_current,sensor1_power,sensor1_raw_bus,sensor1_raw_shunt,sensor1_raw_current,sensor1_raw_power,sensor1_bus_avg,sensor1_shunt_avg,sensor1_current_avg,sensor1_power_avg,sensor2_bus_voltage,sensor2_shunt_voltage,sensor2_current,sensor2_power,sensor2_raw_bus,sensor2_raw_shunt,sensor2_raw_current,sensor2_raw_power,sensor2_bus_avg,sensor2_shunt_avg,sensor2_current_avg,sensor2_power_avg\n");
            fclose(f);
            ESP_LOGI(TAG, "New log file created: %s", g_log_filename);
        } else {
            ESP_LOGE(TAG, "Failed to create new log file");
            ret = ESP_FAIL;
        }
        xSemaphoreGive(g_log_mutex);
    } else {
        ret = ESP_FAIL;
    }
    return ret;
}

// Get current storage type
storage_type_t get_current_storage_type(void) {
    return g_current_storage;
}

// Get storage type as string
const char* get_storage_type_string(void) {
    switch (g_current_storage) {
        case STORAGE_SDCARD:
            return "SD Card";
        case STORAGE_SPIFFS:
            return "SPIFFS";
        default:
            return "Unknown";
    }
}

// Check and switch storage if needed
esp_err_t check_and_switch_storage(void) {
    storage_type_t desired_storage = is_sdcard_available() ? STORAGE_SDCARD : STORAGE_SPIFFS;
    
    // If storage type has changed, switch
    if (desired_storage != g_current_storage) {
        ESP_LOGI(TAG, "Storage type changed from %s to %s", 
                 get_storage_type_string(), 
                 desired_storage == STORAGE_SDCARD ? "SD Card" : "SPIFFS");
        
        g_current_storage = desired_storage;
        
        // Generate new filename for the new storage
        generate_log_filename();
        
        // Create new log file with header
        FILE *f = fopen(g_log_filename, "w");
        if (f != NULL) {
            fprintf(f, "timestamp,datetime,sensor1_bus_voltage,sensor1_shunt_voltage,sensor1_current,sensor1_power,sensor1_raw_bus,sensor1_raw_shunt,sensor1_raw_current,sensor1_raw_power,sensor1_bus_avg,sensor1_shunt_avg,sensor1_current_avg,sensor1_power_avg,sensor2_bus_voltage,sensor2_shunt_voltage,sensor2_current,sensor2_power,sensor2_raw_bus,sensor2_raw_shunt,sensor2_raw_current,sensor2_raw_power,sensor2_bus_avg,sensor2_shunt_avg,sensor2_current_avg,sensor2_power_avg\n");
            fclose(f);
            ESP_LOGI(TAG, "Switched to %s, new log file: %s", 
                     get_storage_type_string(), g_log_filename);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to create new log file after storage switch: %s (errno: %d)", 
                     g_log_filename, errno);
            return ESP_FAIL;
        }
    }
    
    return ESP_OK;
}
