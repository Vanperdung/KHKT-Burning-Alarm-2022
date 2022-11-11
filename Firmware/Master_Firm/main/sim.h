#ifndef _SIM_H_
#define _SIM_H_

#define SIM_TXD_PIN     GPIO_NUM_17
#define SIM_RXD_PIN     GPIO_NUM_16
#define SIM_STATUS_PIN  GPIO_NUM_2
#define SIM_PWRKEY_PIN  GPIO_NUM_4

esp_err_t sim_init(void);
void sim_task(void *param);
esp_err_t send_message(char *sim_mess, char *number);
#endif