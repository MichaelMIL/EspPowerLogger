#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "ina219.h"

// I2C configuration - Try different GPIO combinations
// ESP32-S3: GPIO 4,5 or GPIO 8,9 or GPIO 10,11
// ESP32: GPIO 21,22 or GPIO 26,27
#define I2C_MASTER_SCL_IO           4     /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           5     /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0     /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          50000  /*!< I2C master clock frequency - reduced for stability */
#define I2C_MASTER_TX_BUF_DISABLE   0     /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0     /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

// Alternative GPIO pins to try if the first ones don't work
#define I2C_ALT_SCL_IO              8
#define I2C_ALT_SDA_IO              9

// INA219 configuration - Using Adafruit library style
// Default I2C address for INA219
#define INA219_DEFAULT_ADDRESS      INA219_ADDRESS_GND_GND



// AP Configuration
#define WIFI_MAXIMUM_RETRY  5
#define AP_SSID "PowerMonitor_Config"
#define AP_PASS "config123"
#define AP_IP "192.168.4.1"
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4


// Default configuration values
#define DEFAULT_LOG_INTERVAL_MS 1000
#define DEFAULT_WIFI_SSID "Morties"
#define DEFAULT_WIFI_PASS "RickAndRoll"
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

// External declaration of INA219 handle
extern ina219_handle_t ina219;
extern bool g_logging_enabled;
extern char g_log_filename[64];
extern uint8_t display_rotation;
extern const uint8_t font5x7[][5];
extern bool s_ap_mode;
extern char wifi_ip[16];
extern char ap_ssid[32];
extern char ap_password[32];
extern char ap_ip[16];
extern bool user_on_web_page;
#endif // DEFINITIONS_H
