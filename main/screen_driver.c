#include "definitions.h"
#include "screen_driver.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "screen_driver";

// SPI device handle
static spi_device_handle_t spi_device = NULL;

// Helper function to send command
static void tft_send_cmd(uint8_t cmd) {
    gpio_set_level(TFT_DC_PIN, 0); // Command mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(spi_device, &t);
}

// Helper function to send data
static void tft_send_data(uint8_t data) {
    gpio_set_level(TFT_DC_PIN, 1); // Data mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_transmit(spi_device, &t);
}

// Helper function to send 16-bit data
static void tft_send_data16(uint16_t data) {
    gpio_set_level(TFT_DC_PIN, 1); // Data mode
    uint8_t buffer[2] = {(data >> 8) & 0xFF, data & 0xFF};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = buffer,
    };
    spi_device_transmit(spi_device, &t);
}

// Set address window
static void tft_set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    tft_send_cmd(0x2A); // Column address set
    tft_send_data16(x1);
    tft_send_data16(x2);
    
    tft_send_cmd(0x2B); // Row address set
    tft_send_data16(y1);
    tft_send_data16(y2);
    
    tft_send_cmd(0x2C); // Memory write
}

// Initialize TFT screen
esp_err_t tft_init(void) {
    ESP_LOGI(TAG, "Initializing TFT screen");
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TFT_DC_PIN) | (1ULL << TFT_RST_PIN) | (1ULL << TFT_BL_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    
    // Configure CS pin
    io_conf.pin_bit_mask = (1ULL << TFT_CS_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    
    // Initialize pins
    gpio_set_level(TFT_CS_PIN, 1);
    gpio_set_level(TFT_DC_PIN, 0);
    gpio_set_level(TFT_RST_PIN, 1);
    gpio_set_level(TFT_BL_PIN, 1); // Backlight on
    
    // Configure SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = TFT_MOSI_PIN,
        .sclk_io_num = TFT_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SCREEN_WIDTH * SCREEN_HEIGHT * 2,
    };
    
    esp_err_t ret = spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return ret;
    }
    
    // Configure SPI device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,
        .spics_io_num = TFT_CS_PIN,
        .queue_size = 7,
    };
    
    ret = spi_bus_add_device(TFT_SPI_HOST, &devcfg, &spi_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return ret;
    }
    
    // Reset display
    gpio_set_level(TFT_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    // Initialize display (ST7735 commands)
    tft_send_cmd(0x01); // Software reset
    vTaskDelay(pdMS_TO_TICKS(150));
    
    tft_send_cmd(0x11); // Sleep out
    vTaskDelay(pdMS_TO_TICKS(500));
    
    tft_send_cmd(0x3A); // Pixel format
    tft_send_data(0x05); // 16-bit color
    
    // Set display rotation to landscape
    tft_send_cmd(0x36); // Memory access control
    tft_send_data(0x60); // Landscape mode (MY=0, MX=1, MV=1, ML=0, BGR=0, MH=0)
    
    tft_send_cmd(0x29); // Display on
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Clear screen
    tft_fill_screen(COLOR_BLACK);
    
    ESP_LOGI(TAG, "TFT screen initialized successfully");
    return ESP_OK;
}

// Fill entire screen with color
void tft_fill_screen(uint16_t color) {
    tft_set_addr_window(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        tft_send_data16(color);
    }
}

// Draw a pixel
void tft_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    
    tft_set_addr_window(x, y, x, y);
    tft_send_data16(color);
}

// Draw a line
void tft_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        tft_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Draw a rectangle
void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    tft_draw_line(x, y, x + w - 1, y, color);
    tft_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    tft_draw_line(x + w - 1, y + h - 1, x, y + h - 1, color);
    tft_draw_line(x, y + h - 1, x, y, color);
}

// Fill a rectangle
void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    
    tft_set_addr_window(x, y, x + w - 1, y + h - 1);
    
    for (int i = 0; i < w * h; i++) {
        tft_send_data16(color);
    }
}




