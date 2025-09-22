#include "../definitions/definitions.h"
#include "sdcard_driver.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>


static const char *TAG = "sdcard_driver";
static bool s_sdcard_available = false;
static sdmmc_card_t *s_card = NULL;
static TaskHandle_t s_detection_task_handle = NULL;
static bool s_detection_task_running = false;

// Check if SD card is inserted using card detect pin
static bool check_card_detect(void) {
    
    // Card detect is typically active low (0 when card is present)
    int cd_level = gpio_get_level(PIN_NUM_CD);
    // ESP_LOGI(TAG, "Card detect pin level: %d", cd_level);
    is_sd_card_present = (cd_level == 0);
    // Return true if card is detected (pin is low)
    return (cd_level == 0);
}

// Initialize SD card
esp_err_t init_sdcard(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing SD card...");
    
    // Check if card is physically present
    if (!check_card_detect()) {
        ESP_LOGW(TAG, "No SD card detected");
        s_sdcard_available = false;
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "SD card detected, initializing...");
    
    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // Format if mount fails
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    // Use SPI mode - use HSPI_HOST (SPI1_HOST) for SD card
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;  // Use HSPI_HOST for SD card
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        s_sdcard_available = false;
        return ret;
    }
    
    // This initializes the slot without card detect (CD) and write protect (WP) signals
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        s_sdcard_available = false;
        return ret;
    }
    
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, s_card);
    
    // Test if mount point is accessible
    ESP_LOGI(TAG, "Testing SD card mount point accessibility...");
    FILE *test_file = fopen("/sdcard/test.txt", "w");
    if (test_file != NULL) {
        fprintf(test_file, "SD card test");
        fclose(test_file);
        remove("/sdcard/test.txt");  // Clean up test file
        ESP_LOGI(TAG, "SD card mount point is accessible");
        
        // Test creating a file with a similar name pattern
        char test_filename[128];
        snprintf(test_filename, sizeof(test_filename), "/sdcard/test_log.csv");
        FILE *test_log = fopen(test_filename, "w");
        if (test_log != NULL) {
            fprintf(test_log, "test,data\n");
            fclose(test_log);
            remove(test_filename);
            ESP_LOGI(TAG, "SD card CSV file creation test passed");
        } else {
            ESP_LOGE(TAG, "SD card CSV file creation test failed (errno: %d)", errno);
            s_sdcard_available = false;
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "SD card mount point is not accessible (errno: %d)", errno);
        s_sdcard_available = false;
        return ESP_FAIL;
    }
    
    s_sdcard_available = true;
    ESP_LOGI(TAG, "SD card initialized successfully");
    return ESP_OK;
}

// Deinitialize SD card
esp_err_t deinit_sdcard(void) {
    if (!s_sdcard_available) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Deinitialize the SPI bus
    ret = spi_bus_free(SPI3_HOST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_sdcard_available = false;
    s_card = NULL;
    ESP_LOGI(TAG, "SD card deinitialized");
    return ESP_OK;
}

// Check if SD card is available
bool is_sdcard_available(void) {
    is_sd_card_present = s_sdcard_available;
    return s_sdcard_available;
}

// Get SD card mount point
const char* get_sdcard_mount_point(void) {
    return s_sdcard_available ? MOUNT_POINT : NULL;
}

// Get SD card free space
size_t get_sdcard_free_space(void) {
    if (!s_sdcard_available || s_card == NULL) {
        return 0;
    }
    
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    
    // Get volume information and free clusters of drive
    esp_err_t ret = f_getfree("0:", &fre_clust, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get free space: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // Get total sectors and free sectors
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    
    // Return free space in bytes
    return (size_t)(fre_sect * 512);
}

// Get SD card total space
size_t get_sdcard_total_space(void) {
    if (!s_sdcard_available || s_card == NULL) {
        return 0;
    }
    
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    
    // Get volume information
    esp_err_t ret = f_getfree("0:", &fre_clust, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get total space: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // Get total sectors
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    
    // Return total space in bytes
    return (size_t)(tot_sect * 512);
}

// Initialize dynamic SD card detection
esp_err_t init_dynamic_sdcard_detection(void) {
    if (s_detection_task_running) {
        ESP_LOGW(TAG, "Dynamic detection already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting dynamic SD card detection task");
    
    BaseType_t ret = xTaskCreate(
        sdcard_detection_task,
        "sdcard_detection",
        4096,
        NULL,
        5,
        &s_detection_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create SD card detection task");
        return ESP_FAIL;
    }
    
    s_detection_task_running = true;
    ESP_LOGI(TAG, "Dynamic SD card detection task started");
    return ESP_OK;
}

// SD card detection task
void sdcard_detection_task(void *pvParameters) {
    bool last_card_state = false;
    bool current_card_state = false;
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_NUM_CD),
        .pull_down_en = 0,
        .pull_up_en = 1,  // Enable pull-up resistor
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "SD card detection task started");
    
    while (1) {
        // Check current card state
        current_card_state = check_card_detect();
        
        // If card state changed
        if (current_card_state != last_card_state) {
            ESP_LOGI(TAG, "SD card state changed: %s -> %s", 
                     last_card_state ? "present" : "absent",
                     current_card_state ? "present" : "absent");
            
            if (current_card_state && !s_sdcard_available) {
                // Card inserted - try to initialize
                ESP_LOGI(TAG, "SD card inserted, attempting to initialize...");
                esp_err_t ret = init_sdcard();
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "SD card initialized successfully after insertion");
                    // Notify data logger to switch to SD card
                    // This will be handled by the data logger checking is_sdcard_available()
                } else {
                    ESP_LOGW(TAG, "Failed to initialize SD card after insertion");
                }
            } else if (!current_card_state && s_sdcard_available) {
                // Card removed - deinitialize
                ESP_LOGI(TAG, "SD card removed, deinitializing...");
                esp_err_t ret = deinit_sdcard();
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "SD card deinitialized successfully after removal");
                    // Notify data logger to switch to SPIFFS
                    // This will be handled by the data logger checking is_sdcard_available()
                } else {
                    ESP_LOGE(TAG, "Failed to deinitialize SD card after removal");
                }
            }
            
            last_card_state = current_card_state;
        }
        
        // Check every 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
