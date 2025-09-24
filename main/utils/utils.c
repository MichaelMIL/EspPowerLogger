#ifndef UTILS_H
#define UTILS_H

#include "definitions/definitions.h"
#include "driver/i2c.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t i2c_master_init_with_pins(int sda_pin, int scl_pin)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        printf("Failed to configure I2C with SDA=%d, SCL=%d: %s\n", sda_pin, scl_pin, esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        printf("Failed to install I2C driver with SDA=%d, SCL=%d: %s\n", sda_pin, scl_pin, esp_err_to_name(ret));
        return ret;
    }
    
    printf("I2C driver installed successfully with SDA=%d, SCL=%d\n", sda_pin, scl_pin);
    return ESP_OK;
}

void i2c_master_init(void)
{
    // Try primary GPIO pins first
    esp_err_t ret = i2c_master_init_with_pins(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    if (ret == ESP_OK) {
        return;
    }
    
    // If primary pins fail, try alternative pins
    printf("Primary GPIO pins failed, trying alternative pins...\n");
    ret = i2c_master_init_with_pins(I2C_ALT_SDA_IO, I2C_ALT_SCL_IO);
    if (ret == ESP_OK) {
        return;
    }
    
    // If both fail, try some common ESP32-S3 pins
    printf("Alternative pins failed, trying ESP32-S3 common pins...\n");
    int s3_pins[][2] = {{10, 11}, {12, 13}, {14, 15}, {16, 17}};
    for (int i = 0; i < 4; i++) {
        ret = i2c_master_init_with_pins(s3_pins[i][0], s3_pins[i][1]);
        if (ret == ESP_OK) {
            return;
        }
    }
    
    printf("All GPIO pin combinations failed! Please check your ESP32 variant and update GPIO pins.\n");
}

void i2c_scanner(void)
{
    printf("\nScanning I2C bus...\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");
    
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (addr % 16 == 0) {
            printf("\n%.2x:", addr);
        }
        
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            printf(" %.2x", addr);
        } else {
            printf(" --");
        }
    }
    printf("\n\n");
}

#endif
