# 1.8" TFT Screen Pinout Configuration

## Screen Pin Layout
```
┌─────────────────────────────────┐
│  1.8" TFT SPI 128x160 V1.1     │
│                                 │
│  ┌─────────────────────────────┐ │
│  │                             │ │
│  │        Display Area         │ │
│  │       (128 x 160)           │ │
│  │                             │ │
│  └─────────────────────────────┘ │
│                                 │
│  Pin Layout:                    │
│  ┌─────────────────────────────┐ │
│  │ 1: CS  2: RST 3: A0         │ │
│  │ 4: SDA 5: SCL 6: LED        │ │
│  └─────────────────────────────┘ │
└─────────────────────────────────┘
```

## Pin Mapping

| Screen Pin | Screen Name | ESP32-S3 Pin | ESP32-S3 Name | Function |
|------------|-------------|--------------|---------------|----------|
| 1          | CS          | GPIO 36      | CS            | Chip Select |
| 2          | RST         | GPIO 37      | RST           | Reset |
| 3          | A0          | GPIO 38      | DC            | Data/Command |
| 4          | SDA         | GPIO 39      | MOSI          | Master Out Slave In |
| 5          | SCL         | GPIO 40      | SCLK          | Serial Clock |
| 6          | LED         | GPIO 41      | Backlight     | Backlight Control |

## Wiring Instructions

### ESP32-S3 to TFT Screen Connections:
```
ESP32-S3          TFT Screen
--------          ----------
GPIO 36    ---->  CS   (Pin 1)
GPIO 37    ---->  RST  (Pin 2)
GPIO 38    ---->  A0   (Pin 3)
GPIO 39    ---->  SDA  (Pin 4)
GPIO 40    ---->  SCL  (Pin 5)
GPIO 41    ---->  LED  (Pin 6)
3.3V       ---->  VCC
GND        ---->  GND
```

## Power Requirements
- **VCC**: 3.3V
- **GND**: Ground
- **Current**: ~20-50mA (depending on backlight)

## SPI Configuration
- **SPI Host**: SPI2_HOST
- **Clock Speed**: 10 MHz
- **Mode**: 0 (CPOL=0, CPHA=0)
- **Data Format**: 16-bit RGB565

## Display Specifications
- **Resolution**: 160 x 128 pixels (Landscape Mode)
- **Controller**: ST7735
- **Color Depth**: 16-bit (65,536 colors)
- **Interface**: SPI
- **Backlight**: LED (PWM controllable)

## Notes
- All pins are 3.3V tolerant
- No pull-up resistors required
- Backlight can be controlled via PWM on GPIO 41
- Screen supports partial refresh for better performance
- Default orientation is landscape (160x128)
- Rotation can be changed using tft_set_rotation() function
