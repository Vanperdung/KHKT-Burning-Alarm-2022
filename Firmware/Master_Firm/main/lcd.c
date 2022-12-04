#include <string.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/i2c.h"

#include "lcd.h"

i2c_port_t i2c_num = 0;
static const char *TAG = "LCD";

void i2c_init(void)
{
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_I2C_SDA,
        .scl_io_num = GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_CLOCK_FREQ};
    i2c_param_config(&i2c_config);
    i2c_driver_install(i2c_config.mode, 0, 0, 0);
}

esp_err_t i2c_write_lcd(uint8_t *data, int len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDRESS << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write(cmd, data, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void lcd_send_cmd(char cmd)
{
    char data_u, data_l;
    char data_t[5] = {0};
    data_u = (cmd & 0xF0);
    data_l = ((cmd << 4) & 0xF0);
    data_t[0] = data_u | 0x0C;
    data_t[1] = data_u | 0x08;
    data_t[2] = data_l | 0x0C;
    data_t[3] = data_l | 0x08;
    i2c_write_lcd((uint8_t *)data_t, strlen(data_t));
}

void lcd_send_data(char data)
{
    char data_u, data_l;
    char data_t[5] = {0};
    data_u = (data & 0xF0);
    data_l = ((data << 4) & 0xF0);
    data_t[0] = data_u | 0x0D;
    data_t[1] = data_u | 0x09;
    data_t[2] = data_l | 0x0D;
    data_t[3] = data_l | 0x09;
    i2c_write_lcd((uint8_t *)data_t, strlen(data_t));
}

void lcd_clear(i2c_port_t i2c_num)
{
    lcd_send_cmd(0x00);
    for (int i = 0; i < 100; i++)
    {
        lcd_send_data(' ');
    }
}

void lcd_init(i2c_port_t i2c_num)
{
    vTaskDelay(50 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x30);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x30);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x30);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x20);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x28);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x08);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x01);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x06);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x0C);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void lcd_send_string(char *str)
{
    while (*str)
    {
        lcd_send_data(*str++);
    }
}

void lcd_task(void *param)
{
    i2c_init();
    vTaskDelay(500 / portTICK_RATE_MS);
    lcd_init();
    lcd_clear();
    while (1)
    {
        lcd_clear();
        // Line 1
        lcd_send_cmd(0x80 | 0x00);
        lcd_send_string()
        // Line 2
        lcd_send_cmd(0x80 | 0x40);
        lcd_send_string()
        // Line 3
        lcd_send_cmd(0x80 | 0x14);
        lcd_send_string()
        // Line 4
        lcd_send_cmd(0x80 | 0x54);
        lcd_send_string()

        vTaskDelay(200 / portTICK_RATE_MS);
    }
}
