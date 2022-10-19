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

static const char *TAG = "MAIN";
RTC_NOINIT_ATTR bool smartconfig_flag;
_status status;    

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());  
    status = LOCAL_MODE;
    led_status_init();
    led_onoff_init();
    xTaskCreate(&led_status_task, "led_status_task", 2048, NULL, 5, NULL);
    xTaskCreate(&led_onoff_task, "led_onoff_task", 2048, NULL, 5, NULL);
    wifi_init();
    if(smartconfig_flag == true)
    {
        smartconfig_flag = false;
        status = SMARTCONFIG;
        smartconfig_init();
    }
    else
    {
        wifi_config_t wifi_cfg = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK
            }
        };
        if(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) == ESP_OK)
        {
            ESP_LOGI(TAG, "Wifi configuration already stored in flash partition called NVS");
            ESP_LOGI(TAG, "%s" ,wifi_cfg.sta.ssid);
            ESP_LOGI(TAG, "%s" ,wifi_cfg.sta.password);
            wifi_sta(wifi_cfg, WIFI_MODE_STA);
            while(1)
            {
                vTaskDelay(1000 / portTICK_RATE_MS);
            }
            while(!(status == NORMAL_MODE));
            // mqtt_client_sta();
        }
    }
    vTaskSuspend(NULL);
}