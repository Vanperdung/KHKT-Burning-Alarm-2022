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
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_attr.h"

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
#include "spiffs_user.h"

static const char *TAG = "BUTTON";
extern RTC_NOINIT_ATTR int smartconfig_flag;

typedef struct 
{
    uint64_t time_down;
    uint8_t pin;
    uint64_t time_set;
} _button;

void button_task(void *param)
{
    TickType_t pre_tick = 0;
    _button button_boot = {
        .pin = BUTTON_BOOT_PIN,
        .time_down = 0,
        .time_set = TIME_DOWN_SET
    };
    _button button_onoff = {
        .pin = BUTTON_ONOFF_PIN,
        .time_down = 0,
        .time_set = TIME_DOWN_SET
    };

    gpio_config_t boot_conf;
	boot_conf.intr_type = GPIO_INTR_DISABLE;
	boot_conf.mode = GPIO_MODE_INPUT;
	boot_conf.pull_up_en = GPIO_PULLUP_ONLY;
	boot_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	boot_conf.pin_bit_mask = (1ULL << button_boot.pin);
	gpio_config(&boot_conf);

    gpio_config_t onoff_conf;
	onoff_conf.intr_type = GPIO_INTR_DISABLE;
	onoff_conf.mode = GPIO_MODE_INPUT;
	onoff_conf.pull_up_en = GPIO_PULLUP_ONLY;
	onoff_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	onoff_conf.pin_bit_mask = (1ULL << button_onoff.pin);
	gpio_config(&onoff_conf);
    ESP_LOGI(TAG, "Button init");

    while(1)
    {
        if(!gpio_get_level(button_boot.pin))
        {
            if(pre_tick == 0)
                pre_tick = xTaskGetTickCount();
            button_boot.time_down += xTaskGetTickCount() - pre_tick;
            pre_tick = xTaskGetTickCount();
            if(button_boot.time_down >= (button_boot.time_set / portTICK_RATE_MS))
            {
                ESP_LOGI(TAG, "Trigger smartconfig");
                smartconfig_flag = ENABLE_SC;
                esp_restart();
            }
        }
        else
        {
            button_boot.time_down = 0;
            pre_tick = 0;
        }

        if(!gpio_get_level(button_onoff.pin))
        {
            if(pre_tick == 0)
                pre_tick = xTaskGetTickCount();
            button_onoff.time_down += xTaskGetTickCount() - pre_tick;
            pre_tick = xTaskGetTickCount();
            if(button_onoff.time_down >= (button_onoff.time_set / portTICK_RATE_MS))
            {
                ESP_LOGI(TAG, "Switch alarm mode");
            }
        }
        else
        {
            button_onoff.time_down = 0;
        }
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}

