/*
 * SPDX-FileCopyrightText: 2024 INA219 Component - Based on Adafruit Library
 *
 * SPDX-License-Identifier: MIT
 */

#include "ina219.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "INA219";

// Helper function to read 16-bit register
esp_err_t ina219_read_register(ina219_handle_t *handle, uint8_t reg, uint16_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, (uint8_t *)data, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, (uint8_t *)data + 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02x: %s", reg, esp_err_to_name(ret));
        handle->success = false;
        return ret;
    }
    
    // Swap bytes (INA219 is big-endian)
    *data = (*data << 8) | (*data >> 8);
    handle->success = true;
    return ESP_OK;
}

// Helper function to write 16-bit register
static esp_err_t ina219_write_register(ina219_handle_t *handle, uint8_t reg, uint16_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    // Swap bytes (INA219 is big-endian)
    i2c_master_write_byte(cmd, (data >> 8) & 0xFF, true);
    i2c_master_write_byte(cmd, data & 0xFF, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02x: %s", reg, esp_err_to_name(ret));
        handle->success = false;
        return ret;
    }
    
    handle->success = true;
    return ESP_OK;
}

esp_err_t ina219_begin(ina219_handle_t *handle, i2c_port_t i2c_port, uint8_t i2c_addr)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->i2c_port = i2c_port;
    handle->i2c_addr = i2c_addr;
    handle->cal_value = 0;
    handle->current_divider_mA = 0;
    handle->power_multiplier_mW = 0.0f;
    handle->initialized = false;
    handle->success = false;
    
    // Test if device is responding
    uint16_t config_reg;
    esp_err_t ret = ina219_read_register(handle, INA219_REG_CONFIG, &config_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device not responding at address 0x%02x", i2c_addr);
        return ret;
    }
    
    ESP_LOGI(TAG, "Device responding at address 0x%02x, config register: 0x%04x", i2c_addr, config_reg);
    
    // Initialize with default calibration (32V, 2A)
    ina219_setCalibration_32V_2A(handle);
    
    handle->initialized = true;
    ESP_LOGI(TAG, "INA219 initialized on I2C port %d, address 0x%02x", i2c_port, i2c_addr);
    
    return ESP_OK;
}

void ina219_setCalibration_32V_2A(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return;
    }
    
    // Calibration for 32V, 2A range with 0.1Ω shunt
    // Based on Adafruit calculations:
    // CurrentLSB = 0.0001 (100uA per bit)
    // Cal = 4096 (0x1000)
    // PowerLSB = 0.002 (2mW per bit)
    
    handle->cal_value = 4096;
    handle->current_divider_mA = 10;  // Current LSB = 100uA per bit (1000/100 = 10)
    handle->power_multiplier_mW = 2.0f;  // Power LSB = 2mW per bit
    
    // Set Calibration register
    ESP_LOGI(TAG, "Writing calibration value: 0x%04x", handle->cal_value);
    ina219_write_register(handle, INA219_REG_CALIBRATION, handle->cal_value);
    
    // Set Config register
    uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                      INA219_CONFIG_GAIN_8_320MV |
                      INA219_CONFIG_BADCRES_12BIT |
                      INA219_CONFIG_SADCRES_12BIT_1S_532US |
                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    ESP_LOGI(TAG, "Writing config value: 0x%04x", config);
    ina219_write_register(handle, INA219_REG_CONFIG, config);
    
    // Verify the configuration was written correctly
    uint16_t read_config;
    ina219_read_register(handle, INA219_REG_CONFIG, &read_config);
    ESP_LOGI(TAG, "Read back config value: 0x%04x", read_config);
    
    uint16_t read_cal;
    ina219_read_register(handle, INA219_REG_CALIBRATION, &read_cal);
    ESP_LOGI(TAG, "Read back calibration value: 0x%04x", read_cal);
    
    ESP_LOGI(TAG, "Calibration set for 32V, 2A range");
}

void ina219_setCalibration_32V_1A(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return;
    }
    
    // Calibration for 32V, 1A range with 0.1Ω shunt
    // Based on Adafruit calculations:
    // CurrentLSB = 0.0000400 (40uA per bit)
    // Cal = 10240 (0x2800)
    // PowerLSB = 0.0008 (800uW per bit)
    
    handle->cal_value = 10240;
    handle->current_divider_mA = 25;  // Current LSB = 40uA per bit (1000/40 = 25)
    handle->power_multiplier_mW = 0.8f;  // Power LSB = 800uW per bit
    
    // Set Calibration register
    ina219_write_register(handle, INA219_REG_CALIBRATION, handle->cal_value);
    
    // Set Config register
    uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                      INA219_CONFIG_GAIN_8_320MV |
                      INA219_CONFIG_BADCRES_12BIT |
                      INA219_CONFIG_SADCRES_12BIT_1S_532US |
                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    ina219_write_register(handle, INA219_REG_CONFIG, config);
    
    ESP_LOGI(TAG, "Calibration set for 32V, 1A range");
}

