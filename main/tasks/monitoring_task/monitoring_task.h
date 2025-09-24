#ifndef MONITORING_TASK_H
#define MONITORING_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Individual sensor data structure
typedef struct {
    float bus_voltage;      // Bus voltage in volts
    float shunt_voltage;    // Shunt voltage in millivolts
    float current;          // Current in milliamps
    float power;            // Power in milliwatts
    int16_t raw_bus;        // Raw bus voltage reading
    int16_t raw_shunt;      // Raw shunt voltage reading
    int16_t raw_current;    // Raw current reading
    int16_t raw_power;      // Raw power reading
    float bus_avg;          // Filtered bus voltage
    float shunt_avg;        // Filtered shunt voltage
    float current_avg;      // Filtered current
    float power_avg;        // Filtered power
} sensor_reading_t;

// Combined sensor data structure
typedef struct {
    sensor_reading_t sensor1;   // First INA219 sensor data
    sensor_reading_t sensor2;   // Second INA219 sensor data
    uint64_t timestamp;         // Timestamp in milliseconds
} sensor_data_t;

// Global variables
extern sensor_data_t g_sensor_data;
extern SemaphoreHandle_t g_sensor_data_mutex;

// Function declarations
void monitoring_task(void *pvParameters);
void init_monitoring_task(void);

#endif // MONITORING_TASK_H
