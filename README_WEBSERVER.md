# Power Consumption Monitor with Web Interface

This project has been converted from a simple monitoring loop to a task-based system with a web interface for real-time power consumption monitoring.

## Features

- **Continuous Monitoring**: INA219 sensor data is read continuously in a background task
- **Web Interface**: Beautiful, responsive web interface accessible via any device on the network
- **Real-time Updates**: Auto-refreshing display of sensor data every 5 seconds
- **JSON API**: RESTful API endpoint for programmatic access to sensor data
- **Data Filtering**: Exponential moving average filtering for stable readings
- **Thread Safety**: Mutex-protected shared data between monitoring and webserver tasks

## Hardware Requirements

- ESP32-S3 (or compatible ESP32 variant)
- INA219 current sensor
- WiFi network access

## Software Architecture

### Tasks

1. **Main Task**: Initializes all components and keeps the system running
2. **Monitoring Task**: Continuously reads INA219 sensor data and applies filtering
3. **Webserver Task**: Serves the web interface and API endpoints

### Files Structure

```
main/
├── hello_world_main.c          # Main application entry point
├── definitions.c               # Hardware definitions and INA219 instance
├── utils/
│   └── utils.c                # I2C and INA219 initialization utilities
├── tasks/
│   ├── monitoring_task.c      # Background sensor monitoring task
│   ├── monitoring_task.h      # Monitoring task header
│   ├── webserver_task.c       # HTTP server and web interface
│   └── webserver_task.h       # Webserver task header
├── wifi_config.c              # WiFi connection management
├── wifi_config.h              # WiFi configuration header
└── CMakeLists.txt             # Build configuration
```

## Configuration

### WiFi Setup

Before building, update the WiFi credentials in `main/wifi_config.c`:

```c
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
```

### Hardware Configuration

The I2C pins and INA219 settings are configured in `main/definitions.c`. Default configuration uses:
- SDA: GPIO 5
- SCL: GPIO 4
- I2C Frequency: 50kHz

## Building and Flashing

1. Configure your WiFi credentials in `wifi_config.c`
2. Build the project:
   ```bash
   idf.py build
   ```
3. Flash to your ESP32:
   ```bash
   idf.py flash monitor
   ```

## Usage

1. **Serial Monitor**: Watch the initialization process and debug information
2. **Web Interface**: 
   - Connect to the same WiFi network as the ESP32
   - Open a web browser and navigate to `http://<ESP32_IP>`
   - The interface will show real-time sensor data with auto-refresh
3. **API Access**: 
   - JSON data available at `http://<ESP32_IP>/api/sensor-data`
   - Returns current sensor readings in JSON format

## Web Interface Features

- **Real-time Display**: Shows bus voltage, shunt voltage, current, and power
- **Raw Data**: Displays raw sensor readings for debugging
- **Filtered Data**: Shows exponentially filtered values for stability
- **Auto-refresh**: Automatically updates every 5 seconds
- **Manual Refresh**: Button to manually refresh data
- **Responsive Design**: Works on desktop and mobile devices
- **Status Indicators**: Shows connection status

## API Response Format

The `/api/sensor-data` endpoint returns JSON data in this format:

```json
{
  "bus_voltage": 5.123,
  "shunt_voltage": 10.5,
  "current": 105.2,
  "power": 539.1,
  "raw_bus": 1280,
  "raw_shunt": 1050,
  "raw_current": 1052,
  "raw_power": 269,
  "timestamp": 1234567890,
  "bus_avg": 5.120,
  "shunt_avg": 10.3,
  "current_avg": 104.8,
  "power_avg": 536.2
}
```

## Troubleshooting

1. **WiFi Connection Issues**: 
   - Check SSID and password in `wifi_config.c`
   - Ensure ESP32 is within range of the WiFi network
   - Check serial output for connection status

2. **Sensor Reading Issues**:
   - Verify I2C wiring (SDA/SCL connections)
   - Check power supply to INA219 (3.3V)
   - Ensure pull-up resistors are present on I2C lines

3. **Web Interface Not Accessible**:
   - Check that ESP32 connected to WiFi successfully
   - Verify IP address in serial output
   - Ensure device is on the same network as ESP32

## Performance Notes

- Monitoring task runs every 1 second
- Webserver handles multiple concurrent connections
- Data is filtered using exponential moving average (α=0.3)
- Memory usage is optimized for ESP32 constraints
- Web interface is lightweight and loads quickly

## Future Enhancements

- Data logging to SD card or cloud
- Historical data visualization
- Alert thresholds and notifications
- OTA (Over-The-Air) updates
- Multiple sensor support
- MQTT integration for IoT platforms
