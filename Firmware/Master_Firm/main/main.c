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
#include "driver/i2c.h"
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
#include "spiffs_user.h"
#include "lcd.h"

static const char *TAG = "MAIN";
RTC_NOINIT_ATTR int smartconfig_flag;
RTC_NOINIT_ATTR int alarm_flag;
_status status;
uint8_t topic_room_1_sensor[100] = {0};
uint8_t topic_room_2_sensor[100] = {0};
uint8_t topic_room_3_sensor[100] = {0};
uint8_t topic_room_4_sensor[100] = {0};
uint8_t topic_fota[100] = {0};
uint8_t topic_process[100] = {0};
uint8_t topic_number[100] = {0};
uint8_t topic_message[100] = {0};



void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    status = LOCAL_MODE;
    xTaskCreate(&led_task, "led_status_task", 2048, NULL, 5, NULL);
    xTaskCreate(&button_task, "button_task", 2048, NULL, 10, NULL);
    xTaskCreate(&lcd_task, "lcd_task", 2048, NULL, 11, NULL);
    wifi_init();
    mount_SPIFFS();
    if (smartconfig_flag == ENABLE_SC)
    {
        smartconfig_flag = DISABLE_SC;
        status = SMARTCONFIG;
        smartconfig_init();
    }
    else
    {
        xTaskCreate(&sim_task, "sim_task", 4096, NULL, 10, NULL);
        xTaskCreate(&lora_task, "lora_task", 8192, NULL, 11, NULL);
        wifi_config_t wifi_cfg = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {
                    .capable = true,
                    .required = false}}};
        if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) == ESP_OK)
        {
            ESP_LOGI(TAG, "Wifi configuration already stored in flash partition called NVS");
            ESP_LOGI(TAG, "%s", wifi_cfg.sta.ssid);
            ESP_LOGI(TAG, "%s", wifi_cfg.sta.password);
            wifi_sta(wifi_cfg, WIFI_MODE_STA);
            mqtt_client_sta();
        }
    }
    vTaskSuspend(NULL);
}