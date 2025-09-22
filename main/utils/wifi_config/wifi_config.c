#include "utils/wifi_config/wifi_config.h"
#include "utils/config_manager/config_manager.h"
#include "utils/definitions/definitions.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "tasks/screen_task/screen_task.h"

static const char *TAG = "wifi_config";

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about
// two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < WIFI_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    // Update screen with IP address
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
    screen_update_wifi_status("Connected", ip_str);
    screen_set_mode(SCREEN_MODE_SENSOR_DATA);
  }
}

void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  // Get WiFi credentials from configuration manager
  const config_data_t *config = get_config();

  wifi_config_t wifi_config = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
              .pmf_cfg = {.capable = true, .required = false},
          },
  };

  // Copy SSID and password from configuration
  strncpy((char *)wifi_config.sta.ssid, config->wifi_ssid,
          sizeof(wifi_config.sta.ssid) - 1);
  wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
  strncpy((char *)wifi_config.sta.password, config->wifi_password,
          sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
  // connection failed for the maximum number of retries (WIFI_FAIL_BIT)
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  // xEventGroupWaitBits() returns the bits before the call returned, hence we
  // can test which event actually happened.
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", config->wifi_ssid);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", config->wifi_ssid);
    screen_update_wifi_status("Connection Failed", "");
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

// Start WiFi Access Point for configuration
static void wifi_init_ap(void) {
  ESP_LOGI(TAG, "Starting WiFi Access Point for configuration");

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
      .ap =
          {
              .ssid = AP_SSID,
              .ssid_len = strlen(AP_SSID),
              .channel = AP_CHANNEL,
              .password = AP_PASS,
              .max_connection = AP_MAX_CONNECTIONS,
              .authmode = WIFI_AUTH_WPA_WPA2_PSK,
              .pmf_cfg =
                  {
                      .required = false,
                  },
          },
  };

  if (strlen(AP_PASS) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s", AP_SSID, AP_PASS);
  ESP_LOGI(TAG, "Connect to this network to configure WiFi settings");
  ESP_LOGI(TAG, "Access the web interface at: http://192.168.4.1");

  // Update screen for AP mode
  screen_update_ap_config(AP_SSID, AP_PASS, "192.168.4.1");
  screen_set_mode(SCREEN_MODE_AP_CONFIG);
}

void init_wifi(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_init_sta();

  // Check if we failed to connect to WiFi
  EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
  if (bits & WIFI_FAIL_BIT) {
    ESP_LOGW(TAG, "Failed to connect to WiFi, starting configuration AP");
    s_ap_mode = true;

    // Stop STA mode and start AP mode
    esp_wifi_stop();
    esp_wifi_deinit();

    // Create AP netif (event loop already exists)
    esp_netif_create_default_wifi_ap();

    wifi_init_ap();
  }
}

// Check if we're in AP mode
bool is_ap_mode(void) { return s_ap_mode; }

// Get current WiFi status
const char *get_wifi_status(void) {
  if (s_ap_mode) {
    return "Configuration Mode (AP)";
  } else {
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    if (bits & WIFI_CONNECTED_BIT) {
      return "Connected to WiFi";
    } else if (bits & WIFI_FAIL_BIT) {
      return "WiFi Connection Failed";
    } else {
      return "Connecting to WiFi...";
    }
  }
}
