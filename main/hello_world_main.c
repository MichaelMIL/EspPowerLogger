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
#include <inttypes.h>
#include <stdio.h>

#include "ina219.h"


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
  
  // Initialize INA219 sensor
  printf("Initializing INA219 sensor...\n");
  init_ina219();
  
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
