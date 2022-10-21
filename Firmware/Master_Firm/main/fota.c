#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_wifi.h"
#include "esp_tls.h"
#include "esp_smartconfig.h"
#include "esp_attr.h"
#include "mqtt_client.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_master.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "button.h"
#include "fota.h"
#include "led.h"
#include "lora.h"
#include "mqtt.h"
#include "sim.h"
#include "smartconfig.h"
#include "wifi.h"
#include "main.h"

static const char *TAG = "FOTA";
extern _status status; 
extern const uint8_t github_cert_pem_start[] asm("_binary_git_ota_pem_start");
extern const uint8_t github_cert_pem_end[] asm("_binary_git_ota_pem_end");
extern esp_mqtt_client_handle_t client;  
extern uint8_t topic_sensor[100];
extern uint8_t topic_fota[100];
extern uint8_t topic_process[100];
extern uint8_t topic_number[100];

esp_err_t ota_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) 
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void fota_task(void *param)
{
    char ota_url[100] = {0};
    strcpy(ota_url, (char*)param);
    ESP_LOGI(TAG, "FOTA start, url: %s", ota_url);
    status = FOTA;
    esp_http_client_config_t ota_cfg = {
        .url = ota_url,
        .event_handler = ota_event_handler,
        .keep_alive_enable = true,
        .cert_pem = (char*)github_cert_pem_start
    };
    esp_err_t ret = esp_https_ota(&ota_cfg);
    if(ret == ESP_OK) 
    {
        ESP_LOGI(TAG, "OTA done, restarting...");
        esp_mqtt_client_publish(client, (char*)topic_process, "{\"process\":\"Fota done\"}", strlen("{\"process\":\"Fota done\"}"), 0, 0);
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
    } 
    else 
    {
        ESP_LOGE(TAG, "OTA failed...");
    }
}