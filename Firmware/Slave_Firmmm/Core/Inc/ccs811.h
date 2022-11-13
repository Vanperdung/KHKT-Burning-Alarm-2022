#ifndef _CCS811_H_
#define _CCS811_H_

#include <stdint.h>

#define CCS811_ADDRESS              (0x5A << 1)

#define STATUS_REG                  (0x00)
#define MEAS_MODE_REG               (0x01)
#define ALG_RESULT_DATA_REG         (0x02)
#define RAW_DATA_REG                (0x03)
#define ENV_DATA_REG                (0x05)
#define NTC_REG                     (0x06)
#define THRESHOLDS_REG              (0x10)
#define BASELINE_REG                (0x11)
#define HW_ID_REG                   (0x20)
#define HW_VERSION_REG              (0x21)
#define FW_BOOT_VERSION_REG         (0x23)
#define FW_APP_VERSION_REG          (0x24)
#define ERROR_ID_REG                (0xE0)
#define APP_ERASE_REG               (0xF1)
#define APP_DATA_REG                (0xF2)
#define APP_VERIFY_REG              (0xF3)
#define APP_START_REG               (0xF4)
#define SW_RESET_REG                (0xFF)


typedef struct 
{
    uint8_t fw_mode;
    uint8_t app_valid;
    uint8_t data_ready;
    uint8_t error;
} ccs811_status_t;

typedef struct 
{
    uint8_t current;
    uint16_t voltage;
} ccs811_raw_data_t;

typedef struct 
{
    uint16_t eCO2;
    uint16_t tvoc;
    uint8_t status;
    uint8_t error;
    ccs811_raw_data_t raw_data;
} ccs811_alg_result_data_t;

ccs811_raw_data_t ccs811_read_raw(void);
ccs811_alg_result_data_t ccs811_get_data(void);
void soft_reset(void);
void ccs811_init(void);
char ccs811_get_status(void);
void ccs811_get_reg(char reg);
#endif