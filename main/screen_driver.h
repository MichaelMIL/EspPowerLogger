#ifndef SCREEN_DRIVER_H
#define SCREEN_DRIVER_H

#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <stdint.h>

// Screen dimensions (landscape mode)
#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 128

// TFT Screen Pin Configuration
// Screen pins: CS, RST, A0, SDA, SCL, LED
// ESP32-S3 pins: CS, RST, DC, MOSI, SCLK, Backlight
#define TFT_SPI_HOST    SPI2_HOST
#define TFT_CS_PIN      36      // CS (Chip Select)
#define TFT_RST_PIN     37      // RST (Reset)
#define TFT_DC_PIN      38      // A0 (Data/Command)
#define TFT_MOSI_PIN    39      // SDA (MOSI - Master Out Slave In)
#define TFT_SCLK_PIN    40      // SCL (SCLK - Serial Clock)
#define TFT_BL_PIN      41      // LED (Backlight)

// Colors (RGB565 format)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_ORANGE    0xFC00
#define COLOR_PURPLE    0x780F
#define COLOR_GRAY      0x8410
#define COLOR_DARK_GRAY 0x4208

// Font sizes
#define FONT_SMALL      1
#define FONT_MEDIUM     2
#define FONT_LARGE      3

// Display modes
typedef enum {
    DISPLAY_WIFI_CONNECTING,
    DISPLAY_WIFI_CONNECTED,
    DISPLAY_WIFI_AP_MODE,
    DISPLAY_SENSOR_DATA,
    DISPLAY_CONFIG
} display_mode_t;

// Initialize the TFT screen
esp_err_t tft_init(void);

// Basic drawing functions
void tft_fill_screen(uint16_t color);
void tft_draw_pixel(int16_t x, int16_t y, uint16_t color);
void tft_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void tft_fill_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

// Text functions
void tft_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void tft_draw_string(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);
void tft_draw_string_centered(int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);

// Display functions
void tft_display_wifi_status(const char* status, const char* ip);
void tft_display_sensor_data(float voltage, float current, float power);
void tft_display_ap_info(const char* ssid, const char* password, const char* ip);
void tft_display_test_pattern(void);
void tft_display_log_status(bool logging_enabled);

// Utility functions
uint16_t tft_color565(uint8_t r, uint8_t g, uint8_t b);
void tft_set_rotation(uint8_t rotation);

#endif // SCREEN_DRIVER_H
