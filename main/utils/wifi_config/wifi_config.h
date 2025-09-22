#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_wifi.h"
#include "esp_event.h"

// Function declarations
void wifi_init_sta(void);
void init_wifi(void);
bool is_ap_mode(void);
const char* get_wifi_status(void);

#endif // WIFI_CONFIG_H
