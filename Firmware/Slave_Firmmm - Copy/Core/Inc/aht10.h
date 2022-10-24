#ifndef _AHT10_H_
#define _AHT10_H_

#define AHT10_ADDRESS         (0x38 << 1)
#define AHT10_CMD_MODE        (0)
#define AHT10_MEASURE_MODE    (1)

typedef enum {
	STM_ERROR,
	STM_OK
} stm_err_t;

stm_err_t aht10_read_data(char *temp, char *hum);

#endif