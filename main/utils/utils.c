#ifndef UTILS_H
#define UTILS_H

#include "../definitions.h"
#include "driver/i2c.h"


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

void init_ina219(void) {
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ", CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154)
               ? ", 802.15.4 (Zigbee/Thread)"
               : "");
  
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
      printf("Get flash size failed");
      return;
    }
  
    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                         : "external");
  
    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());
  
    // Initialize I2C
    printf("\nInitializing I2C...\n");
    i2c_master_init();
  
    // Scan for I2C devices
    i2c_scanner();
  
    // Try to find INA219 on different addresses
    uint8_t ina219_addr = 0;
    uint8_t possible_addresses[] = {INA219_ADDRESS, INA219_CALC_ADDRESS(0, 1),
                                    INA219_CALC_ADDRESS(1, 0),
                                    INA219_CALC_ADDRESS(1, 1)};
  
    printf("Looking for INA219...\n");
    for (int i = 0; i < 4; i++) {
      i2c_cmd_handle_t cmd = i2c_cmd_link_create();
      i2c_master_start(cmd);
      i2c_master_write_byte(cmd, (possible_addresses[i] << 1) | I2C_MASTER_WRITE,
                            true);
      i2c_master_stop(cmd);
      esp_err_t ret =
          i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_PERIOD_MS);
      i2c_cmd_link_delete(cmd);
  
      if (ret == ESP_OK) {
        ina219_addr = possible_addresses[i];
        printf("Found INA219 at address 0x%02x\n", ina219_addr);
        break;
      }
    }
  
    if (ina219_addr == 0) {
      printf("INA219 not found! Please check:\n");
      printf("1. Wiring: SDA->GPIO%d, SCL->GPIO%d\n", I2C_MASTER_SDA_IO,
             I2C_MASTER_SCL_IO);
      printf("2. Power: VCC->3.3V, GND->GND\n");
      printf("3. Pull-up resistors on SDA/SCL lines\n");
      printf("4. INA219 is properly connected\n");
      return;
    }
  
    // Initialize INA219 (Adafruit style)
    printf("Initializing INA219 current sensor at 0x%02x...\n", ina219_addr);
    esp_err_t ret = ina219_begin(&ina219, I2C_MASTER_NUM, ina219_addr);
    if (ret != ESP_OK) {
      printf("Failed to initialize INA219: %s\n", esp_err_to_name(ret));
      printf("This might be due to:\n");
      printf("- Wrong I2C address\n");
      printf("- Hardware connection issues\n");
      printf("- INA219 not responding\n");
      return;
    }
  
    // Set calibration for 32V, 2A range (Adafruit style)
    printf("Setting calibration for 32V, 2A range...\n");
    ina219_setCalibration_32V_2A(&ina219);
    if (!ina219_success(&ina219)) {
      printf("Failed to calibrate INA219\n");
      return;
    }
  
    printf("INA219 initialized successfully!\n");
    printf("Calibration: 32V, 2A range with 0.1Ω shunt\n");
    printf("Expected input: 5V, 100mA\n");
    printf("Expected shunt voltage: %.1f mV (100mA * 0.1Ω)\n", 100.0f * 0.1f);
    printf("Expected bus voltage raw: ~1250 (5V / 0.004V)\n");
    printf("Expected shunt voltage raw: ~1000 (10mV / 0.01mV)\n");
    printf("Note: If bus voltage reads high, check actual input voltage with "
           "multimeter\n");
  
    // Test basic I2C communication
    printf("\nTesting I2C communication...\n");
    uint16_t test_config;
    esp_err_t config_ret =
        ina219_read_register(&ina219, INA219_REG_CONFIG, &test_config);
    if (config_ret == ESP_OK) {
      printf("Config register read: 0x%04x\n", test_config);
    } else {
      printf("Failed to read config register: %s\n", esp_err_to_name(config_ret));
    }
  
    // Test bus voltage register stability
    printf("Testing bus voltage register stability...\n");
    for (int test = 0; test < 5; test++) {
      int16_t test_bus = ina219_getBusVoltage_raw(&ina219);
      printf("Test %d: Bus voltage raw = %d\n", test + 1, test_bus);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
#endif
