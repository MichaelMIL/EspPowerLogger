#ifndef SDCARD_DRIVER_H
#define SDCARD_DRIVER_H

#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <stdbool.h>

// SD Card pin definitions
#define PIN_NUM_MISO 15
#define PIN_NUM_MOSI 16
#define PIN_NUM_CLK  17
#define PIN_NUM_CS   18
#define PIN_NUM_CD   8

// SD Card mount point
#define MOUNT_POINT "/sdcard"

// Function declarations
esp_err_t init_sdcard(void);
esp_err_t deinit_sdcard(void);
bool is_sdcard_available(void);
const char* get_sdcard_mount_point(void);
size_t get_sdcard_free_space(void);
size_t get_sdcard_total_space(void);
esp_err_t init_dynamic_sdcard_detection(void);
void sdcard_detection_task(void *pvParameters);

#endif // SDCARD_DRIVER_H
