#ifndef BUTTON_TASK_H
#define BUTTON_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// Button configuration
#define BUTTON_GPIO_PIN         7  // GPIO pin for the button
#define BUTTON_DEBOUNCE_MS      50  // Debounce time in milliseconds
#define BUTTON_SHORT_PRESS_MS   200  // Short press detection time
#define BUTTON_LONG_PRESS_MS    3000 // Long press detection time (3 seconds)
#define BUTTON_REBOOT_PRESS_MS  15000 // Reboot press detection time (15 seconds)


// Button states
typedef enum {
    BUTTON_RELEASED,
    BUTTON_PRESSED,
    BUTTON_SHORT_PRESS,
    BUTTON_LONG_PRESS
} button_state_t;

// Function declarations
void button_task(void *pvParameters);
void init_button_task(void);
bool is_button_pressed(void);

#endif // BUTTON_TASK_H
