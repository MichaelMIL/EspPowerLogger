#include "tasks/monitoring_task/monitoring_task.h"
#include "utils/definitions/definitions.h"
#include "ina219.h"
#include "tasks/data_logger/data_logger.h"
#include "utils/config_manager/config_manager.h"
#include "tasks/screen_task/screen_task.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

// Global sensor data structure
sensor_data_t g_sensor_data;
SemaphoreHandle_t g_sensor_data_mutex;

void monitoring_task(void *pvParameters) {
    printf("Monitoring task started\n");
    
    // Initialize sensor data
    memset(&g_sensor_data, 0, sizeof(sensor_data_t));
    
    // Initialize filtered values for both sensors
    static float sensor1_bus_avg = 0, sensor1_shunt_avg = 0, sensor1_current_avg = 0, sensor1_power_avg = 0;
    static float sensor2_bus_avg = 0, sensor2_shunt_avg = 0, sensor2_current_avg = 0, sensor2_power_avg = 0;
    static int first_read = 1;
    
    while (1) {
        // Read from Sensor 1
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor1_raw_bus = ina219_getBusVoltage_raw(&ina219_sensor1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor1_raw_shunt = ina219_getShuntVoltage_raw(&ina219_sensor1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor1_raw_current = ina219_getCurrent_raw(&ina219_sensor1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor1_raw_power = ina219_getPower_raw(&ina219_sensor1);
        
        // Read from Sensor 2
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor2_raw_bus = ina219_getBusVoltage_raw(&ina219_sensor2);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor2_raw_shunt = ina219_getShuntVoltage_raw(&ina219_sensor2);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor2_raw_current = ina219_getCurrent_raw(&ina219_sensor2);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int16_t sensor2_raw_power = ina219_getPower_raw(&ina219_sensor2);
        
        // Convert Sensor 1 values
        float sensor1_bus_voltage = sensor1_raw_bus * 0.004f;    // Convert from 4mV units to volts
        float sensor1_shunt_voltage = sensor1_raw_shunt * 0.01f; // Convert to millivolts
        float sensor1_current = (sensor1_raw_current / 10.0f) - 6.0;     // Convert using current divider (10mA per bit)
        float sensor1_power = sensor1_raw_power * 2.0f;          // Convert using power multiplier (2mW per bit)
        
        // Convert Sensor 2 values
        float sensor2_bus_voltage = sensor2_raw_bus * 0.004f;    // Convert from 4mV units to volts
        float sensor2_shunt_voltage = sensor2_raw_shunt * 0.01f; // Convert to millivolts
        float sensor2_current = (sensor2_raw_current / 10.0f) - 6.0;     // Convert using current divider (10mA per bit)
        float sensor2_power = sensor2_raw_power * 2.0f;          // Convert using power multiplier (2mW per bit)
        
        // Apply filtering (simple exponential moving average) for both sensors
        if (first_read) {
            // Initialize Sensor 1 averages
            sensor1_bus_avg = sensor1_bus_voltage;
            sensor1_shunt_avg = sensor1_shunt_voltage;
            sensor1_current_avg = sensor1_current;
            sensor1_power_avg = sensor1_power;
            
            // Initialize Sensor 2 averages
            sensor2_bus_avg = sensor2_bus_voltage;
            sensor2_shunt_avg = sensor2_shunt_voltage;
            sensor2_current_avg = sensor2_current;
            sensor2_power_avg = sensor2_power;
            
            first_read = 0;
        } else {
            // Filter Sensor 1 (alpha = 0.3)
            sensor1_bus_avg = 0.7f * sensor1_bus_avg + 0.3f * sensor1_bus_voltage;
            sensor1_shunt_avg = 0.7f * sensor1_shunt_avg + 0.3f * sensor1_shunt_voltage;
            sensor1_current_avg = 0.7f * sensor1_current_avg + 0.3f * sensor1_current;
            sensor1_power_avg = 0.7f * sensor1_power_avg + 0.3f * sensor1_power;
            
            // Filter Sensor 2 (alpha = 0.3)
            sensor2_bus_avg = 0.7f * sensor2_bus_avg + 0.3f * sensor2_bus_voltage;
            sensor2_shunt_avg = 0.7f * sensor2_shunt_avg + 0.3f * sensor2_shunt_voltage;
            sensor2_current_avg = 0.7f * sensor2_current_avg + 0.3f * sensor2_current;
            sensor2_power_avg = 0.7f * sensor2_power_avg + 0.3f * sensor2_power;
        }
        
        // Update global sensor data with mutex protection
        if (xSemaphoreTake(g_sensor_data_mutex, portMAX_DELAY) == pdTRUE) {
            // Store Sensor 1 data
            g_sensor_data.sensor1.bus_voltage = sensor1_bus_voltage;
            g_sensor_data.sensor1.shunt_voltage = sensor1_shunt_voltage;
            g_sensor_data.sensor1.current = sensor1_current;
            g_sensor_data.sensor1.power = sensor1_power;
            g_sensor_data.sensor1.raw_bus = sensor1_raw_bus;
            g_sensor_data.sensor1.raw_shunt = sensor1_raw_shunt;
            g_sensor_data.sensor1.raw_current = sensor1_raw_current;
            g_sensor_data.sensor1.raw_power = sensor1_raw_power;
            g_sensor_data.sensor1.bus_avg = sensor1_bus_avg;
            g_sensor_data.sensor1.shunt_avg = sensor1_shunt_avg;
            g_sensor_data.sensor1.current_avg = sensor1_current_avg;
            g_sensor_data.sensor1.power_avg = sensor1_power_avg;
            
            // Store Sensor 2 data
            g_sensor_data.sensor2.bus_voltage = sensor2_bus_voltage;
            g_sensor_data.sensor2.shunt_voltage = sensor2_shunt_voltage;
            g_sensor_data.sensor2.current = sensor2_current;
            g_sensor_data.sensor2.power = sensor2_power;
            g_sensor_data.sensor2.raw_bus = sensor2_raw_bus;
            g_sensor_data.sensor2.raw_shunt = sensor2_raw_shunt;
            g_sensor_data.sensor2.raw_current = sensor2_raw_current;
            g_sensor_data.sensor2.raw_power = sensor2_raw_power;
            g_sensor_data.sensor2.bus_avg = sensor2_bus_avg;
            g_sensor_data.sensor2.shunt_avg = sensor2_shunt_avg;
            g_sensor_data.sensor2.current_avg = sensor2_current_avg;
            g_sensor_data.sensor2.power_avg = sensor2_power_avg;
            
            // Store timestamp
            g_sensor_data.timestamp = esp_timer_get_time() / 1000; // Convert to milliseconds
            
            // Log data if logging is enabled
            log_sensor_data(&g_sensor_data);
            
            // Update screen with sensor data (using sensor 1 for now)
            screen_update_sensor_data(sensor1_bus_voltage, sensor1_current, sensor1_power, sensor2_bus_voltage, sensor2_current, sensor2_power);
            
            xSemaphoreGive(g_sensor_data_mutex);
        }
        
        // Print debug info every 10 seconds
        static int debug_counter = 0;
        if (++debug_counter >= 10) {
            printf("Sensor1: Bus=%.3fV, Current=%.3fmA, Power=%.3fmW | Sensor2: Bus=%.3fV, Current=%.3fmA, Power=%.3fmW\n", 
                   sensor1_bus_avg, sensor1_current_avg, sensor1_power_avg,
                   sensor2_bus_avg, sensor2_current_avg, sensor2_power_avg);
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
