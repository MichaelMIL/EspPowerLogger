#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Function declarations
esp_err_t init_time_sync(void);
esp_err_t init_sntp(void);
esp_err_t wait_for_time_sync(uint32_t timeout_ms);
void get_current_time_string(char *buffer, size_t buffer_size);
void get_current_time_iso_string(char *buffer, size_t buffer_size);
uint64_t get_current_timestamp_ms(void);
void stop_sntp(void);

#endif // TIME_SYNC_H
