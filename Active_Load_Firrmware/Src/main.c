/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

volatile uint32_t cycle_count = 0;
volatile uint8_t _10m_flag = 0;
volatile uint8_t _100m_flag = 0;
volatile uint8_t _1s_flag = 0;
volatile uint8_t received_command[3] = {0, 0, 0};
volatile uint8_t data_rdy = 0;

typedef enum sys_state{
	RUN,
	SETUP,
	LOST
} sys_state;

/* UART COMMANDS BEGIN */

enum UartCommProtocol{
	UART_COM_SYS_READ = 0x01,
	UART_COM_SYS_RESET = 0x02,
	UART_COM_TMPREAD = 0x03,
	UART_COM_SET_ACTIVE_CHANNELS = 0x04,
	UART_COM_SET_RELAY = 0x05,
	UART_COM_SET_VOLT_RANGE = 0x06,
	UART_COM_START_LOAD = 0x07,
	UART_COM_STOP_LOAD = 0x08,

	UART_COM_SET_CUR_ALL = 0x10,
	UART_COM_SET_CUR_CH_1 = 0x11,
	UART_COM_SET_CUR_CH_2 = 0x12,
	UART_COM_SET_CUR_CH_3 = 0x13,
	UART_COM_SET_CUR_CH_4 = 0x14,
	UART_COM_SET_CUR_CH_5 = 0x15,
	UART_COM_SET_CUR_CH_6 = 0x16,
	UART_COM_SET_CUR_CH_7 = 0x17,
	UART_COM_SET_CUR_CH_8 = 0x18,

	UART_COM_WRITE_EEPROM_P1 = 0x21,
	UART_COM_WRITE_EEPROM_P2 = 0x22,
	UART_COM_WRITE_EEPROM_P3 = 0x23,
	UART_COM_WRITE_EEPROM_P4 = 0x24,
	UART_COM_WRITE_EEPROM_P5 = 0x25,
	UART_COM_WRITE_EEPROM_P6 = 0x26,
	UART_COM_WRITE_EEPROM_P7 = 0x27,
	UART_COM_WRITE_EEPROM_P8 = 0x28,

	UART_COM_READ_EEPROM_P1 = 0x31,
	UART_COM_READ_EEPROM_P2 = 0x32,
	UART_COM_READ_EEPROM_P3 = 0x33,
	UART_COM_READ_EEPROM_P4 = 0x34,
	UART_COM_READ_EEPROM_P5 = 0x35,
	UART_COM_READ_EEPROM_P6 = 0x36,
	UART_COM_READ_EEPROM_P7 = 0x37,
	UART_COM_READ_EEPROM_P8 = 0x38,

	UART_COM_READ_VOLT_ACTIVE_CHANNELS = 0x40,
	UART_COM_READ_VOLT_CH_1 = 0x41,
	UART_COM_READ_VOLT_CH_2 = 0x42,
	UART_COM_READ_VOLT_CH_3 = 0x43,
	UART_COM_READ_VOLT_CH_4 = 0x44,
	UART_COM_READ_VOLT_CH_5 = 0x45,
	UART_COM_READ_VOLT_CH_6 = 0x46,
	UART_COM_READ_VOLT_CH_7 = 0x47,
	UART_COM_READ_VOLT_CH_8 = 0x48

};

/* UART COMMANDS END */

#define COMM_TIMEOUT 5000
#define COMM_STIMEOUT 1

#define TC74A0_ADDRESS 0x90
#define TC74_TEMP_LOC 0x00

#define TEMP_LIMIT 120

/* GPIO DEFINES */

#define U_LED_PORT GPIOC
#define U_LED_PIN GPIO_PIN_12

#define STORE_PORT PORTA
#define STORE_PIN GPIO_PIN_4





