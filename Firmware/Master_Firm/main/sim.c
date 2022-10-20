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

#include "driver/gpio.h"
#include "driver/uart.h"

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

static const char *TAG = "SIM";

static void uart_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(UART_NUM_2, 1024, 2048, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_cfg);
    uart_set_pin(UART_NUM_2, SIM_TXD_PIN, SIM_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void gpio_sim_init(void)
{
    gpio_config_t pwrkey_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0, 
        .pin_bit_mask = (1ULL << SIM_PWRKEY_PIN)
    };
    gpio_config(&pwrkey_cfg);
    gpio_set_level(SIM_PWRKEY_PIN, 1);

    gpio_config_t status_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = 0,
        .pull_up_en = 0, 
        .pin_bit_mask = (1ULL << SIM_STATUS_PIN)
    };
    gpio_config(&status_cfg);
}

static bool read_sim_response(char *sim_mess_rx, char *check_response, int time_delay)
{
    TickType_t pre_tick = xTaskGetTickCount();
    int cur_len, pre_len = 0;
    uart_flush(UART_NUM_2);
    while(!(xTaskGetTickCount() - pre_tick > (time_delay / portTICK_RATE_MS)))
    {
        uart_get_buffered_data_len(UART_NUM_2, (size_t*)&cur_len);
        if(cur_len > pre_len)
        {
            pre_tick = xTaskGetTickCount();
            pre_len = cur_len;
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    uart_read_bytes(UART_NUM_2, sim_mess_rx, cur_len, 20 / portTICK_RATE_MS);
    // ESP_LOGI(TAG, "Sim response: %s", sim_mess_rx);
    if(check_response)
    {
        if(strstr(sim_mess_rx, check_response) != NULL)
            return true;
        else 
            return false;
    }
    else
        return true;
}

esp_err_t sim_init(void)
{
    // char sim_mess_tx[100] = {0};
    char sim_mess_rx[100] = {0};
    ESP_LOGI(TAG, "Sim init");
    uart_init();
    gpio_sim_init();
    while(!gpio_get_level(SIM_STATUS_PIN))
    {
        gpio_set_level(SIM_PWRKEY_PIN, 1);
        vTaskDelay(50 / portTICK_RATE_MS);
    }
    gpio_set_level(SIM_PWRKEY_PIN, 0);

    uart_write_bytes(UART_NUM_2, "ATE0\r\n", strlen("ATE0\r\n"));
    if(!read_sim_response(sim_mess_rx, "OK", 100))
    {
        ESP_LOGI(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT\r\n", strlen("AT\r\n"));
    if(!read_sim_response(sim_mess_rx, "OK", 100))
    {
        ESP_LOGI(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT+CPIN?\r\n", strlen("AT+CPIN?\r\n"));
    if(!read_sim_response(sim_mess_rx, "OK", 1000))
    {
        ESP_LOGI(TAG, "Sim got error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void send_message(void)
{
    
}

void sim_task(void)
{
    esp_err_t ret;
    do
    {
        ret = sim_init();
    } while(ret != ESP_OK);
    ESP_LOGI(TAG, "Sim init done");
    while(1)
    {

    }
}