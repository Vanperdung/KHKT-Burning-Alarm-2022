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

uint8_t lora_read_reg(uint8_t reg)
{
    uint8_t data_send[2] = {0x00 | reg, 0xFF}; 
    uint8_t data_recv[2] = {0};
    spi_transaction_t lora_recv = {
        .length = 8 * sizeof(data_send),
        .rx_buffer = (void*)data_recv
    };
    gpio_set_level(LORA_NSS_PIN, 0);
    spi_device_transmit(spi_handle, &lora_recv);
    gpio_set_level(LORA_NSS_PIN, 1);
    return data_recv[1];
}

void lora_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data_send[2] = {0x80 | reg, val};
    spi_transaction_t lora_send = {
        .length = 8 * sizeof(data_send),
        .tx_buffer = (void*)data_send
    };
    gpio_set_level(LORA_NSS_PIN, 0);
    spi_device_transmit(spi_handle, &lora_send);
    gpio_set_level(LORA_NSS_PIN, 1); 
}

static void gpio_spi_init(void)
{
    gpio_pad_select_gpio(LORA_RST_PIN);
    gpio_set_direction(LORA_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(LORA_NSS_PIN);
    gpio_set_direction(LORA_NSS_PIN, GPIO_MODE_OUTPUT);
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
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    spi_bus_initialize(VSPI_HOST, &bus_cfg, 0);
    // Configuration for the SPI device
    spi_device_interface_config_t device_cfg = {
        .clock_speed_hz = 9000000,
        .mode = 0,
        .spics_io_num = LORA_NSS_PIN,
        .queue_size = 4,
    };
    spi_bus_add_device(VSPI_HOST, &device_cfg, &spi_handle);
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

void lora_set_tx_power(uint8_t level)
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

}