enum EEPROM_P_ADDRESS{
	EEPROM_PAGE_1 = 0xA0, // SYSTEM INFO
	EEPROM_PAGE_2 = 0xA1, // CURRENT CALIBRATION DATA
	EEPROM_PAGE_3 = 0xA2, // VOLTAGE CALIBRATION DATA?
	EEPROM_PAGE_4 = 0xA3, // ERROR LOG?
	EEPROM_PAGE_5 = 0xA4,
	EEPROM_PAGE_6 = 0xA5,
	EEPROM_PAGE_7 = 0xA6,
	EEPROM_PAGE_8 = 0xA7
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
}

void HAL_SYSTICK_Callback(){
	cycle_count+=1;
	if(!(cycle_count%10)){
		_10m_flag = 1;
		if(!(cycle_count%100)){
			_100m_flag = 1;
			if(!(cycle_count%1000)){
				_1s_flag = 1;
				cycle_count = 0;
			}
		}
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	data_rdy = 1;
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c){
}

void inline SetFanSpeed(uint8_t heatsink_temp){
	uint8_t fan_speed = (uint8_t)((heatsink_temp-20)*(0.598)); // 20...127 -> 0...64
	__HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, fan_speed);
}

void OverTempProt(void){
	//
	//TODO
	//
}

void SendShiftReg(uint16_t *shift_reg_data){
	HAL_SPI_Transmit(&hspi2, shift_reg_data, 2, COMM_TIMEOUT);
	HAL_GPIO_TogglePin(STORE_GPIO_Port, STORE_PIN);
	HAL_GPIO_TogglePin(STORE_GPIO_Port, STORE_PIN);
}
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
		sys_state state = SETUP;
		uint8_t heatsink_temp = 0;
		uint16_t shift_reg_data = 0;
		uint8_t voltage_ranges = 0xFF;
		uint8_t active_channels = 0;
		uint8_t i = 0;

		uint8_t eeprom_buffer[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	//	uint16_t eeprom_values_dev[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF};

		/*	Calibration data: contains 12bit codes that are closest to 0 and 2A outputs */
		uint16_t calib_channel_code_2A[8] ={0, 0, 0, 0, 0, 0, 0, 0};
		uint16_t calib_channel_code_0A[8] ={0, 0, 0, 0, 0, 0, 0, 0};

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM1_Init();
  MX_TIM5_Init();
  MX_TIM9_Init();
  MX_TIM11_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  SendShiftReg(&shift_reg_data);
  while(received_command[0] != UART_COM_SYS_READ){
      	  HAL_UART_Receive(&huart1, &received_command, 1, COMM_TIMEOUT);
        }
        /* SEND SYSTEM INFO BEGINS */
        HAL_UART_Transmit(&huart1, &received_command, 1, COMM_TIMEOUT);


        /* SEND SYSTEM INFO ENDS */
        HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1);
  	  HAL_UART_Receive_DMA(&huart1, &received_command, 3);

  	  /*LOAD CALIBRATION DATA BEGINS*/

  	  /*  Loading data to EEPROM - DEV ONLY
  	   * 	  	HAL_I2C_Mem_Write(&hi2c1, EEPROM_PAGE_2, 0, 1, eeprom_values_dev, 16, COMM_TIMEOUT);
  	   *		HAL_Delay(5);
  	   *	  	HAL_I2C_Mem_Write(&hi2c1, EEPROM_PAGE_2, 16, 1, &eeprom_values_dev[8], 16, COMM_TIMEOUT);
  	   *	 	HAL_Delay(5);
  	   */

  	  HAL_I2C_Mem_Read(&hi2c1, EEPROM_PAGE_2, 0, 1, calib_channel_code_0A, 16, COMM_TIMEOUT);
  	  HAL_I2C_Mem_Read(&hi2c1, EEPROM_PAGE_2, 16, 1, calib_channel_code_2A, 16, COMM_TIMEOUT);
