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

static const char *TAG = "LORA";
static spi_device_handle_t spi_handle;
static int implicit;
static long frequency;
esp_mqtt_client_handle_t client; 
extern RTC_NOINIT_ATTR int alarm_flag;
extern uint8_t topic_room_1_sensor[100];
extern uint8_t topic_room_2_sensor[100];
extern uint8_t topic_room_3_sensor[100];
extern uint8_t topic_room_4_sensor[100];
extern _status status;    

typedef enum 
{
    NODE_1 = 1,
    NODE_2,
    NODE_3, 
    NODE_4
} node_id_t;

typedef struct 
{
    char nodeID[10];
    char type[10];
    char alarm_status[5];
    char temp[10];
    char hum[10];
    char mq7_status[5];
    char co2[10];
    char tvoc[10];
} mess_t;

uint8_t lora_read_reg(uint8_t reg)
{
    uint8_t data_send[2] = {0x00 | reg, 0xFF}; 
    uint8_t data_recv[2] = {0};
    spi_transaction_t lora_recv = {
        .flags = 0,
        .length = 8 * sizeof(data_send),
        .rx_buffer = (void*)data_recv,
        .tx_buffer = (void*)data_send
    };
    gpio_set_level(LORA_NSS_PIN, 0);
    spi_device_transmit(spi_handle, &lora_recv);
    gpio_set_level(LORA_NSS_PIN, 1);
    return data_recv[1];
}

void lora_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data_send[2] = {0x80 | reg, val};
    uint8_t data_recv[2] = {0};
    spi_transaction_t lora_send = {
        .flags = 0,
        .length = 8 * sizeof(data_send),
        .rx_buffer = (void*)data_recv,
        .tx_buffer = (void*)data_send
    };
    gpio_set_level(LORA_NSS_PIN, 0);
    spi_device_transmit(spi_handle, &lora_send);
    gpio_set_level(LORA_NSS_PIN, 1); 
}

static void gpio_spi_init(void)
{
    gpio_config_t lora_rst_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0, 
        .pin_bit_mask = (1ULL << LORA_RST_PIN)
    };
    gpio_config(&lora_rst_cfg);
    gpio_set_level(LORA_RST_PIN, 1);
    ESP_LOGI(TAG, "LoRa rst init");

    gpio_config_t lora_nss_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0, 
        .pin_bit_mask = (1ULL << LORA_NSS_PIN)
    };
    gpio_config(&lora_nss_cfg);
    gpio_set_level(LORA_NSS_PIN, 1);
    ESP_LOGI(TAG, "LoRa nss init");
}

void lora_reset(void)
{
    gpio_set_level(LORA_RST_PIN, 0);
    vTaskDelay(10 / portTICK_RATE_MS);
    gpio_set_level(LORA_RST_PIN, 1);
    vTaskDelay(10 / portTICK_RATE_MS);
}

void spi_init(void)
{
    // Configuration for the SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LORA_MOSI_PIN,
        .miso_io_num = LORA_MISO_PIN,
        .sclk_io_num = LORA_SCK_PIN,
        .max_transfer_sz = 0,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    assert(spi_bus_initialize(VSPI_HOST, &bus_cfg, 0) == ESP_OK);
    // Configuration for the SPI device
    spi_device_interface_config_t device_cfg = {
        .clock_speed_hz = 4000000,
        .mode = 0,
        .spics_io_num = LORA_NSS_PIN,
        .queue_size = 1,
        .pre_cb = NULL,
        .flags = 0
    };
    assert(spi_bus_add_device(VSPI_HOST, &device_cfg, &spi_handle) == ESP_OK);
    ESP_LOGI(TAG, "SPI init");
}

