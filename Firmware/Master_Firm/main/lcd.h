#ifndef _LCD_H_
#define _LCD_H_

#define GPIO_I2C_SDA                GPIO_NUM_26
#define GPIO_I2C_SCL                GPIO_NUM_27
#define I2C_CLOCK_FREQ              100000

#define ACK_CHECK_EN                0x1                        
#define ACK_CHECK_DIS               0x0 
#define ACK_VAL                     0x0                            
#define NACK_VAL                    0x1

#define LCD_ADDRESS                 0x27

void lcd_init(void);
void lcd_send_string(char *str);
void lcd_clear(void);
void lcd_send_data(char data);
void lcd_send_cmd(char cmd);
void lcd_task(void *param);
#endif


#endif