// Draw a circle
void tft_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    tft_draw_pixel(x0, y0 + r, color);
    tft_draw_pixel(x0, y0 - r, color);
    tft_draw_pixel(x0 + r, y0, color);
    tft_draw_pixel(x0 - r, y0, color);
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        tft_draw_pixel(x0 + x, y0 + y, color);
        tft_draw_pixel(x0 - x, y0 + y, color);
        tft_draw_pixel(x0 + x, y0 - y, color);
        tft_draw_pixel(x0 - x, y0 - y, color);
        tft_draw_pixel(x0 + y, y0 + x, color);
        tft_draw_pixel(x0 - y, y0 + x, color);
        tft_draw_pixel(x0 + y, y0 - x, color);
        tft_draw_pixel(x0 - y, y0 - x, color);
    }
}

// Fill a circle
void tft_fill_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    tft_draw_line(x0, y0 - r, x0, y0 + r, color);
    
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        tft_draw_line(x0 - x, y0 - y, x0 + x, y0 - y, color);
        tft_draw_line(x0 - x, y0 + y, x0 + x, y0 + y, color);
        tft_draw_line(x0 - y, y0 - x, x0 + y, y0 - x, color);
        tft_draw_line(x0 - y, y0 + x, x0 + y, y0 + x, color);
    }
}

// Draw a character
void tft_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 126) c = 32; // Replace invalid chars with space
    
    const uint8_t* font_data = font5x7[c - 32];
    
    // Draw background first if different from text color
    if (bg != color) {
        tft_fill_rect(x, y, 6 * size, 8 * size, bg);
    }
    
    // Draw character pixels
    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = font_data[i];
        for (int8_t j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                if (size == 1) {
                    tft_draw_pixel(x + i, y + j, color);
                } else {
                    tft_fill_rect(x + i * size, y + j * size, size, size, color);
                }
            }
        }
    }
}

// Draw a string
void tft_draw_string(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;
    
    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            cursor_y += 9 * size; // Line spacing
        } else {
            tft_draw_char(cursor_x, cursor_y, *str, color, bg, size);
            cursor_x += 6 * size; // Character spacing (5 pixels + 1 pixel gap)
            if (cursor_x > SCREEN_WIDTH - 6 * size) {
                cursor_x = x;
                cursor_y += 9 * size;
            }
        }
        str++;
    }
}

// Draw centered string
void tft_draw_string_centered(int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t len = strlen(str);
    int16_t x = (SCREEN_WIDTH - len * 6 * size) / 2; // Updated for proper character spacing
    tft_draw_string(x, y, str, color, bg, size);
}

// Convert RGB to RGB565
uint16_t tft_color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Set rotation
void tft_set_rotation(uint8_t rotation) {
    display_rotation = rotation & 3; // Keep only 2 bits
    
    tft_send_cmd(0x36); // Memory access control
    
    switch (display_rotation) {
        case 0: // Portrait
            tft_send_data(0x00); // MY=0, MX=0, MV=0, ML=0, BGR=0, MH=0
            break;
        case 1: // Landscape
            tft_send_data(0x60); // MY=0, MX=1, MV=1, ML=0, BGR=0, MH=0
            break;
        case 2: // Portrait upside down
            tft_send_data(0xC0); // MY=1, MX=1, MV=0, ML=0, BGR=0, MH=0
            break;
        case 3: // Landscape upside down
            tft_send_data(0xA0); // MY=1, MX=0, MV=1, ML=0, BGR=0, MH=0
            break;
    }
}

// Display WiFi status
void tft_display_wifi_status(const char* status, const char* ip) {
    tft_fill_screen(COLOR_BLACK);
    
    // Title
    tft_draw_string_centered(15, "WiFi Status", COLOR_WHITE, COLOR_BLACK, FONT_SMALL);
    
    // Status
    tft_draw_string_centered(45, status, COLOR_YELLOW, COLOR_BLACK, FONT_SMALL);
    
    // IP address
    if (ip && strlen(ip) > 0) {
        tft_draw_string_centered(75, "IP:", COLOR_WHITE, COLOR_BLACK, FONT_SMALL);
        tft_draw_string_centered(95, ip, COLOR_CYAN, COLOR_BLACK, FONT_SMALL);
    }
    
    // Instructions
    tft_draw_string_centered(115, "Web Interface Available", COLOR_GREEN, COLOR_BLACK, FONT_SMALL);
}

