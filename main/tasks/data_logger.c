#include "definitions.h"

#include "data_logger.h"
#include "monitoring_task.h"
#include "time_sync.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "data_logger";
static SemaphoreHandle_t g_log_mutex = NULL;


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
    
    snprintf(g_log_filename, sizeof(g_log_filename), 
             "/spiffs/sensor_log_%04d%02d%02d_%02d%02d%02d.csv",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// Initialize data logger
esp_err_t init_data_logger(void) {
    // Initialize SPIFFS
    esp_err_t ret = init_spiffs();
    if (ret != ESP_OK) {
        return ret;
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
    FILE *f = fopen(g_log_filename, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create log file: %s", g_log_filename);
        return ESP_FAIL;
    }

    fprintf(f, "timestamp,datetime,bus_voltage,shunt_voltage,current,power,raw_bus,raw_shunt,raw_current,raw_power,bus_avg,shunt_avg,current_avg,power_avg\n");
    fclose(f);

    ESP_LOGI(TAG, "Data logger initialized. Log file: %s", g_log_filename);
    return ESP_OK;
}

// Log sensor data to file
void log_sensor_data(const sensor_data_t *data) {
    if (!g_logging_enabled || g_log_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        FILE *f = fopen(g_log_filename, "a");
        if (f != NULL) {
            // Get human-readable datetime
            char datetime_str[32];
            get_current_time_string(datetime_str, sizeof(datetime_str));
            
            fprintf(f, "%llu,\"%s\",%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                    data->timestamp,
                    datetime_str,
                    data->bus_voltage,
                    data->shunt_voltage,
                    data->current,
                    data->power,
                    data->raw_bus,
                    data->raw_shunt,
                    data->raw_current,
                    data->raw_power,
                    data->bus_avg,
                    data->shunt_avg,
                    data->current_avg,
                    data->power_avg);
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
            fprintf(f, "timestamp,datetime,bus_voltage,shunt_voltage,current,power,raw_bus,raw_shunt,raw_current,raw_power,bus_avg,shunt_avg,current_avg,power_avg\n");
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
        generate_log_filename();
        FILE *f = fopen(g_log_filename, "w");
        if (f != NULL) {
            fprintf(f, "timestamp,datetime,bus_voltage,shunt_voltage,current,power,raw_bus,raw_shunt,raw_current,raw_power,bus_avg,shunt_avg,current_avg,power_avg\n");
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
