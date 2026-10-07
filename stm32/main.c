/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim10;
TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

uint8_t rx_packet[4];
char tx_data[128];
volatile uint8_t uart_tx_ready=1;
volatile int16_t target_speed1 = 0;
int16_t current_speed1= 0;
volatile int16_t target_speed2 = 0;
int16_t current_speed2= 0;
volatile int16_t target_speed3 = 0;
int16_t current_speed3= 0;
volatile int16_t target_speed4 = 0;
int16_t current_speed4= 0;

int16_t encoder_pulses1 = 0;
int16_t encoder_pulses2 = 0;
int16_t encoder_pulses3 = 0;
int16_t encoder_pulses4 = 0;

float wheel_speed1 = 0.0;
float wheel_speed2 = 0.0;
float wheel_speed3 = 0.0;
float wheel_speed4 = 0.0;



typedef enum {
    HCSR04_TRIG_SENSOR1,
    HCSR04_WAIT_FOR_SENSOR2,
    HCSR04_TRIG_SENSOR2,
    HCSR04_WAIT_FOR_NEXT_CYCLE
} HCSR04_State_t;

HCSR04_State_t hcsr04_current_state = HCSR04_TRIG_SENSOR1;
uint32_t hcsr04_last_tick = 0;

volatile uint32_t t1_start = 0, t1_stop = 0, t1_duration = 0;
volatile uint8_t czujnik1_flaga = 0;
float distance_sensor1 = 0.0f;

volatile uint32_t t2_start = 0, t2_stop = 0, t2_duration = 0;
volatile uint8_t czujnik2_flaga = 0;
float distance_sensor2 = 0.0f;


uint16_t adc_raw_buffer[2];
float termistor_temp = 0.0f;
float CPU_temp = 0.0f;

#define TS_CAL1 *((uint16_t*)0x1FFF7A2C)
#define TS_CAL2 *((uint16_t*)0x1FFF7A2E)
#define TS_CAL1_TEMP 30.0f
#define TS_CAL2_TEMP 110.0f