// Display sensor data
void tft_display_sensor_data(float voltage, float current, float power) {
    tft_fill_screen(COLOR_BLACK);
    
    // Title
    tft_draw_string_centered(10, "Power Monitor", COLOR_WHITE, COLOR_BLACK, FONT_SMALL);
    
    // Voltage
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "V: %.2fV", voltage);
    tft_draw_string_centered(40, buffer, COLOR_GREEN, COLOR_BLACK, FONT_SMALL);
    
    // Current
    snprintf(buffer, sizeof(buffer), "I: %.1fmA", current);
    tft_draw_string_centered(70, buffer, COLOR_BLUE, COLOR_BLACK, FONT_SMALL);
    
    // Power
    snprintf(buffer, sizeof(buffer), "P: %.1fmW", power);
    tft_draw_string_centered(100, buffer, COLOR_RED, COLOR_BLACK, FONT_SMALL);
    
    // Status indicator
    tft_draw_string_centered(120, "Monitoring...", COLOR_YELLOW, COLOR_BLACK, FONT_SMALL);
}

// Display AP mode info
void tft_display_ap_info(const char* ssid, const char* password, const char* ip) {
    tft_fill_screen(COLOR_BLACK);
    
    // Title
    tft_draw_string_centered(10, "Config Mode", COLOR_WHITE, COLOR_BLACK, FONT_LARGE);
    
    // SSID
    tft_draw_string_centered(35, "SSID:", COLOR_YELLOW, COLOR_BLACK, FONT_SMALL);
    tft_draw_string_centered(55, ssid, COLOR_CYAN, COLOR_BLACK, FONT_SMALL);
    
    // Password
    tft_draw_string_centered(80, "Pass:", COLOR_YELLOW, COLOR_BLACK, FONT_SMALL);
    tft_draw_string_centered(100, password, COLOR_CYAN, COLOR_BLACK, FONT_SMALL);
    
    // IP
    tft_draw_string_centered(120, "IP:", COLOR_YELLOW, COLOR_BLACK, FONT_SMALL);
    tft_draw_string_centered(125, ip, COLOR_CYAN, COLOR_BLACK, FONT_SMALL);
}

// Display test pattern for debugging
void tft_display_test_pattern(void) {
    tft_fill_screen(COLOR_BLACK);
    
    // Test different font sizes
    tft_draw_string_centered(10, "Font Test", COLOR_WHITE, COLOR_BLACK, FONT_MEDIUM);
    tft_draw_string_centered(30, "Small Font", COLOR_RED, COLOR_BLACK, FONT_SMALL);
    tft_draw_string_centered(45, "Medium Font", COLOR_GREEN, COLOR_BLACK, FONT_MEDIUM);
    tft_draw_string_centered(65, "Large Font", COLOR_BLUE, COLOR_BLACK, FONT_MEDIUM);
    
    // Test individual characters
    tft_draw_string_centered(85, "ABCDEFGHIJKLMNOP", COLOR_YELLOW, COLOR_BLACK, FONT_MEDIUM);
    tft_draw_string_centered(100, "abcdefghijklmnop", COLOR_CYAN, COLOR_BLACK, FONT_MEDIUM);
    tft_draw_string_centered(115, "0123456789", COLOR_MAGENTA, COLOR_BLACK, FONT_MEDIUM);
}

// Display log status
void tft_display_log_status(bool logging_enabled) {
    if (logging_enabled) {
    // create a red circle on the right top corner of the screen
        tft_fill_circle(SCREEN_WIDTH - 10, 10, 5, COLOR_RED);
    }
    else {
        // create a green circle on the right top corner of the screen
        tft_fill_circle(SCREEN_WIDTH - 10, 10, 5, COLOR_WHITE);
    }
}