#include "ccs811.h"
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c2;

static char msg[100] = {0};

static void debug(char *data)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)data, (uint16_t)strlen(data), 1000);
}

void ccs811_write_reg(char reg)
{
    HAL_I2C_Master_Transmit(&hi2c2, CCS811_ADDRESS, (uint8_t *)&reg, 1, 500);
}

void ccs811_write_data_byte(char reg, char *data_send)
{
    char val[3] = {0};
    val[0] = reg;
    val[1] = *data_send;
    HAL_I2C_Master_Transmit(&hi2c2, CCS811_ADDRESS, (uint8_t *)val, 2, 500);
}

void ccs811_write_data(char reg, char *data_send, size_t len)
{
    char *val = NULL;
    val = (char *)malloc(len + 2);
    memset(val, '\0', len + 2);
    val[0] = reg;
    strcpy(&val[1], data_send);
    HAL_I2C_Master_Transmit(&hi2c2, CCS811_ADDRESS, (uint8_t *)val, strlen(val), 500);
    free(val);
}

void ccs811_read_reg(char reg, char *data_recv)
{
    ccs811_write_reg(reg);
    HAL_I2C_Master_Receive(&hi2c2, CCS811_ADDRESS, (uint8_t *)data_recv, 1, 500);
}

void ccs811_read_data(char reg, char *data_recv, size_t len)
{
    // char reg_addr = reg;
    // for (int index = 0; index < len; index++)
    // {
    //     ccs811_write_reg(reg_addr);
    //     HAL_I2C_Master_Receive(&hi2c2, CCS811_ADDRESS, (uint8_t *)&data_recv[index], 1, 500);
    //     reg_addr++;
    // }

    ccs811_write_reg(reg);
    HAL_I2C_Master_Receive(&hi2c2, CCS811_ADDRESS, (uint8_t *)data_recv, len, 500);
}

void ccs811_get_reg(char reg)
{
    char status;
    ccs811_read_reg(reg, &status);
    sprintf(msg, "status: %02x\r\n", status);
    debug(msg);
}

char ccs811_get_status(void)
{
    char status;
    ccs811_read_reg(STATUS_REG, &status);
    return status;
}

void ccs811_start_app(void)
{
    ccs811_write_reg(APP_START_REG);
    HAL_Delay(100);
}

void ccs811_write_measure_mode(uint8_t mode, uint8_t intr_state, uint8_t intr_threshold)
{
    ccs811_status_t *ccs811_status;
    uint8_t meas_mode = (mode << 4) | (intr_state << 3) | (intr_threshold << 2);
    ccs811_write_data_byte(MEAS_MODE_REG, (char *)&meas_mode);
}

void ccs811_init(void)
{
    uint8_t hw_id;
    char status;
    ccs811_start_app();
    ccs811_read_reg(STATUS_REG, &status);
    sprintf(msg, "status: %02x\r\n", status);
    debug(msg);
    if (status && 0x01)
    {
        ccs811_read_reg(ERROR_ID_REG, &status);
        sprintf(msg, "error: %02x\r\n", status);
        debug(msg);
    }
    ccs811_read_reg(HW_ID_REG, (char *)&hw_id);
    sprintf(msg, "hw_id: %02x\r\n", hw_id);
    debug(msg);
    if (hw_id != 0x81)
        return;
    else
        ccs811_write_measure_mode(1, 0, 0);
}

ccs811_raw_data_t ccs811_read_raw(void)
{
    ccs811_raw_data_t ret;
    uint8_t ret_data[2];
    ccs811_read_data(RAW_DATA_REG, (char *)ret_data, 2);
    sprintf(msg, "ret0: %d, ret1: %d\r\n", (uint32_t)ret_data[0], (uint32_t)ret_data[1]);
    debug(msg);
    ret.current = ret_data[0] >> 2;
    ret.voltage = ((uint16_t)(ret_data[0]) << 8) & 0x0300 | (uint16_t)(ret_data[1]) & 0x00ff;
    return ret;
}

ccs811_alg_result_data_t ccs811_get_data(void)
{
    ccs811_alg_result_data_t result_data;
    uint8_t data[8] = {0};
    ccs811_read_data(ALG_RESULT_DATA_REG, (char *)data, 4);
    result_data.eCO2 = (((uint16_t)data[0] << 8) & 0xff00) | ((uint16_t)data[1] & 0x00ff);
    result_data.tvoc = (((uint16_t)data[2] << 8) & 0xff00) | ((uint16_t)data[3] & 0x00ff);
    return result_data;
}

void soft_reset(void)
{
    char sw_reset[4] = {0x11, 0xe5, 0x72, 0x8a};
    ccs811_write_data(SW_RESET_REG, sw_reset, 4);
    HAL_Delay(100);
}

// void ccs811_init(void)
// {
// 	char status;
// ccs811_read_reg(STATUS_REG, &status);
// sprintf(msg, "status: %02x\r\n", status);
// debug(msg);
// 	if(status & 0x80)
// 	{

// 	}
// }