const float BETA = 3950.0f;
const float R_ROV = 22000.0f;
const float R_NOM = 22000.0f;
const float T_NOM = 298.15f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM5_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM10_Init(void);
static void MX_TIM14_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
void delay_us(uint16_t us);
float Przelicz_Termistor(uint16_t raw_adc);
float Przelicz_Temp_Procesora(uint16_t raw_adc);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        uart_tx_ready = 1;
    }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        	for (int i = 0; i < 4; i++)
        	        {
        	            uint8_t current_byte = rx_packet[i];

        	            uint8_t motor_id  = (current_byte >> 6) & 0x03;
        	            uint8_t direction = (current_byte >> 5) & 0x01;
        	            uint8_t speed_raw = current_byte & 0x1F;


        	            int16_t speed_pwm = speed_raw * 33;
        	            if (direction == 1)
        	            {
        	                speed_pwm = -speed_pwm;
        	            }


        	            switch (motor_id)
        	            {
        	                case 0: target_speed1 = speed_pwm; break;
        	                case 1: target_speed2 = speed_pwm; break;
        	                case 2: target_speed3 = speed_pwm; break;
        	                case 3: target_speed4 = speed_pwm; break;
        	                default: break;
        	            }
        	        }
            HAL_UART_Receive_IT(&huart3, rx_packet, 4);

    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
    	__IO uint32_t tmpreg = 0x00U;
    	        tmpreg = huart->Instance->SR;
    	        tmpreg = huart->Instance->DR;
    	        (void)tmpreg;        HAL_UART_Receive_IT(&huart3, rx_packet, 4);
    }
}
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{

    if (htim->Instance == TIM10)
    {
        if (czujnik1_flaga == 0)
        {
            t1_start = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            czujnik1_flaga = 1;
        }
        else
        {
            t1_stop = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

            if (t1_stop >= t1_start) t1_duration = t1_stop - t1_start;
            else t1_duration = (65535 - t1_start) + t1_stop;

            distance_sensor1 = (t1_duration * 0.0343f) / 2.0f;
            if (distance_sensor1 <= 0.0f)
            {
                distance_sensor1 = 0.0f;
            }
            czujnik1_flaga = 0;
        }
    }

    else if (htim->Instance == TIM14)
    {
        if (czujnik2_flaga == 0)
        {
            t2_start = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            czujnik2_flaga = 1;
        }
        else
        {
            t2_stop = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

            if (t2_stop >= t2_start) t2_duration = t2_stop - t2_start;
            else t2_duration = (65535 - t2_start) + t2_stop;

            distance_sensor2 = (t2_duration * 0.0343f) / 2.0f;
            if (distance_sensor2 <= 0.0f)
                      {
                          distance_sensor2 = 0.0f;
                      }
            czujnik2_flaga = 0;
        }
    }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_USART3_UART_Init();
  MX_TIM10_Init();
  MX_TIM14_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  char *msg = "STM32 F446RE - TIM1 1000Hz - Inteligentny Soft-Start!\r\n";
    HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg));


    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

    HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);

    HAL_TIM_IC_Start_IT(&htim10, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_raw_buffer,2);

    __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_RXNE | UART_FLAG_ORE);
    volatile uint32_t dummy = huart3.Instance->DR;
    (void)dummy;
    HAL_UART_Receive_IT(&huart3, rx_packet, 4);
    uint32_t last_ramp_time = 0;

    uint32_t last_encoder_read_time = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  uint32_t current_tick = HAL_GetTick();
	  if (current_tick - last_ramp_time >= 1)
	        {
	            last_ramp_time = current_tick;

	            if (current_speed1 < target_speed1)
	            {
	                current_speed1 += 3;
	                if (current_speed1 > target_speed1) current_speed1 = target_speed1;
	            }
	            else if (current_speed1 > target_speed1)
	            {
	                current_speed1 -= 3;
	                if (current_speed1 < target_speed1) current_speed1 = target_speed1;
	            }


	            if (current_speed2 < target_speed2)
	            {
	            	current_speed2 += 3;
	            	if (current_speed2 > target_speed2) current_speed2 = target_speed2;
	            }
	            else if (current_speed2 > target_speed2)
	            {
	            	current_speed2 -= 3;
	            	if (current_speed2 < target_speed2) current_speed2 = target_speed2;
	            }


	            if (current_speed3 < target_speed3)
	            {
	           	    current_speed3 += 3;
	           	    if (current_speed3 > target_speed3) current_speed3 = target_speed3;
	            }
	           	else if (current_speed3 > target_speed3)
	           	{
	           	    current_speed3 -= 3;
	           	    if (current_speed3 < target_speed3) current_speed3 = target_speed3;
	           	}

	           	if (current_speed4 < target_speed4)
	           	{
	           	    current_speed4 += 3;
	           	   if (current_speed4 > target_speed4) current_speed4 = target_speed4;
	           	   }
	           	   else if (current_speed4 > target_speed4)
	           	   {
	           		   current_speed4 -= 3;
	           	       if (current_speed4 < target_speed4) current_speed4 = target_speed4;
	           	   }
	            // ===== FIZYCZNE STEROWANIE NOWYMI PINAMI (CN10) =====

	            if (current_speed1 > 0)
	            {
	                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
	                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
	                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, current_speed1);
	            }
	            else if (current_speed1 < 0)
	            {
	                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);

	                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, -current_speed1);
	            }
	            else
	            {
	                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

	                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
	            }



	            if (current_speed2 > 0)
	           	{
	           	   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
	           	   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	           	   __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, current_speed2);
	           	}
	           	else if (current_speed2 < 0)
	           	{
	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
	           	    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, -current_speed2);
	           	}
	           	else
	           	{
	           	    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	           	}

	            if (current_speed3 > 0)
	            {
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
	            	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, current_speed3);
	            }
	            else if (current_speed3 < 0)
	            {
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
	            	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, -current_speed3);
	            }
	            else
	            {
	            	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0);
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
	            }



	            if (current_speed4 > 0)
	            {
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	            	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	            	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, current_speed4);
	            }
	            else if (current_speed4 < 0)
	            {
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	            	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
	            	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, -current_speed4);
	            }
	            else
	            {
	            	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
	            	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	            	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	            }

	            if (current_tick - last_encoder_read_time >= 1000)
	                  {
	                      last_encoder_read_time = current_tick;


	                      encoder_pulses1 = (int16_t)__HAL_TIM_GET_COUNTER(&htim5);
	                      encoder_pulses2 = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
	                      encoder_pulses3 = (int16_t)__HAL_TIM_GET_COUNTER(&htim4);
	                      encoder_pulses4 = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);



	                      //11x4x30 przekładnia
	                      //50ms
	                      wheel_speed1 = (encoder_pulses1 *60)/1320.0;
	                      wheel_speed2 = (encoder_pulses2 *60)/1320.0;
	                      wheel_speed3 = (encoder_pulses3 *60)/1320.0;
	                      wheel_speed4 = (encoder_pulses4 *60)/1320.0;

	                      __HAL_TIM_SET_COUNTER(&htim2, 0);
	                      __HAL_TIM_SET_COUNTER(&htim3, 0);
	                      __HAL_TIM_SET_COUNTER(&htim4, 0);
	                      __HAL_TIM_SET_COUNTER(&htim5, 0);
	                      if (uart_tx_ready == 1){
	                     	  		  uart_tx_ready=0;
	                     	  		  int len = snprintf(tx_data, sizeof(tx_data),
	                     	  				  "{\"s1\":%.2f,\"s2\":%.2f,\"s3\":%.2f,\"s4\":%.2f,\"d1\":%.2f,\"d2\":%.2f,\"t1\":%.2f,\"t2\":%.2f,\"t3\":\n",
	                     					  wheel_speed1, wheel_speed2, wheel_speed3, wheel_speed4,distance_sensor1,distance_sensor2,termistor_temp,CPU_temp);
	                     	  		HAL_UART_Transmit_IT(&huart3, (uint8_t*)tx_data, len);
	                    }
	                  }
	        }
	  	  	switch (hcsr04_current_state)
	  	                {
	  	                    case HCSR04_TRIG_SENSOR1:

	  	                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
	  	                        delay_us(10);
	  	                        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);

	  	                        hcsr04_last_tick = current_tick;
	  	                        hcsr04_current_state = HCSR04_WAIT_FOR_SENSOR2;
	  	                        break;

	  	                    case HCSR04_WAIT_FOR_SENSOR2:
	  	                        if (current_tick - hcsr04_last_tick >= 30)
	  	                        {
	  	                            hcsr04_current_state = HCSR04_TRIG_SENSOR2;
	  	                        }
	  	                        break;

	  	                    case HCSR04_TRIG_SENSOR2:
	  	                        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
	  	                        delay_us(10);
	  	                        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);

	  	                        hcsr04_last_tick = current_tick;
	  	                        hcsr04_current_state = HCSR04_WAIT_FOR_NEXT_CYCLE;
	  	                        break;

	  	                    case HCSR04_WAIT_FOR_NEXT_CYCLE:
	  	                        if (current_tick - hcsr04_last_tick >= 70)
	  	                        {
	  	                            hcsr04_current_state = HCSR04_TRIG_SENSOR1;
	  	                        }
	  	                        break;
	  	                }
	  	  uint16_t raw_termistor = adc_raw_buffer[0];
	  	  uint16_t raw_procesor  = adc_raw_buffer[1];
	  	  termistor_temp = Przelicz_Termistor(raw_termistor);
	  	  CPU_temp = Przelicz_Temp_Procesora(raw_procesor);


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 179;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1023;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 65535;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler = 83;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 65535;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim10, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 83;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 65535;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim14, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|BIN22_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, AIN11_Pin|TRIG2_Pin|AIN22_Pin|BIN12_Pin
                          |BIN21_Pin|AIN21_Pin|AIN12_Pin|BIN11_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TRIG1_GPIO_Port, TRIG1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin BIN22_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|BIN22_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : AIN11_Pin TRIG2_Pin AIN22_Pin BIN12_Pin
                           BIN21_Pin AIN21_Pin AIN12_Pin BIN11_Pin */
  GPIO_InitStruct.Pin = AIN11_Pin|TRIG2_Pin|AIN22_Pin|BIN12_Pin
                          |BIN21_Pin|AIN21_Pin|AIN12_Pin|BIN11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : TRIG1_Pin */
  GPIO_InitStruct.Pin = TRIG1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TRIG1_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void delay_us(uint16_t us)
{
    volatile uint32_t count = us * 20;
    while(count--);
}
float Przelicz_Termistor(uint16_t raw_adc)
{
    if (raw_adc == 0 || raw_adc >= 4095) return -273.15f;

    float rezystancja = R_ROV / ((4095.0f / (float)raw_adc) - 1.0f);

    float kelwiny = 1.0f / ((1.0f / T_NOM) + (1.0f / BETA) * log(rezystancja / R_NOM));

    return kelwiny - 273.15f;
}

float Przelicz_Temp_Procesora(uint16_t raw_adc)
{

    if (TS_CAL2 == TS_CAL1) return 0.0f;

    float temp = ((TS_CAL2_TEMP - TS_CAL1_TEMP) / (float)(TS_CAL2 - TS_CAL1)) * (raw_adc - TS_CAL1) + TS_CAL1_TEMP;
    return temp;
}
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
#ifdef USE_FULL_ASSERT
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
