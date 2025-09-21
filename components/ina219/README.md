# INA219 DC Current Sensor Component

This component provides an interface for the INA219 high-side current shunt monitor IC from Texas Instruments. The INA219 is a precision digital power monitor that can measure both current and voltage.

## Features

- **Current Measurement**: Measures current through a shunt resistor
- **Voltage Measurement**: Measures both bus voltage and shunt voltage
- **Power Calculation**: Calculates power consumption
- **High Precision**: 12-bit ADC resolution
- **I2C Interface**: Simple I2C communication
- **Configurable**: Adjustable gain and measurement ranges

## Hardware Connection

Connect the INA219 to your ESP32 as follows:

| INA219 Pin | ESP32 Pin | Description |
|------------|-----------|-------------|
| VCC        | 3.3V      | Power supply |
| GND        | GND       | Ground      |
| SDA        | GPIO 21   | I2C Data    |
| SCL        | GPIO 22   | I2C Clock   |
| VIN+       | Load +    | Positive supply to load |
| VIN-       | Load -    | Negative supply to load |
| V-         | GND       | Ground reference |

## Usage

### 1. Initialize the INA219

```c
#include "ina219.h"
#include "driver/i2c.h"

// I2C configuration
#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_NUM       0
#define I2C_MASTER_FREQ_HZ   400000

// INA219 configuration
#define SHUNT_RESISTOR_OHMS  0.1f  // 100mΩ shunt resistor
#define MAX_CURRENT_A        3.2f  // Maximum expected current

// Initialize I2C
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
};
i2c_param_config(I2C_MASTER_NUM, &conf);
i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

// Initialize INA219
ina219_handle_t ina219;
esp_err_t ret = ina219_init(&ina219, I2C_MASTER_NUM, INA219_I2C_ADDR_DEFAULT, SHUNT_RESISTOR_OHMS);
if (ret != ESP_OK) {
    printf("Failed to initialize INA219: %s\n", esp_err_to_name(ret));
    return;
}

// Set calibration for maximum expected current
ret = ina219_set_calibration(&ina219, MAX_CURRENT_A, SHUNT_RESISTOR_OHMS);
if (ret != ESP_OK) {
    printf("Failed to calibrate INA219: %s\n", esp_err_to_name(ret));
    return;
}
```

### 2. Read Measurements

```c
float bus_voltage, shunt_voltage, current, power;

// Read bus voltage (V)
esp_err_t ret = ina219_get_bus_voltage(&ina219, &bus_voltage);
if (ret == ESP_OK) {
    printf("Bus voltage: %.3f V\n", bus_voltage);
}

// Read shunt voltage (V)
ret = ina219_get_shunt_voltage(&ina219, &shunt_voltage);
if (ret == ESP_OK) {
    printf("Shunt voltage: %.3f mV\n", shunt_voltage * 1000.0f);
}

// Read current (A)
ret = ina219_get_current(&ina219, &current);
if (ret == ESP_OK) {
    printf("Current: %.3f mA\n", current * 1000.0f);
}

// Read power (W)
ret = ina219_get_power(&ina219, &power);
if (ret == ESP_OK) {
    printf("Power: %.3f mW\n", power * 1000.0f);
}
```

## API Reference

### Functions

#### `ina219_init()`
Initialize the INA219 sensor.

```c
esp_err_t ina219_init(ina219_handle_t *handle, i2c_port_t i2c_port, uint8_t i2c_addr, float shunt_resistor_ohms);
```

**Parameters:**
- `handle`: Pointer to INA219 handle structure
- `i2c_port`: I2C port number
- `i2c_addr`: I2C address (use `INA219_I2C_ADDR_DEFAULT`)
- `shunt_resistor_ohms`: Shunt resistor value in ohms

**Returns:** `ESP_OK` on success, error code on failure

#### `ina219_set_calibration()`
Set calibration for maximum expected current.

```c
esp_err_t ina219_set_calibration(ina219_handle_t *handle, float max_expected_current, float shunt_resistor_ohms);
```

**Parameters:**
- `handle`: Pointer to INA219 handle structure
- `max_expected_current`: Maximum expected current in amperes
- `shunt_resistor_ohms`: Shunt resistor value in ohms

**Returns:** `ESP_OK` on success, error code on failure

#### `ina219_get_bus_voltage()`
Read bus voltage.

```c
esp_err_t ina219_get_bus_voltage(ina219_handle_t *handle, float *voltage);
```

**Parameters:**
- `handle`: Pointer to INA219 handle structure
- `voltage`: Pointer to store voltage value (in volts)

**Returns:** `ESP_OK` on success, error code on failure

#### `ina219_get_shunt_voltage()`
Read shunt voltage.

```c
esp_err_t ina219_get_shunt_voltage(ina219_handle_t *handle, float *voltage);
```

**Parameters:**
- `handle`: Pointer to INA219 handle structure
- `voltage`: Pointer to store voltage value (in volts)

**Returns:** `ESP_OK` on success, error code on failure

#### `ina219_get_current()`
Read current.

```c
esp_err_t ina219_get_current(ina219_handle_t *handle, float *current);
```

**Parameters:**
- `handle`: Pointer to INA219 handle structure
- `current`: Pointer to store current value (in amperes)

**Returns:** `ESP_OK` on success, error code on failure

#### `ina219_get_power()`
Read power.

```c
esp_err_t ina219_get_power(ina219_handle_t *handle, float *power);
```

**Parameters:**
- `handle`: Pointer to INA219 handle structure
- `power`: Pointer to store power value (in watts)

**Returns:** `ESP_OK` on success, error code on failure

### Constants

#### I2C Addresses
- `INA219_I2C_ADDR_DEFAULT`: Default address (0x40)
- `INA219_I2C_ADDR_A0_GND`: Address when A0 is connected to GND
- `INA219_I2C_ADDR_A0_VCC`: Address when A0 is connected to VCC
- `INA219_I2C_ADDR_A0_SDA`: Address when A0 is connected to SDA
- `INA219_I2C_ADDR_A0_SCL`: Address when A0 is connected to SCL

## Configuration

The INA219 can be configured for different measurement ranges and resolutions. The default configuration uses:

- 32V bus voltage range
- ±320mV shunt voltage range
- 12-bit ADC resolution
- Continuous measurement mode

## Error Handling

All functions return `ESP_OK` on success or an appropriate error code on failure. Common error codes include:

- `ESP_ERR_INVALID_ARG`: Invalid parameters
- `ESP_ERR_INVALID_STATE`: INA219 not initialized
- `ESP_ERR_TIMEOUT`: I2C communication timeout
- `ESP_FAIL`: General failure

## Example

See the main application (`hello_world_main.c`) for a complete example that demonstrates:

1. I2C initialization
2. INA219 initialization and calibration
3. Continuous current monitoring
4. Error handling

## Dependencies

- ESP-IDF I2C driver
- FreeRTOS (for delays)
- ESP logging system
