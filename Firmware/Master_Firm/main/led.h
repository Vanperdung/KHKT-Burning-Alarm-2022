#ifndef _LED_H_
#define _LED_H_

#define LED_ONOFF_PIN       GPIO_NUM_32
#define LED_STATUS_PIN      GPIO_NUM_33
#define LED_ON              1
#define LED_OFF             0

void led_status_init(void);
void led_onoff_init(void);
void led_task(void *param);

#endif