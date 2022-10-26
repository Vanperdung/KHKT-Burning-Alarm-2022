/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "aht10.h"
#include "lora.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NODE_ID 0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
lora_pins_t lora_pins;
lora_t lora;
typedef struct 
{
    char *nodeID;
    char type[10];
    char alarm_status[5];
    char temp[10];
    char hum[10];
    char mq7_status[5];
} mess_t;
char *node_id[10] = {"node_1", "node_2", "node_3", "node_4"};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void debug(char *data)
{
  HAL_UART_Transmit(&huart1, (uint8_t*)data, (uint16_t)strlen(data), 1000);
}

void switch_relay(GPIO_PinState PinState)
{
	HAL_GPIO_WritePin(GPIOB, RL1_Pin, PinState);
	HAL_GPIO_WritePin(GPIOB, RL2_Pin, PinState);
	HAL_GPIO_WritePin(GPIOB, RL3_Pin, PinState);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  char data_send[128] = {0};
  char data_recv[128] = {0};
  char msg[64] = {0};
  uint32_t tick = 0;
  mess_t mess = {
    .nodeID = node_id[NODE_ID]
  };
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  lora_pins.nss.port = LORA_SS_PORT;
	lora_pins.nss.pin = LORA_SS_PIN;					
	lora_pins.reset.port = LORA_RESET_PORT;			
	lora_pins.reset.pin = LORA_RESET_PIN;			
	lora_pins.spi = &hspi1;
  lora.pin = &lora_pins;											
	lora.frequency = FREQ_433MHZ;
  sprintf(msg, "Configuring LoRa module\r\n");
	debug(msg);
  while(lora_init(&lora))
  {										
		sprintf(msg, "LoRa Failed\r\n");
		debug(msg);
		HAL_Delay(1000);
	}
  sprintf(msg, "LoRa init done\r\n");
  debug(msg);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if(HAL_GetTick() - tick > 5000 || tick == 0)
    {
      tick = HAL_GetTick();
      aht10_read_data(mess.temp, mess.hum);   
      sprintf(data_send, "$,%s,%s,*\r\n", mess.temp, mess.hum);
      if(HAL_GPIO_ReadPin(GPIOB, MQ7_DIGIT_Pin) == GPIO_PIN_RESET)
      {
        switch_relay(GPIO_PIN_SET);
        sprintf(mess.mq7_status, "on");
      }
      else
      { 
        switch_relay(GPIO_PIN_RESET);
        sprintf(mess.mq7_status, "off");
      }
    }

    uint8_t ret = lora_prasePacket(&lora);
    if(ret)
    {
      uint8_t i = 0;
      while(lora_available(&lora))
      {
        data_recv[i] = lora_read(&lora);
        i++;
      }
      data_recv[i] = '\0';
      sprintf(msg, "Receive: %s\r\n", data_recv);
      debug(msg);
      if(data_recv[0] == '$')
      {
        sscanf(data_recv, "$,%[^,],%[^,],%[^,],*", mess.nodeID, mess.type, mess.alarm_status);
        if(strstr(mess.nodeID, node_id[NODE_ID]) != NULL && strstr(mess.type, "request") != NULL)
        {
          sprintf(msg, "Receive request: ");
          debug(msg);
          lora_begin_packet(&lora);
          sprintf(data_send, "$,%s,response,%s,%s,%s,*\r\n", mess.nodeID, mess.temp, mess.hum, mess.mq7_status);
          lora_tx(&lora, (uint8_t *)data_send, strlen(data_send));
          lora_end_packet(&lora);
          debug(data_send);
        }
      }
    }
    else
      HAL_Delay(10);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the peripherals clocks
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