void ina219_setCalibration_16V_400mA(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return;
    }
    
    // Calibration for 16V, 400mA range with 0.1Ω shunt
    // Based on Adafruit calculations:
    // CurrentLSB = 0.00005 (50uA per bit)
    // Cal = 8192 (0x2000)
    // PowerLSB = 0.001 (1mW per bit)
    
    handle->cal_value = 8192;
    handle->current_divider_mA = 20;  // Current LSB = 50uA per bit (1000/50 = 20)
    handle->power_multiplier_mW = 1.0f;  // Power LSB = 1mW per bit
    
    // Set Calibration register
    ina219_write_register(handle, INA219_REG_CALIBRATION, handle->cal_value);
    
    // Set Config register
    uint16_t config = INA219_CONFIG_BVOLTAGERANGE_16V |
                      INA219_CONFIG_GAIN_1_40MV |
                      INA219_CONFIG_BADCRES_12BIT |
                      INA219_CONFIG_SADCRES_12BIT_1S_532US |
                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    ina219_write_register(handle, INA219_REG_CONFIG, config);
    
    ESP_LOGI(TAG, "Calibration set for 16V, 400mA range");
}

void ina219_powerSave(ina219_handle_t *handle, bool on)
{
    if (handle == NULL || !handle->initialized) {
        return;
    }
    
    // Read current config
    uint16_t config;
    ina219_read_register(handle, INA219_REG_CONFIG, &config);
    
    // Clear mode bits
    config &= ~INA219_CONFIG_MODE_MASK;
    
    // Set new mode
    if (on) {
        config |= INA219_CONFIG_MODE_POWERDOWN;
    } else {
        config |= INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    }
    
    ina219_write_register(handle, INA219_REG_CONFIG, config);
}

bool ina219_success(ina219_handle_t *handle)
{
    if (handle == NULL) {
        return false;
    }
    return handle->success;
}

int16_t ina219_getBusVoltage_raw(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return 0;
    }
    
    uint16_t value;
    esp_err_t ret = ina219_read_register(handle, INA219_REG_BUSVOLTAGE, &value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read bus voltage register: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // Check for invalid register values (should be reasonable range)
    if (value == 0x0000 || value == 0xFFFF) {
        ESP_LOGW(TAG, "Suspicious bus voltage register value: 0x%04x", value);
    }
    
    // Debug: print raw register value
    // ESP_LOGI(TAG, "Bus voltage register: 0x%04x", value);
    
    // Shift to the right 3 to drop CNVR and OVF
    // The raw value is in 4mV units, so we return it as-is (no conversion needed)
    int16_t result = (int16_t)(value >> 3);
    // ESP_LOGI(TAG, "Bus voltage raw result: %d", result);
    return result;
}

int16_t ina219_getShuntVoltage_raw(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return 0;
    }
    
    uint16_t value;
    esp_err_t ret = ina219_read_register(handle, INA219_REG_SHUNTVOLTAGE, &value);
    if (ret != ESP_OK) {
        return 0;
    }
    
    // Debug: print raw register value
    ESP_LOGD(TAG, "Shunt voltage register: 0x%04x", value);
    
    return (int16_t)value;
}

int16_t ina219_getCurrent_raw(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return 0;
    }
    
    // Sometimes a sharp load will reset the INA219, which will
    // reset the cal register, meaning CURRENT and POWER will
    // not be available ... avoid this by always setting a cal
    // value even if it's an unfortunate extra step
    ina219_write_register(handle, INA219_REG_CALIBRATION, handle->cal_value);
    
    // Now we can safely read the CURRENT register!
    uint16_t value;
    esp_err_t ret = ina219_read_register(handle, INA219_REG_CURRENT, &value);
    if (ret != ESP_OK) {
        return 0;
    }
    
    return (int16_t)value;
}

int16_t ina219_getPower_raw(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return 0;
    }
    
    // Sometimes a sharp load will reset the INA219, which will
    // reset the cal register, meaning CURRENT and POWER will
    // not be available ... avoid this by always setting a cal
    // value even if it's an unfortunate extra step
    ina219_write_register(handle, INA219_REG_CALIBRATION, handle->cal_value);
    
    // Now we can safely read the POWER register!
    uint16_t value;
    esp_err_t ret = ina219_read_register(handle, INA219_REG_POWER, &value);
    if (ret != ESP_OK) {
        return 0;
    }
    
    return (int16_t)value;
}

float ina219_getBusVoltage_V(ina219_handle_t *handle)
{
    int16_t value = ina219_getBusVoltage_raw(handle);
    return value * 0.004f;  // Convert from 4mV units to volts
}

float ina219_getShuntVoltage_mV(ina219_handle_t *handle)
{
    int16_t value = ina219_getShuntVoltage_raw(handle);
    return value * 0.01f;  // Convert to millivolts
}

float ina219_getCurrent_mA(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return 0.0f;
    }
    
    float valueDec = ina219_getCurrent_raw(handle);
    valueDec /= handle->current_divider_mA;
    return valueDec;
}

float ina219_getPower_mW(ina219_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return 0.0f;
    }
    
    float valueDec = ina219_getPower_raw(handle);
    valueDec *= handle->power_multiplier_mW;
    return valueDec;
}