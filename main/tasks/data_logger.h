#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "monitoring_task.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Function declarations
esp_err_t init_data_logger(void);
void log_sensor_data(const sensor_data_t *data);
void set_logging_enabled(bool enabled);
bool is_logging_enabled(void);
const char* get_log_filename(void);
size_t get_log_file_size(void);
esp_err_t clear_log_file(void);
esp_err_t create_new_log_file(void);

#endif // DATA_LOGGER_H
