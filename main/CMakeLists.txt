idf_component_register(SRCS "hello_world_main.c" "definitions.c" "tasks/monitoring_task.c" "tasks/webserver_simple.c" "tasks/data_logger.c" "tasks/time_sync.c" "tasks/screen_task.c" "wifi_config.c" "config_manager.c" "screen_driver.c"
                       PRIV_REQUIRES spi_flash driver ina219 esp_http_server json esp_wifi esp_event nvs_flash esp_netif esp_timer spiffs lwip
                       INCLUDE_DIRS "utils" "tasks" ".")
