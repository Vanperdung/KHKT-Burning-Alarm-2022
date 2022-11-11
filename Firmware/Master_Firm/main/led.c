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

#include "driver/gpio.h"

#include "button.h"
#include "fota.h"
#include "led.h"
#include "lora.h"
#include "mqtt.h"
#include "sim.h"
#include "smartconfig.h"
#include "wifi.h"
#include "main.h"

static const char *TAG = "LED";
extern _status status;    

void led_status_init(void)
{
    gpio_config_t led_status_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0, 
        .pin_bit_mask = (1ULL << LED_STATUS_PIN)
    };
    gpio_config(&led_status_cfg);
    gpio_set_level(LED_STATUS_PIN, LED_OFF);
    ESP_LOGI(TAG, "Led status init");
}

void led_onoff_init(void)
{
    gpio_config_t led_onoff_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .pin_bit_mask = (1ULL << LED_ONOFF_PIN)
    };
    gpio_config(&led_onoff_cfg);
    gpio_set_level(LED_ONOFF_PIN, LED_OFF);
    ESP_LOGI(TAG, "Led on-off init");
}

void led_task(void *param)
{
    led_status_init();
    led_onoff_init();
    while(1)
    {
        switch(status)
        {
            case LOCAL_MODE:
                gpio_set_level(LED_STATUS_PIN, LED_ON);
                vTaskDelay(100 / portTICK_RATE_MS);
                gpio_set_level(LED_STATUS_PIN, LED_OFF);
                vTaskDelay(100 / portTICK_RATE_MS);
                break;
            case NORMAL_MODE:
                gpio_set_level(LED_STATUS_PIN, LED_ON);
                vTaskDelay(100 / portTICK_RATE_MS);
                break;
            case SMARTCONFIG:   
                gpio_set_level(LED_ONOFF_PIN, LED_ON);
                vTaskDelay(100 / portTICK_RATE_MS);
                gpio_set_level(LED_ONOFF_PIN, LED_OFF);
                vTaskDelay(100 / portTICK_RATE_MS);
                break;
            case FOTA:
                gpio_set_level(LED_ONOFF_PIN, LED_ON);
                vTaskDelay(1000 / portTICK_RATE_MS);
                gpio_set_level(LED_ONOFF_PIN, LED_OFF);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            default:
                gpio_set_level(LED_STATUS_PIN, LED_OFF);
                gpio_set_level(LED_ONOFF_PIN, LED_OFF);
                vTaskDelay(100 / portTICK_RATE_MS);
                break;  
        }
    }
}