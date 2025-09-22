# SD Card Pinout and Configuration

## Pin Configuration

The SD card module is connected to the ESP32-S3 using SPI interface with the following pin assignments:

| SD Card Pin | ESP32-S3 Pin | Function |
|-------------|--------------|----------|
| CS (Chip Select) | GPIO 18 | SPI CS |
| MOSI (Master Out Slave In) | GPIO 16 | SPI MOSI |
| MISO (Master In Slave Out) | GPIO 15 | SPI MISO |
| SCK (Serial Clock) | GPIO 17 | SPI CLK |
| CD (Card Detect) | GPIO 8 | Card Detection |
| VCC | 3.3V | Power |
| GND | GND | Ground |

## Features

- **Automatic Detection**: The system automatically detects if an SD card is present using the card detect (CD) pin
- **Fallback Storage**: If no SD card is detected, the system automatically falls back to SPIFFS storage
- **Large Capacity**: SD cards can store much more data than SPIFFS (typically 1GB+ vs 1MB)
- **Removable Storage**: SD cards can be easily removed and data can be accessed on other devices
- **SPI Host Separation**: Uses HSPI_HOST to avoid conflicts with the screen driver (which uses SPI2_HOST)

## Usage

The SD card functionality is automatically initialized during system startup. The data logger will:

1. First attempt to initialize the SD card
2. If successful, use SD card for logging
3. If SD card is not available, fall back to SPIFFS
4. Log files are created with timestamps in the format: `sensor_log_YYYYMMDD_HHMMSS.csv`

## File Locations

- **SD Card**: `/sdcard/sensor_log_YYYYMMDD_HHMMSS.csv`
- **SPIFFS**: `/spiffs/sensor_log_YYYYMMDD_HHMMSS.csv`

## API Functions

- `init_sdcard()`: Initialize SD card driver
- `is_sdcard_available()`: Check if SD card is available
- `get_sdcard_mount_point()`: Get SD card mount point
- `get_sdcard_free_space()`: Get available free space
- `get_sdcard_total_space()`: Get total SD card capacity
- `get_current_storage_type()`: Get current storage type (SD Card or SPIFFS)
- `get_storage_type_string()`: Get storage type as human-readable string

## Troubleshooting

1. **SD Card Not Detected**: Check wiring, especially the card detect pin
2. **Mount Failed**: Ensure SD card is formatted with FAT32
3. **Write Errors**: Check SD card capacity and write protection
4. **SPI Bus Conflicts**: Ensure no other devices are using the same SPI pins
