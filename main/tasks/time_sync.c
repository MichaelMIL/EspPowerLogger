#include "time_sync.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <time.h>
#include <string.h>
#include <sys/time.h>

static const char *TAG = "time_sync";
static EventGroupHandle_t sntp_event_group;
static const int SNTP_TIME_SYNCED_BIT = BIT0;

// SNTP callback function
static void sntp_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    xEventGroupSetBits(sntp_event_group, SNTP_TIME_SYNCED_BIT);
}

// Initialize SNTP
esp_err_t init_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    
    sntp_event_group = xEventGroupCreate();
    if (sntp_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create SNTP event group");
        return ESP_FAIL;
    }
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_setservername(2, "time.google.com");
    esp_sntp_set_time_sync_notification_cb(sntp_notification_cb);
    esp_sntp_init();
    
    ESP_LOGI(TAG, "SNTP initialized");
    return ESP_OK;
}

// Wait for time synchronization
esp_err_t wait_for_time_sync(uint32_t timeout_ms) {
    ESP_LOGI(TAG, "Waiting for time synchronization...");
    
    EventBits_t bits = xEventGroupWaitBits(sntp_event_group,
                                           SNTP_TIME_SYNCED_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    
    if (bits & SNTP_TIME_SYNCED_BIT) {
        ESP_LOGI(TAG, "Time synchronized successfully");
        
        // Print current time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);
        
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Time synchronization timeout");
        return ESP_FAIL;
    }
}

// Get current time as string
void get_current_time_string(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

// Get current time as ISO 8601 string
void get_current_time_iso_string(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%S", &timeinfo);
}

// Get current timestamp in milliseconds
uint64_t get_current_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Time sync task
static void time_sync_task(void *pvParameters) {
    ESP_LOGI(TAG, "Time sync task started");
    
    // Initialize SNTP
    if (init_sntp() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP");
        vTaskDelete(NULL);
        return;
    }
    
    // Wait for time synchronization (30 seconds timeout)
    if (wait_for_time_sync(30000) == ESP_OK) {
        ESP_LOGI(TAG, "Time synchronization completed successfully");
    } else {
        ESP_LOGW(TAG, "Time synchronization failed, continuing with system time");
    }
    
    // Keep the task running and periodically check time sync
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3600000)); // Check every hour
        
        // Check if time is still synchronized
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // If year is before 2020, time is probably not synchronized
        if (timeinfo.tm_year + 1900 < 2020) {
            ESP_LOGW(TAG, "Time appears to be out of sync, attempting re-sync");
            esp_sntp_restart();
        }
    }
}

// Initialize time sync task
esp_err_t init_time_sync(void) {
    BaseType_t ret = xTaskCreate(time_sync_task, "time_sync_task", 4096, NULL, 3, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create time sync task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Time sync task initialized");
    return ESP_OK;
}

// Stop SNTP
void stop_sntp(void) {
    esp_sntp_stop();
    if (sntp_event_group) {
        vEventGroupDelete(sntp_event_group);
        sntp_event_group = NULL;
    }
    ESP_LOGI(TAG, "SNTP stopped");
}
