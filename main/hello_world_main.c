/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "utils/definitions/definitions.h"

#include "freertos/task.h"
#include "sdkconfig.h"
#include "utils/utils.c"
#include "tasks/monitoring_task/monitoring_task.h"
#include "tasks/webserver_simple/webserver_simple.c"
#include "tasks/data_logger/data_logger.h"
#include "tasks/time_sync/time_sync.h"
#include "tasks/screen_task/screen_task.h"
#include "utils/wifi_config/wifi_config.h"
#include "utils/config_manager/config_manager.h"
#include "utils/sdcard_driver/sdcard_driver.h"
#include "driver/i2c.h"
#include <inttypes.h>
#include <stdio.h>

#include "ina219.h"

// Initialize both INA219 sensors
void init_dual_ina219_sensors(void) {
  printf("Initializing I2C...\n");
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };
  
  i2c_param_config(I2C_MASTER_NUM, &conf);
  i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
  printf("I2C driver installed successfully with SDA=%d, SCL=%d\n", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
  
  printf("\nScanning I2C bus...\n");
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  printf("00:         ");
  for (uint8_t i = 4; i < 0x78; i++) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (i % 16 == 0) {
      printf("\n%.2x:", i);
    }
    if (ret == ESP_OK) {
      printf(" %.2x", i);
    } else {
      printf(" --");
    }
  }
  printf("\n\n");
  
  // Initialize Sensor 1
  printf("Looking for INA219 Sensor 1...\n");
  esp_err_t ret1 = ina219_begin(&ina219_sensor1, I2C_MASTER_NUM, INA219_SENSOR1_ADDRESS);
  if (ret1 == ESP_OK) {
    printf("Found INA219 Sensor 1 at address 0x%02x\n", INA219_SENSOR1_ADDRESS);
    printf("Initializing INA219 Sensor 1...\n");
    ina219_setCalibration_32V_2A(&ina219_sensor1); // 32V, 2A range
    printf("INA219 Sensor 1 initialized successfully!\n");
  } else {
    printf("INA219 Sensor 1 not found at address 0x%02x (error: %s)\n", INA219_SENSOR1_ADDRESS, esp_err_to_name(ret1));
  }
  
  // Initialize Sensor 2
  printf("Looking for INA219 Sensor 2...\n");
  esp_err_t ret2 = ina219_begin(&ina219_sensor2, I2C_MASTER_NUM, INA219_SENSOR2_ADDRESS);
  if (ret2 == ESP_OK) {
    printf("Found INA219 Sensor 2 at address 0x%02x\n", INA219_SENSOR2_ADDRESS);
    printf("Initializing INA219 Sensor 2...\n");
    ina219_setCalibration_32V_2A(&ina219_sensor2); // 32V, 2A range
    printf("INA219 Sensor 2 initialized successfully!\n");
  } else {
    printf("INA219 Sensor 2 not found at address 0x%02x (error: %s)\n", INA219_SENSOR2_ADDRESS, esp_err_to_name(ret2));
  }
  
  printf("Dual INA219 sensor initialization complete!\n");
}

void app_main(void) {
  printf("Hello world!\n");
  printf("Power Consumption Monitor with Web Interface\n");
  printf("============================================\n");

  // Initialize configuration manager
  printf("Initializing configuration manager...\n");
  init_config_manager();
  
  // Initialize screen task first (initializes SPI bus)
  printf("Initializing screen task...\n");
  init_screen_task();
  
  // Initialize WiFi
  printf("Initializing WiFi...\n");
  init_wifi();
  
  // Initialize time synchronization
  printf("Initializing time synchronization...\n");
  init_time_sync();
  
  // Wait a moment for time sync to complete
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  // Initialize INA219 sensors
  printf("Initializing INA219 sensors...\n");
  init_dual_ina219_sensors();
  
  // Initialize data logger
  printf("Initializing data logger...\n");
  init_data_logger();
  
  // Initialize dynamic SD card detection
  printf("Initializing dynamic SD card detection...\n");
  init_dynamic_sdcard_detection();
  
  // Initialize monitoring task
  printf("Starting monitoring task...\n");
  init_monitoring_task();
  
  // Initialize webserver task
  printf("Starting webserver task...\n");
  init_webserver_task();
  
  printf("\nSystem initialized successfully!\n");
  printf("Web interface available at: http://<ESP32_IP>\n");
  printf("API endpoint: http://<ESP32_IP>/api/sensor-data\n");
  printf("Monitoring will continue in background...\n");
  
  // Keep the main task running
  while (1) {
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Print status every 10 seconds
    printf("System running - Web interface active\n");
  }
}
