// #include "webserver_task.h"
#include "definitions.h"
#include "monitoring_task.h"
#include "data_logger.h"
#include "config_manager.h"
#include "wifi_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "webserver";
static httpd_handle_t server = NULL;

// Function to serve static files from SPIFFS
static esp_err_t serve_file(httpd_req_t *req, const char *file_path, const char *content_type) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, content_type);
    
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            break;
        }
    }
    
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); // End of chunks
    return ESP_OK;
}

// Handler for the main page
static esp_err_t root_handler(httpd_req_t *req) {
    return serve_file(req, "/spiffs/index.html", "text/html");
}

// Handler for static files (CSS, JS, etc.)
static esp_err_t static_file_handler(httpd_req_t *req) {
    char file_path[256];
    size_t uri_len = strlen(req->uri);
    if (uri_len + 8 > sizeof(file_path)) { // 8 for "/spiffs" + null terminator
        ESP_LOGE(TAG, "URI too long: %s", req->uri);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    strcpy(file_path, "/spiffs");
    strncat(file_path, req->uri, sizeof(file_path) - 8);
    
    // Determine content type based on file extension
    const char *content_type = "text/plain";
    if (strstr(req->uri, ".html")) {
        content_type = "text/html";
    } else if (strstr(req->uri, ".css")) {
        content_type = "text/css";
    } else if (strstr(req->uri, ".js")) {
        content_type = "application/javascript";
    } else if (strstr(req->uri, ".png")) {
        content_type = "image/png";
    } else if (strstr(req->uri, ".jpg") || strstr(req->uri, ".jpeg")) {
        content_type = "image/jpeg";
    } else if (strstr(req->uri, ".gif")) {
        content_type = "image/gif";
    } else if (strstr(req->uri, ".svg")) {
        content_type = "image/svg+xml";
    }
    
    return serve_file(req, file_path, content_type);
}

// Handler for sensor data API
static esp_err_t sensor_data_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    // Get sensor data with mutex protection
    sensor_data_t sensor_data;
    if (xSemaphoreTake(g_sensor_data_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        sensor_data = g_sensor_data;
        xSemaphoreGive(g_sensor_data_mutex);
    } else {
        // Return error if mutex timeout
        const char* error_json = "{\"error\": \"Failed to get sensor data\"}";
        return httpd_resp_send(req, error_json, strlen(error_json));
    }
    
    // Create JSON response
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "bus_voltage", sensor_data.bus_voltage);
    cJSON_AddNumberToObject(json, "shunt_voltage", sensor_data.shunt_voltage);
    cJSON_AddNumberToObject(json, "current", sensor_data.current);
    cJSON_AddNumberToObject(json, "power", sensor_data.power);
    cJSON_AddNumberToObject(json, "raw_bus", sensor_data.raw_bus);
    cJSON_AddNumberToObject(json, "raw_shunt", sensor_data.raw_shunt);
    cJSON_AddNumberToObject(json, "raw_current", sensor_data.raw_current);
    cJSON_AddNumberToObject(json, "raw_power", sensor_data.raw_power);
    cJSON_AddNumberToObject(json, "timestamp", sensor_data.timestamp);
    cJSON_AddNumberToObject(json, "bus_avg", sensor_data.bus_avg);
    cJSON_AddNumberToObject(json, "shunt_avg", sensor_data.shunt_avg);
    cJSON_AddNumberToObject(json, "current_avg", sensor_data.current_avg);
    cJSON_AddNumberToObject(json, "power_avg", sensor_data.power_avg);
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// Handler for log status API
static esp_err_t log_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "enabled", is_logging_enabled());
    cJSON_AddStringToObject(json, "filename", get_log_filename());
    cJSON_AddNumberToObject(json, "size", get_log_file_size());
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// Handler for log toggle API
static esp_err_t log_toggle_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    cJSON *enabled = cJSON_GetObjectItem(json, "enabled");
    if (cJSON_IsBool(enabled)) {
        set_logging_enabled(cJSON_IsTrue(enabled));
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);
        char *response_str = cJSON_Print(response);
        httpd_resp_send(req, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(response);
    } else {
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", "Invalid request");
        char *response_str = cJSON_Print(response);
        httpd_resp_send(req, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(response);
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

// Handler for log clear API
static esp_err_t log_clear_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    esp_err_t ret = clear_log_file();
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", ret == ESP_OK);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(json, "error", "Failed to clear log file");
    }
    
    char *json_string = cJSON_Print(json);
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

// Handler for log download API
static esp_err_t log_download_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=sensor_data.csv");
    
    FILE *f = fopen(get_log_filename(), "r");
    if (f == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            break;
        }
    }
    
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // End of chunks
    return ESP_OK;
}

// Handler for new log file API
static esp_err_t log_new_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    esp_err_t ret = create_new_log_file();
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", ret == ESP_OK);
    if (ret != ESP_OK) {
        cJSON_AddStringToObject(json, "error", "Failed to create new log file");
    }
    
    char *json_string = cJSON_Print(json);
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