void lora_sleep(void)
{
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void lora_idle(void)
{
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void lora_rx_mode(void)
{
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

void lora_tx_mode(void)
{
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
}

int lora_received(void)
{
    if(lora_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK)
        return 1;
    return 0;
}

void lora_receive(void)
{
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

int lora_get_irq(void)
{
    return (lora_read_reg(REG_IRQ_FLAGS));
}

int lora_packet_rssi(void)
{
    return (lora_read_reg(REG_PKT_RSSI_VALUE) - (frequency < 868E6 ? 164 : 157));
}

float lora_packet_snr(void)
{
    return ((int8_t)lora_read_reg(REG_PKT_SNR_VALUE)) * 0.25;
}

void lora_close(void)
{
    lora_sleep();
}

void lora_dump_registers(void)
{
    ESP_LOGI(TAG, "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
    for(int i = 0; i < 0x40; i++)
    {
        ESP_LOGI(TAG, "%02x", lora_read_reg(i));
        if((i & 0x0f) == 0x0f)
            printf("\n");
    }
    printf("\n");
}

void lora_set_tx_power(int8_t level)
{
    if(level < 0)
        level = 0;
    else if(level > 15)
        level = 15;
    lora_write_reg(REG_PA_CONFIG, PA_BOOST | level);
}

void lora_init(void)
{
    ESP_LOGI(TAG, "LoRa init");
    gpio_spi_init();
    spi_init();
    lora_reset();
    while(lora_read_reg(REG_VERSION) != 0x12);
    lora_sleep();
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);
    lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | 0x03);
    lora_write_reg(REG_MODEM_CONFIG_3, 0x04);
    lora_set_tx_power(15);
    lora_idle();
}

void lora_send_packet(uint8_t *buff, int size)
{
    lora_idle();
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    for(int i = 0; i < size; i++)
        lora_write_reg(REG_FIFO, *buff++);
    lora_write_reg(REG_PAYLOAD_LENGTH, size);
    // Start transmission and wait for conclusion
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
    while((lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0)
    {
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}

int lora_receive_packet(uint8_t *buff, int size)
{
    int len = 0;
    int irq = lora_read_reg(REG_IRQ_FLAGS);
    lora_write_reg(REG_IRQ_FLAGS, irq);
    if((irq & IRQ_RX_DONE_MASK) == 0)
        return 0;
    if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK)
        return 0;
    // Find packet size
    if(implicit)
        len = lora_read_reg(REG_PAYLOAD_LENGTH);
    else
        len = lora_read_reg(REG_RX_NB_BYTES);
    // Transfer data from radio
    lora_idle();
    lora_write_reg(REG_FIFO_ADDR_PTR, lora_read_reg(REG_FIFO_RX_CURRENT_ADDR));
    if(len > size)
        len = size;
    for(int i = 0; i < len; i++)
        *buff++ = lora_read_reg(REG_FIFO);
    return len;
}

void lora_explicit_header_mode(void)
{
    implicit = 0;
    lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) & 0xfe);
}

void lora_implicit_header_mode(int size)
{
    implicit = 1;
    lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) | 0x01);
    lora_write_reg(REG_PAYLOAD_LENGTH, size);
}

