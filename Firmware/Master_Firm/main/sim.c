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
#include "spiffs_user.h"

static const char *TAG = "SIM";
bool send_sms_alarm_flag = false;
extern RTC_NOINIT_ATTR int alarm_flag;
extern char room_alarm[50];
extern bool node_1_alarm_flag;
extern bool node_2_alarm_flag;
extern bool node_3_alarm_flag;
extern bool node_4_alarm_flag;
extern bool node_1_sms_done;
extern bool node_2_sms_done;
extern bool node_3_sms_done;
extern bool node_4_sms_done;
bool send_sms = false;

static void uart_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
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
        .pin_bit_mask = (1ULL << SIM_PWRKEY_PIN)};
    gpio_config(&pwrkey_cfg);
    gpio_set_level(SIM_PWRKEY_PIN, 1);

    gpio_config_t status_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .pin_bit_mask = (1ULL << SIM_STATUS_PIN)};
    gpio_config(&status_cfg);
}

static bool read_sim_response(char *sim_mess_rx, char *check_response, int time_delay)
{
    TickType_t pre_tick = xTaskGetTickCount();
    int cur_len, pre_len = 0;
    uart_flush(UART_NUM_2);
    while (!(xTaskGetTickCount() - pre_tick > (time_delay / portTICK_RATE_MS)))
    {
        uart_get_buffered_data_len(UART_NUM_2, (size_t *)&cur_len);
        if (cur_len > pre_len)
        {
            pre_tick = xTaskGetTickCount();
            pre_len = cur_len;
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    uart_read_bytes(UART_NUM_2, sim_mess_rx, cur_len, 20 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Sim response: %s", sim_mess_rx);
    if (check_response)
    {
        if (strstr(sim_mess_rx, check_response) != NULL)
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
    // ESP_LOGI(TAG, "Sim init");
    while (!gpio_get_level(SIM_STATUS_PIN))
    {
        gpio_set_level(SIM_PWRKEY_PIN, 1);
        vTaskDelay(50 / portTICK_RATE_MS);
    }
    gpio_set_level(SIM_PWRKEY_PIN, 0);

    uart_write_bytes(UART_NUM_2, "ATE0\r\n", strlen("ATE0\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 100))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT\r\n", strlen("AT\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 100))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT+CPIN?\r\n", strlen("AT+CPIN?\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 100))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT+COPS?\r\n", strlen("AT+COPS?\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 100))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT+CMGF=1\r\n", strlen("AT+CMGF=1\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 2000))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, "AT&W\r\n", strlen("AT&W\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 2000))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t send_message(char *sim_mess, char *number)
{
    char end_mess[2] = {0x1a, 0x00};
    char at_send_mess[100] = {0};
    char sim_mess_rx[100] = {0};
    sprintf(at_send_mess, "AT+CMGS=\"%s\"\r\n", number);
    uart_write_bytes(UART_NUM_2, at_send_mess, strlen(at_send_mess));
    if (!read_sim_response(sim_mess_rx, ">", 200))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, sim_mess, strlen(sim_mess));
    if (!read_sim_response(sim_mess_rx, NULL, 200))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }

    uart_write_bytes(UART_NUM_2, end_mess, strlen(end_mess));
    if (!read_sim_response(sim_mess_rx, NULL, 200))
    {
        ESP_LOGE(TAG, "Sim got error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool call_phone(char *number)
{
    char at_send_mess[100] = {0};
    char sim_mess_rx[100] = {0};
    char end_mess[2] = {0x1a, 0x00};
    sprintf(at_send_mess, "ATD%s;\r\n", number);
    uart_write_bytes(UART_NUM_2, "AT+CMGDA=\"DEL ALL\"\r\n", strlen("AT+CMGDA=\"DEL ALL\"\r\n"));
    if (!read_sim_response(sim_mess_rx, "OK", 10000))
    {
        ESP_LOGE(TAG, "Sim got error");
    }
    vTaskDelay(2000 / portTICK_RATE_MS);
    uart_write_bytes(UART_NUM_2, at_send_mess, strlen(at_send_mess));
    if (!read_sim_response(sim_mess_rx, "OK", 2000))
    {
        ESP_LOGE(TAG, "Sim got error");
    }
    vTaskDelay(20000 / portTICK_RATE_MS);
    uart_write_bytes(UART_NUM_2, "ATH\r\n", strlen("ATH\r\n"));
    // uart_write_bytes(UART_NUM_2, end_mess, strlen(end_mess));
    if (!read_sim_response(sim_mess_rx, "OK", 1000))
    {
        ESP_LOGE(TAG, "Sim got error");
    }
    vTaskDelay(20000 / portTICK_RATE_MS);
    uart_write_bytes(UART_NUM_2, "AT+CMGR=1\r\n", strlen("AT+CMGR=1\r\n"));
    if (!read_sim_response(sim_mess_rx, "Xac nhan", 10000))
    {
        ESP_LOGE(TAG, "Khong nhan duoc tin nhan xac nhan");
        return false;
    }
    else
    {
        ESP_LOGI(TAG, "Nhan duoc tin nhan xac nhan");
        send_sms = true;
        return true;
    }
}

void sim_task(void *param)
{
    esp_err_t ret;
    char number[20] = "0373529544";
    char *num_list[20] = {"0373529544", "0327116935", "0869446596"};
    char mess[50] = "Nha ban co chay";
    char mess_send[100] = {0};
    bool call_state = false;
    int index = 0;
    uart_init();
    gpio_sim_init();
    do
    {
        ret = sim_init();
    } while (ret != ESP_OK);
    ESP_LOGI(TAG, "Sim init done");
    vTaskDelay(2000 / portTICK_RATE_MS);
    while (1)
    {
        // if(node_1_alarm_flag == true && node_2_alarm_flag == true && node_3_alarm_flag == true && node_4_alarm_flag == true)
        if (send_sms_alarm_flag == false && alarm_flag == ENABLE_ALARM)
        {
            // read_from_file("message.txt", message);
            if (strlen(number) > 0)
            {   
                ESP_LOGI(TAG, "Call phone");
                do
                {
                    call_state = call_phone(num_list[index]);
                    index++;
                    if(index == 3)
                        index = 0;
                } while (call_state == false);
                vTaskDelay(5000 / portTICK_RATE_MS);
                // send_message(message, number);
            }
            else
                ESP_LOGE(TAG, "Read number or message fail");
            send_sms_alarm_flag = true;
        }
        else
            vTaskDelay(100 / portTICK_RATE_MS);
        if(alarm_flag == ENABLE_ALARM && send_sms == true)
        {
            if(node_1_alarm_flag == true && node_1_sms_done == false)
            {
                ESP_LOGI(TAG, "Send sms node 1");
                // read_from_file("message.txt", message);
                strcpy(mess_send, mess);
                strcat(mess_send, room_alarm);
                send_message(mess_send, number);
                node_1_sms_done = true;
                vTaskDelay(5000 / portTICK_RATE_MS);
            }
            else if(node_2_alarm_flag == true && node_2_sms_done == false)
            {
                ESP_LOGI(TAG, "Send sms node 2");
                // read_from_file("message.txt", message);
                strcpy(mess_send, mess);
                strcat(mess_send, room_alarm);
                send_message(mess_send, number);
                node_2_sms_done = true;
                vTaskDelay(5000 / portTICK_RATE_MS);
            }
            else if(node_3_alarm_flag == true && node_3_sms_done == false)
            {
                ESP_LOGI(TAG, "Send sms node 3");
                // read_from_file("message.txt", message);
                strcpy(mess_send, mess);
                strcat(mess_send, room_alarm);
                send_message(mess_send, number);
                node_3_sms_done = true;    
                vTaskDelay(5000 / portTICK_RATE_MS);            
            }
            else if(node_4_alarm_flag == true && node_4_sms_done == false)
            {
                ESP_LOGI(TAG, "Send sms node 4");
                // read_from_file("message.txt", message);
                strcpy(mess_send, mess);
                strcat(mess_send, room_alarm);
                send_message(mess_send, number);
                node_4_sms_done = true;
                vTaskDelay(5000 / portTICK_RATE_MS);                
            }
            else 
                vTaskDelay(100 / portTICK_RATE_MS);
        }
        else 
            vTaskDelay(100 / portTICK_RATE_MS);
    }
}