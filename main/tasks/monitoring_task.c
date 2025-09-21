#include "monitoring_task.h"
#include "../definitions.h"
#include "ina219.h"
#include "data_logger.h"
#include "config_manager.h"
#include "screen_task.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <stdio.h>

// Global sensor data structure
sensor_data_t g_sensor_data;
SemaphoreHandle_t g_sensor_data_mutex;

void monitoring_task(void *pvParameters) {
    printf("Monitoring task started\n");
    
    // Initialize sensor data
    g_sensor_data.bus_voltage = 0.0f;
    g_sensor_data.shunt_voltage = 0.0f;
    g_sensor_data.current = 0.0f;
    g_sensor_data.power = 0.0f;
    g_sensor_data.raw_bus = 0;
    g_sensor_data.raw_shunt = 0;
    g_sensor_data.raw_current = 0;
    g_sensor_data.raw_power = 0;
    g_sensor_data.timestamp = 0;
    
    // Initialize filtered values
    static float bus_avg = 0, shunt_avg = 0, current_avg = 0, power_avg = 0;
    static int first_read = 1;
    
    while (1) {
        // Read raw values with small delays for stability
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t raw_bus = ina219_getBusVoltage_raw(&ina219);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t raw_shunt = ina219_getShuntVoltage_raw(&ina219);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t raw_current = ina219_getCurrent_raw(&ina219);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t raw_power = ina219_getPower_raw(&ina219);
        
        // Convert to float values using proper conversion factors
        float bus_voltage = raw_bus * 0.004f;    // Convert from 4mV units to volts
        float shunt_voltage = raw_shunt * 0.01f; // Convert to millivolts
        float current = (raw_current / 10.0f) - 6.0;     // Convert using current divider (10mA per bit)
        float power = raw_power * 2.0f;          // Convert using power multiplier (2mW per bit)
        
        // Apply filtering (simple exponential moving average)
        if (first_read) {
            bus_avg = bus_voltage;
            shunt_avg = shunt_voltage;
            current_avg = current;
            power_avg = power;
            first_read = 0;
        } else {
            // Simple exponential moving average (alpha = 0.3)
            bus_avg = 0.7f * bus_avg + 0.3f * bus_voltage;
            shunt_avg = 0.7f * shunt_avg + 0.3f * shunt_voltage;
            current_avg = 0.7f * current_avg + 0.3f * current;
            power_avg = 0.7f * power_avg + 0.3f * power;
        }
        
        // Update global sensor data with mutex protection
        if (xSemaphoreTake(g_sensor_data_mutex, portMAX_DELAY) == pdTRUE) {
            g_sensor_data.bus_voltage = bus_voltage;
            g_sensor_data.shunt_voltage = shunt_voltage;
            g_sensor_data.current = current;
            g_sensor_data.power = power;
            g_sensor_data.raw_bus = raw_bus;
            g_sensor_data.raw_shunt = raw_shunt;
            g_sensor_data.raw_current = raw_current;
            g_sensor_data.raw_power = raw_power;
            g_sensor_data.timestamp = esp_timer_get_time() / 1000; // Convert to milliseconds
            g_sensor_data.bus_avg = bus_avg;
            g_sensor_data.shunt_avg = shunt_avg;
            g_sensor_data.current_avg = current_avg;
            g_sensor_data.power_avg = power_avg;
            
            // Log data if logging is enabled
            log_sensor_data(&g_sensor_data);
            
            // Update screen with sensor data
            screen_update_sensor_data(bus_voltage, current, power);
            
            xSemaphoreGive(g_sensor_data_mutex);
        }
        
        // Print debug info every 10 seconds
        static int debug_counter = 0;
        if (++debug_counter >= 10) {
            printf("Sensor: Bus=%.3fV, Shunt=%.3fmV, Current=%.3fmA, Power=%.3fmW\n", 
                   bus_avg, shunt_avg, current_avg, power_avg);
            debug_counter = 0;
        }
        
        // Get current logging interval from configuration
        const config_data_t* config = get_config();
        uint32_t log_interval_ms = config->log_interval_ms;
        
        // Wait for the configured interval before next reading
        vTaskDelay(log_interval_ms / portTICK_PERIOD_MS);
    }
}

void init_monitoring_task(void) {
    // Create mutex for sensor data protection
    g_sensor_data_mutex = xSemaphoreCreateMutex();
    if (g_sensor_data_mutex == NULL) {
        printf("Failed to create sensor data mutex\n");
        return;
    }
    
    // Create monitoring task
    BaseType_t ret = xTaskCreate(monitoring_task, "monitoring_task", 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        printf("Failed to create monitoring task\n");
        vSemaphoreDelete(g_sensor_data_mutex);
        return;
    }
    
    printf("Monitoring task initialized\n");
}
