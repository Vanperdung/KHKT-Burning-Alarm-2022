/*
 * aht10.h
 *
 *  Created on: Oct 22, 2022
 *      Author: dung
 */

#ifndef _AHT10_H_
#define _AHT10_H_

#define AHT10_ADDRESS         (0x38 << 1)
#define AHT10_CMD_MODE        (0)
#define AHT10_MEASURE_MODE    (1)

stm_err_t aht10_read_data(char *temp, char *hum);

#endif
