/*
 * SPDX-FileCopyrightText: 2024 INA219 Component - Based on Adafruit Library
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// INA219 I2C address calculation
#define INA219_CALC_ADDRESS(INA_ADDR0, INA_ADDR1) \
  (0x40 | (INA_ADDR0 != 0 ? 0x01 : 0x00) | (INA_ADDR1 != 0 ? 0x04 : 0x00))

// Default I2C address
#define INA219_ADDRESS (0x40) // 1000000 (A0+A1=GND)

// Register addresses
#define INA219_REG_CONFIG          0x00
#define INA219_REG_SHUNTVOLTAGE    0x01
#define INA219_REG_BUSVOLTAGE      0x02
#define INA219_REG_POWER           0x03
#define INA219_REG_CURRENT         0x04
#define INA219_REG_CALIBRATION     0x05

// Configuration register bits
#define INA219_CONFIG_RESET        (0x8000) // Reset Bit

// Bus voltage range
#define INA219_CONFIG_BVOLTAGERANGE_MASK (0x2000)
#define INA219_CONFIG_BVOLTAGERANGE_16V  (0x0000) // 0-16V Range
#define INA219_CONFIG_BVOLTAGERANGE_32V  (0x2000) // 0-32V Range

// Gain settings
#define INA219_CONFIG_GAIN_MASK    (0x1800)
#define INA219_CONFIG_GAIN_1_40MV  (0x0000)  // Gain 1, 40mV Range
#define INA219_CONFIG_GAIN_2_80MV  (0x0800)  // Gain 2, 80mV Range
#define INA219_CONFIG_GAIN_4_160MV (0x1000)  // Gain 4, 160mV Range
#define INA219_CONFIG_GAIN_8_320MV (0x1800)  // Gain 8, 320mV Range

// Bus ADC resolution
#define INA219_CONFIG_BADCRES_MASK (0x0780)
#define INA219_CONFIG_BADCRES_9BIT     (0x0000)  // 9-bit bus res = 0..511
#define INA219_CONFIG_BADCRES_10BIT    (0x0080)  // 10-bit bus res = 0..1023
#define INA219_CONFIG_BADCRES_11BIT    (0x0100)  // 11-bit bus res = 0..2047
#define INA219_CONFIG_BADCRES_12BIT    (0x0180)  // 12-bit bus res = 0..4097
#define INA219_CONFIG_BADCRES_12BIT_2S_1060US  (0x0480)  // 2 x 12-bit bus samples averaged
#define INA219_CONFIG_BADCRES_12BIT_4S_2130US  (0x0500)  // 4 x 12-bit bus samples averaged
#define INA219_CONFIG_BADCRES_12BIT_8S_4260US  (0x0580)  // 8 x 12-bit bus samples averaged
#define INA219_CONFIG_BADCRES_12BIT_16S_8510US (0x0600)  // 16 x 12-bit bus samples averaged
#define INA219_CONFIG_BADCRES_12BIT_32S_17MS   (0x0680)  // 32 x 12-bit bus samples averaged
#define INA219_CONFIG_BADCRES_12BIT_64S_34MS   (0x0700)  // 64 x 12-bit bus samples averaged
#define INA219_CONFIG_BADCRES_12BIT_128S_69MS  (0x0780)  // 128 x 12-bit bus samples averaged

// Shunt ADC resolution
#define INA219_CONFIG_SADCRES_MASK (0x0078)
#define INA219_CONFIG_SADCRES_9BIT_1S_84US   (0x0000)   // 1 x 9-bit shunt sample
#define INA219_CONFIG_SADCRES_10BIT_1S_148US (0x0008)   // 1 x 10-bit shunt sample
#define INA219_CONFIG_SADCRES_11BIT_1S_276US (0x0010)   // 1 x 11-bit shunt sample
#define INA219_CONFIG_SADCRES_12BIT_1S_532US (0x0018)   // 1 x 12-bit shunt sample
#define INA219_CONFIG_SADCRES_12BIT_2S_1060US  (0x0048)   // 2 x 12-bit shunt samples averaged
#define INA219_CONFIG_SADCRES_12BIT_4S_2130US  (0x0050)   // 4 x 12-bit shunt samples averaged
#define INA219_CONFIG_SADCRES_12BIT_8S_4260US  (0x0058)   // 8 x 12-bit shunt samples averaged
#define INA219_CONFIG_SADCRES_12BIT_16S_8510US (0x0060)   // 16 x 12-bit shunt samples averaged
#define INA219_CONFIG_SADCRES_12BIT_32S_17MS   (0x0068)   // 32 x 12-bit shunt samples averaged
#define INA219_CONFIG_SADCRES_12BIT_64S_34MS   (0x0070)   // 64 x 12-bit shunt samples averaged
#define INA219_CONFIG_SADCRES_12BIT_128S_69MS  (0x0078)   // 128 x 12-bit shunt samples averaged

// Operating mode
#define INA219_CONFIG_MODE_MASK (0x0007)
#define INA219_CONFIG_MODE_POWERDOWN           0x00  // power down
#define INA219_CONFIG_MODE_SVOLT_TRIGGERED     0x01  // shunt voltage triggered
#define INA219_CONFIG_MODE_BVOLT_TRIGGERED     0x02  // bus voltage triggered
#define INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED 0x03  // shunt and bus voltage triggered
#define INA219_CONFIG_MODE_ADCOFF              0x04  // ADC off
#define INA219_CONFIG_MODE_SVOLT_CONTINUOUS    0x05  // shunt voltage continuous
#define INA219_CONFIG_MODE_BVOLT_CONTINUOUS    0x06  // bus voltage continuous
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x07  // shunt and bus voltage continuous

// INA219 handle structure
typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    uint32_t cal_value;
    uint32_t current_divider_mA;
    float power_multiplier_mW;
    bool initialized;
    bool success;
} ina219_handle_t;

// Adafruit-style function prototypes
esp_err_t ina219_begin(ina219_handle_t *handle, i2c_port_t i2c_port, uint8_t i2c_addr);
void ina219_setCalibration_32V_2A(ina219_handle_t *handle);
void ina219_setCalibration_32V_1A(ina219_handle_t *handle);
void ina219_setCalibration_16V_400mA(ina219_handle_t *handle);
void ina219_powerSave(ina219_handle_t *handle, bool on);
bool ina219_success(ina219_handle_t *handle);

// Measurement functions (Adafruit style)
float ina219_getBusVoltage_V(ina219_handle_t *handle);
float ina219_getShuntVoltage_mV(ina219_handle_t *handle);
float ina219_getCurrent_mA(ina219_handle_t *handle);
float ina219_getPower_mW(ina219_handle_t *handle);

// Raw measurement functions
int16_t ina219_getBusVoltage_raw(ina219_handle_t *handle);
int16_t ina219_getShuntVoltage_raw(ina219_handle_t *handle);
int16_t ina219_getCurrent_raw(ina219_handle_t *handle);
int16_t ina219_getPower_raw(ina219_handle_t *handle);

// Low-level register functions
esp_err_t ina219_read_register(ina219_handle_t *handle, uint8_t reg, uint16_t *data);

#ifdef __cplusplus
}
#endif