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
- **Dynamic Switching**: Automatically switches between SD card and SPIFFS when card is inserted/removed while running
- **Fallback Storage**: If no SD card is detected, the system automatically falls back to SPIFFS storage
- **Large Capacity**: SD cards can store much more data than SPIFFS (typically 1GB+ vs 1MB)
- **Removable Storage**: SD cards can be easily removed and data can be accessed on other devices
- **SPI Host Separation**: Uses SPI3_HOST to avoid conflicts with the screen driver (which uses SPI2_HOST)
- **Hot Swapping**: SD cards can be inserted or removed while the ESP32 is powered on

## Usage

The SD card functionality is automatically initialized during system startup. The data logger will:

1. First attempt to initialize the SD card
2. If successful, use SD card for logging
3. If SD card is not available, fall back to SPIFFS
4. Log files are created in date-based directories with time-based filenames: `YYYY_MM_DD/HHMMSS.csv`
5. **Dynamic switching**: Continuously monitors card detect pin and switches storage automatically
6. **Hot swapping**: Insert or remove SD card while ESP32 is running - it will switch automatically

## File Locations

- **SD Card**: `/sdcard/YYYY_MM_DD/HHMMSS.csv`
- **SPIFFS**: `/spiffs/YYYY_MM_DD/HHMMSS.csv`

### Directory Structure Example
```
/sdcard/
├── 2025_01_15/
│   ├── 143022.csv  (2:30:22 PM)
│   ├── 143123.csv  (2:31:23 PM)
│   └── 143224.csv  (2:32:24 PM)
├── 2025_01_16/
│   ├── 090000.csv  (9:00:00 AM)
│   └── 090100.csv  (9:01:00 AM)
└── 2025_01_17/
    └── 120000.csv  (12:00:00 PM)
```

## API Functions

- `init_sdcard()`: Initialize SD card driver
- `init_dynamic_sdcard_detection()`: Start dynamic SD card detection task
- `is_sdcard_available()`: Check if SD card is available
- `get_sdcard_mount_point()`: Get SD card mount point
- `get_sdcard_free_space()`: Get available free space
- `get_sdcard_total_space()`: Get total SD card capacity
- `get_current_storage_type()`: Get current storage type (SD Card or SPIFFS)
- `get_storage_type_string()`: Get storage type as human-readable string
- `check_and_switch_storage()`: Check and switch storage if needed

## Troubleshooting

1. **SD Card Not Detected**: Check wiring, especially the card detect pin
2. **Mount Failed**: Ensure SD card is formatted with FAT32
3. **Write Errors**: Check SD card capacity and write protection
4. **SPI Bus Conflicts**: Ensure no other devices are using the same SPI pins