// Handler for getting configuration
static esp_err_t config_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    const config_data_t* config = get_config();
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "log_interval_ms", config->log_interval_ms);
    cJSON_AddStringToObject(json, "wifi_ssid", config->wifi_ssid);
    cJSON_AddStringToObject(json, "wifi_password", config->wifi_password);
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// Handler for setting configuration
static esp_err_t config_set_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Parse configuration data
    config_data_t new_config;
    cJSON *log_interval = cJSON_GetObjectItem(json, "log_interval_ms");
    cJSON *wifi_ssid = cJSON_GetObjectItem(json, "wifi_ssid");
    cJSON *wifi_password = cJSON_GetObjectItem(json, "wifi_password");
    
    if (cJSON_IsNumber(log_interval) && cJSON_IsString(wifi_ssid) && cJSON_IsString(wifi_password)) {
        new_config.log_interval_ms = log_interval->valueint;
        strncpy(new_config.wifi_ssid, wifi_ssid->valuestring, sizeof(new_config.wifi_ssid) - 1);
        new_config.wifi_ssid[sizeof(new_config.wifi_ssid) - 1] = '\0';
        strncpy(new_config.wifi_password, wifi_password->valuestring, sizeof(new_config.wifi_password) - 1);
        new_config.wifi_password[sizeof(new_config.wifi_password) - 1] = '\0';
        
        esp_err_t config_ret = update_config(&new_config);
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", config_ret == ESP_OK);
        if (config_ret != ESP_OK) {
            cJSON_AddStringToObject(response, "error", "Failed to update configuration");
        }
        
        char *response_str = cJSON_Print(response);
        httpd_resp_send(req, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(response);
    } else {
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", "Invalid configuration data");
        char *response_str = cJSON_Print(response);
        httpd_resp_send(req, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(response);
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

// Handler for device restart
static esp_err_t restart_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", true);
    cJSON_AddStringToObject(json, "message", "Device restart initiated");
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    // Give some time for the response to be sent before restarting
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Restart the device
    esp_restart();
    
    return ret;
}

// Handler for WiFi status API
static esp_err_t wifi_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", get_wifi_status());
    cJSON_AddBoolToObject(json, "ap_mode", is_ap_mode());
    
    if (is_ap_mode()) {
        cJSON_AddStringToObject(json, "ap_ssid", AP_SSID);
        cJSON_AddStringToObject(json, "ap_password", AP_PASS);
        cJSON_AddStringToObject(json, "ap_ip", AP_IP);
    }
    
    char *json_string = cJSON_Print(json);
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

// Start the HTTP server
static esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 12;
    config.max_resp_headers = 8;
    config.max_open_sockets = 7;
    config.max_resp_headers = 8;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        // Register static file handler for all other files
        httpd_uri_t static_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = static_file_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &static_uri);
        
        httpd_uri_t sensor_uri = {
            .uri = "/api/sensor-data",
            .method = HTTP_GET,
            .handler = sensor_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &sensor_uri);
        
        // Logging API endpoints
        httpd_uri_t log_status_uri = {
            .uri = "/api/log-status",
            .method = HTTP_GET,
            .handler = log_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &log_status_uri);
        
        httpd_uri_t log_toggle_uri = {
            .uri = "/api/log-toggle",
            .method = HTTP_POST,
            .handler = log_toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &log_toggle_uri);
        
        httpd_uri_t log_clear_uri = {
            .uri = "/api/log-clear",
            .method = HTTP_POST,
            .handler = log_clear_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &log_clear_uri);
        
        httpd_uri_t log_download_uri = {
            .uri = "/api/log-download",
            .method = HTTP_GET,
            .handler = log_download_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &log_download_uri);
        
        httpd_uri_t log_new_uri = {
            .uri = "/api/log-new",
            .method = HTTP_POST,
            .handler = log_new_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &log_new_uri);
        
        // Configuration API endpoints
        httpd_uri_t config_get_uri = {
            .uri = "/api/config",
            .method = HTTP_GET,
            .handler = config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_get_uri);
        
        httpd_uri_t config_set_uri = {
            .uri = "/api/config",
            .method = HTTP_POST,
            .handler = config_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_set_uri);
        
        // Restart API endpoint
        httpd_uri_t restart_uri = {
            .uri = "/api/restart",
            .method = HTTP_POST,
            .handler = restart_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &restart_uri);
        
        // WiFi status API endpoint
        httpd_uri_t wifi_status_uri = {
            .uri = "/api/wifi-status",
            .method = HTTP_GET,
            .handler = wifi_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_status_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error starting server!");
    return ESP_FAIL;
}

// Stop the HTTP server
static void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

// Webserver task
void webserver_task(void *pvParameters) {
    ESP_LOGI(TAG, "Webserver task started");
    
    // Start the web server
    if (start_webserver() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        vTaskDelete(NULL);
        return;
    }
    
    // Keep the task running
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Initialize webserver task
void init_webserver_task(void) {
    BaseType_t ret = xTaskCreate(webserver_task, "webserver_task", 8192, NULL, 4, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create webserver task");
        return;
    }
    
    ESP_LOGI(TAG, "Webserver task initialized");
}