void lora_set_frequency(uint64_t freq)
{
    ESP_LOGI(TAG, "Set frequency");
    frequency = freq;
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void lora_set_spreading_factor(int sf)
{
    if(sf < 6)
        sf = 6;
    else if(sf > 12)
        sf = 12;
    if(sf == 6) 
    {
        lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc5);
        lora_write_reg(REG_DETECTION_THRESHOLD, 0x0c);
    } 
    else 
    {
        lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc3);
        lora_write_reg(REG_DETECTION_THRESHOLD, 0x0a);
    }
    lora_write_reg(REG_MODEM_CONFIG_2, (lora_read_reg(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

void lora_set_bandwidth(long sbw)
{
   int bw;

   if (sbw <= 7.8E3) bw = 0;
   else if (sbw <= 10.4E3) bw = 1;
   else if (sbw <= 15.6E3) bw = 2;
   else if (sbw <= 20.8E3) bw = 3;
   else if (sbw <= 31.25E3) bw = 4;
   else if (sbw <= 41.7E3) bw = 5;
   else if (sbw <= 62.5E3) bw = 6;
   else if (sbw <= 125E3) bw = 7;
   else if (sbw <= 250E3) bw = 8;
   else bw = 9;
   lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

void  lora_set_coding_rate(int denominator)
{
   if (denominator < 5) denominator = 5;
   else if (denominator > 8) denominator = 8;

   int cr = denominator - 4;
   lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

void lora_set_preamble_length(long length)
{
   lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
   lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

void lora_set_sync_word(int sw)
{
   lora_write_reg(REG_SYNC_WORD, sw);
}

void lora_enable_crc(void)
{
   lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) | 0x04);
}

void lora_disable_crc(void)
{
   lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) & 0xfb);
}

void lora_task(void *param)
{
    int len = 0;
    node_id_t node_id = NODE_1;
    char request_mess[100] = {0};
    char response_mess[100] = {0};
    char alarm_status[5] = {0};
    mess_t mess;
    TickType_t tick_3 = 0;
    TickType_t tick_9 = 0;
    char mqtt_mess[100] = {0};
    bool recv_flag = false;
    if(alarm_flag == ENABLE_ALARM)
        strcpy(alarm_status, "on");
    else
    {
        strcpy(alarm_status, "off");
        alarm_flag = DISABLE_ALARM;
    }
    lora_init();
    lora_set_frequency(433E6);
    lora_enable_crc();
    while(1)
    {
        switch(node_id)
        {
            case NODE_1:
                sprintf(request_mess, "$,node_1,request,%s,*\r\n", alarm_status);
                tick_9 = xTaskGetTickCount();
                while((xTaskGetTickCount() - tick_9 < 9000 / portTICK_RATE_MS) && (recv_flag == false))
                {
                    tick_3 = xTaskGetTickCount();
                    lora_send_packet((uint8_t*)request_mess, strlen(request_mess));
                    ESP_LOGI(TAG, "Send request %s", request_mess);
                    while((xTaskGetTickCount() - tick_3 < 3000 / portTICK_RATE_MS))
                    {
                        lora_receive();
                        if(lora_received())
                        {
                            len = lora_receive_packet((uint8_t*)response_mess, sizeof(response_mess));
                            response_mess[len] = '\0';  
                            if(response_mess[0] == '$')
                            {
                                sscanf(response_mess, "$,%[^,],%[^,],%[^,],%[^,],%[^,],*", mess.nodeID, mess.type, mess.temp, mess.hum, mess.mq7_status);
                                if(strstr(mess.nodeID, "node_1") != NULL && strstr(mess.type, "response") != NULL)
                                {
                                    ESP_LOGI(TAG, "Receive packet: %s", response_mess);   
                                    recv_flag = true;
                                    if(status == NORMAL_MODE)
                                    {
                                        // sprintf(mqtt_mes, "{\"co2\":%s,\"tvoc\":%s,\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s"},mess.co2, mess.tvoc, mess.alarm_status, mess.temp, mess.hum);
                                        sprintf(mqtt_mess, "{\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s}", mess.alarm_status, mess.temp, mess.hum);
                                        esp_mqtt_client_publish(client, (char*)topic_room_1_sensor, mqtt_mess, strlen(mqtt_mess), 0, 0);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Error packet");
                                vTaskDelay(10 / portTICK_RATE_MS);
                            }
                        }
                        else
                            vTaskDelay(10 / portTICK_RATE_MS);
                    }
                }
                if(recv_flag == false)
                    ESP_LOGE(TAG, "Skip node_1");
                else
                    recv_flag = false;
                node_id = NODE_2;  
                break;
            case NODE_2:
                sprintf(request_mess, "$,node_2,request,%s,*\r\n", alarm_status);
                tick_9 = xTaskGetTickCount();
                while((xTaskGetTickCount() - tick_9 < 9000 / portTICK_RATE_MS) && (recv_flag == false))
                {
                    tick_3 = xTaskGetTickCount();
                    lora_send_packet((uint8_t*)request_mess, strlen(request_mess));
                    ESP_LOGI(TAG, "Send request %s", request_mess);
                    while((xTaskGetTickCount() - tick_3 < 3000 / portTICK_RATE_MS))
                    {
                        lora_receive();
                        if(lora_received())
                        {
                            len = lora_receive_packet((uint8_t*)response_mess, sizeof(response_mess));
                            response_mess[len] = '\0';
                            if(response_mess[0] == '$')
                            {
                                sscanf(response_mess, "$,%[^,],%[^,],%[^,],%[^,],%[^,],*", mess.nodeID, mess.type, mess.temp, mess.hum, mess.mq7_status);
                                if(strstr(mess.nodeID, "node_2") != NULL && strstr(mess.type, "response") != NULL)
                                {
                                    ESP_LOGI(TAG, "Receive packet: %s", response_mess);   
                                    recv_flag = true;
                                    if(status == NORMAL_MODE)
                                    {
                                        // sprintf(mqtt_mes, "{\"co2\":%s,\"tvoc\":%s,\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s"},mess.co2, mess.tvoc, mess.alarm_status, mess.temp, mess.hum);
                                        sprintf(mqtt_mess, "{\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s}", mess.alarm_status, mess.temp, mess.hum);
                                        esp_mqtt_client_publish(client, (char*)topic_room_2_sensor, mqtt_mess, strlen(mqtt_mess), 0, 0);
                                    }                                    
                                    break;
                                }
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Error packet");
                                vTaskDelay(10 / portTICK_RATE_MS);
                            }
                        }
                        else
                            vTaskDelay(10 / portTICK_RATE_MS);
                    }
                }
                if(recv_flag == false)
                    ESP_LOGE(TAG, "Skip node_2");
                else
                    recv_flag = false;
                node_id = NODE_3;  
                break;
            case NODE_3:
                sprintf(request_mess, "$,node_3,request,%s,*\r\n", alarm_status);
                tick_9 = xTaskGetTickCount();
                while((xTaskGetTickCount() - tick_9 < 9000 / portTICK_RATE_MS) && (recv_flag == false))
                {
                    tick_3 = xTaskGetTickCount();
                    lora_send_packet((uint8_t*)request_mess, strlen(request_mess));
                    ESP_LOGI(TAG, "Send request %s", request_mess);
                    while((xTaskGetTickCount() - tick_3 < 3000 / portTICK_RATE_MS))
                    {
                        lora_receive();
                        if(lora_received())
                        {
                            len = lora_receive_packet((uint8_t*)response_mess, sizeof(response_mess));
                            response_mess[len] = '\0';
                            if(response_mess[0] == '$')
                            {
                                sscanf(response_mess, "$,%[^,],%[^,],%[^,],%[^,],%[^,],*", mess.nodeID, mess.type, mess.temp, mess.hum, mess.mq7_status);
                                if(strstr(mess.nodeID, "node_3") != NULL && strstr(mess.type, "response") != NULL)
                                {
                                    ESP_LOGI(TAG, "Receive packet: %s", response_mess);   
                                    recv_flag = true;
                                    if(status == NORMAL_MODE)
                                    {
                                        // sprintf(mqtt_mes, "{\"co2\":%s,\"tvoc\":%s,\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s"},mess.co2, mess.tvoc, mess.alarm_status, mess.temp, mess.hum);
                                        sprintf(mqtt_mess, "{\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s}", mess.alarm_status, mess.temp, mess.hum);
                                        esp_mqtt_client_publish(client, (char*)topic_room_3_sensor, mqtt_mess, strlen(mqtt_mess), 0, 0);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Error packet");
                                vTaskDelay(10 / portTICK_RATE_MS);
                            }
                        }
                        else
                            vTaskDelay(10 / portTICK_RATE_MS);
                    }
                }
                if(recv_flag == false)
                    ESP_LOGE(TAG, "Skip node_3");
                else
                    recv_flag = false;
                node_id = NODE_4;  
                break;
            case NODE_4:
                sprintf(request_mess, "$,node_4,request,%s,*\r\n", alarm_status);
                tick_9 = xTaskGetTickCount();
                while((xTaskGetTickCount() - tick_9 < 9000 / portTICK_RATE_MS) && (recv_flag == false))
                {
                    tick_3 = xTaskGetTickCount();
                    lora_send_packet((uint8_t*)request_mess, strlen(request_mess));
                    ESP_LOGI(TAG, "Send request %s", request_mess);
                    while((xTaskGetTickCount() - tick_3 < 3000 / portTICK_RATE_MS))
                    {
                        lora_receive();
                        if(lora_received())
                        {
                            len = lora_receive_packet((uint8_t*)response_mess, sizeof(response_mess));
                            response_mess[len] = '\0';
                            if(response_mess[0] == '$')
                            {
                                sscanf(response_mess, "$,%[^,],%[^,],%[^,],%[^,],%[^,],*", mess.nodeID, mess.type, mess.temp, mess.hum, mess.mq7_status);
                                if(strstr(mess.nodeID, "node_4") != NULL && strstr(mess.type, "response") != NULL)
                                {
                                    ESP_LOGI(TAG, "Receive packet: %s", response_mess);   
                                    recv_flag = true;
                                    if(status == NORMAL_MODE)
                                    {
                                        // sprintf(mqtt_mes, "{\"co2\":%s,\"tvoc\":%s,\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s"},mess.co2, mess.tvoc, mess.alarm_status, mess.temp, mess.hum);
                                        sprintf(mqtt_mess, "{\"alarm\":\"%s\",\"temp\":%s,\"hum\":%s}", mess.alarm_status, mess.temp, mess.hum);
                                        esp_mqtt_client_publish(client, (char*)topic_room_4_sensor, mqtt_mess, strlen(mqtt_mess), 0, 0);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Error packet");
                                vTaskDelay(10 / portTICK_RATE_MS);
                            }
                        }
                        else
                            vTaskDelay(10 / portTICK_RATE_MS);
                    }
                }
                if(recv_flag == false)
                    ESP_LOGE(TAG, "Skip node_4");
                else
                    recv_flag = false;
                node_id = NODE_1;  
                break;
            default:
                break;
        }
    }
}