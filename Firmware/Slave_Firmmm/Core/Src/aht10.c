#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "aht10.h"
#include <math.h>

extern I2C_HandleTypeDef hi2c2;

void reverse(char* str, int len)
{
    int i = 0, j = len - 1, temp;
    while (i < j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

int intToStr(int x, char str[], int d)
{
    int i = 0;
    if(x == 0)
    {
        str[i++] = '0';
    }
    while (x) {
        str[i++] = (x % 10) + '0';
        x = x / 10;
    }
    while (i < d)
        str[i++] = '0';
  
    reverse(str, i);
    str[i] = '\0';
    return i;
}

void ftoa(float n, char* res, int afterpoint)
{
    int ipart = (int)n;
    float fpart = n - (float)ipart;
    int i = intToStr(ipart, res, 0);
    if (afterpoint != 0) 
    {
        res[i] = '.';
        fpart = fpart * pow(10, afterpoint);
        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}

void write_trigger(void)
{
    char aht10_cmd[3] = {0xac, 0x33, 0x00};
    HAL_I2C_Master_Transmit(&hi2c2, AHT10_ADDRESS, (uint8_t*)aht10_cmd, 3, 100);
    HAL_Delay(100);
}

void read_data(char *data_rcv)
{
    HAL_I2C_Master_Receive(&hi2c2, AHT10_ADDRESS, (uint8_t*)data_rcv, 6, 100);
}

stm_err_t aht10_read_data(char *temp, char *hum)
{
    char data_rcv[10] = {0};
    float f_temp, f_hum;
    int i_temp, i_hum;
    write_trigger();
    read_data(data_rcv);
    if(~data_rcv[0] & 0x80)
    {
        i_temp = (int)(((uint32_t)data_rcv[3] & 15) << 16) | ((uint32_t)data_rcv[4] << 8) | ((uint32_t)data_rcv[5]);
        i_hum = (int)((uint32_t)data_rcv[1] << 12) | ((uint32_t)data_rcv[2] << 4) | ((uint32_t)data_rcv[3] >> 4);
        f_temp = ((float)i_temp * 200 / 1048576) - 50;
        f_hum = ((float)i_hum * 100 / 1048576);
        ftoa(f_temp, temp, 1);
        ftoa(f_hum, hum, 1);
        return STM_OK;
    }
    return STM_ERROR;
}