//  	  HAL_UART_Transmit(&huart2, calib_channel_code_0A, 16, COMM_TIMEOUT);
// 	  HAL_UART_Transmit(&huart2, calib_channel_code_2A, 16, COMM_TIMEOUT);
  	  HAL_Delay(100);

  	  /*READ CALIBRATION DATA ENDS*/
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
switch(state){
	  case SETUP:
		  if(data_rdy){
//			  HAL_UART_Transmit(&huart2, &received_command, 3, COMM_TIMEOUT);
			  switch(received_command[0]){

			  case UART_COM_TMPREAD:
				  HAL_UART_Transmit(&huart1, &heatsink_temp, 1, COMM_TIMEOUT);
				  break;

			  case UART_COM_SYS_RESET:
				  HAL_NVIC_SystemReset();
				  break;

			  case UART_COM_SET_ACTIVE_CHANNELS:
				  active_channels = received_command[1];
				  break;

			  case UART_COM_SET_RELAY:
				  shift_reg_data &= 0xFF00;
				  shift_reg_data |= received_command[1];
				  SendShiftReg(&shift_reg_data);
				  break;

			  case UART_COM_SET_VOLT_RANGE:
				  shift_reg_data &= 0x00FF;
				  shift_reg_data |= (received_command[1] << 8);
				  SendShiftReg(&shift_reg_data);
				  break;

			  case UART_COM_READ_EEPROM_P2:
			  	  HAL_I2C_Mem_Read(&hi2c1, EEPROM_PAGE_1, 0, 1, eeprom_buffer, 32, COMM_TIMEOUT);
			  	  HAL_UART_Transmit(&huart1, eeprom_buffer, 32, COMM_TIMEOUT);
				  break;

			  case UART_COM_WRITE_EEPROM_P2:
				  HAL_I2C_Mem_Write(&hi2c1, EEPROM_PAGE_2, received_command[1], 1, &received_command[2], 1, COMM_TIMEOUT);
				  break;

			  case	UART_COM_SET_CUR_CH_1:
				  //TODO, TEST
				  break;

			  case	UART_COM_SET_CUR_CH_2:
				  //TODO
				  break;

			  case	UART_COM_SET_CUR_CH_3:
				  //TODO
				  break;

			  case	UART_COM_SET_CUR_CH_4:
				  //TODO
				  break;

			  case	UART_COM_SET_CUR_CH_5:
				  //TODO
				  break;

			  case	UART_COM_SET_CUR_CH_6:
				  //TODO
				  break;

			  case	UART_COM_SET_CUR_CH_7:
				  //TODO
				  break;

			  case	UART_COM_SET_CUR_CH_8:
				  //TODO
				  break;

			  case UART_COM_START_LOAD:
				  state = RUN;
				  //TODO

				  //START LOAD

				  //TODO
				  break;

			  default:

				  break;
			  }
			  data_rdy = 0;
		  }
		  break;
	  case RUN:
		  if(data_rdy){
			  switch(received_command[0]){

			  case UART_COM_STOP_LOAD:
				  //TODO

				  //STOP LOAD

				  //TODO
				  state = SETUP;
				  break;

			  case UART_COM_READ_VOLT_ACTIVE_CHANNELS:
				  for(i = 0; i < 8; i++){
					  if(active_channels & (1 << i)){
						  HAL_UART_Transmit(&huart1, &cycle_count, 1, COMM_TIMEOUT);
					  }

				  }

				  break;
			  }


			  data_rdy = 0;
		  }

		  break;

	  case LOST:
		  //going to safe state, signaling error
		  break;
	  default:
		  break;
	  }

	  if(_1s_flag == 1){
		  //1S TASKS BEGIN
		  HAL_I2C_Mem_Read_DMA(&hi2c1, TC74A0_ADDRESS, TC74_TEMP_LOC, 1, &heatsink_temp, 1);
		  SetFanSpeed(heatsink_temp);
		  if(heatsink_temp > TEMP_LIMIT){
			  OverTempProt();
		  }

		  HAL_GPIO_TogglePin(U_LED_PORT, U_LED_PIN);
		  //1S TASKS END
		  _1s_flag = 0;
	  }

	  if(_100m_flag ==1){
		  //100MS TASKS BEGIN

		  //100MS TASKS END
		  _100m_flag = 0;
	  }

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
