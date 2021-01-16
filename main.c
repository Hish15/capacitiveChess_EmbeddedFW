#include <stm32l4xx_hal.h>

#include "LS013B7DH03.h"
#include "buttons.h"
#include "capacitive.h"
#include "led.h"
#include "SMPS.h"
#include "SEGGER_RTT.h"
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>

#define NB_ADC_MEAS_AVG_CALIB (64)
#define NB_ADC_MEAS_AVG_DETECT (8)

static void SystemClockHSI_Config(void);
static void Error_Handler(void);
static void mainTask(void *arg);


static void mainTask(void *arg)
{
    BSP_SMPS_Init(0);
    // connect VDD12 after 100ms
    BSP_SMPS_Supply_Enable(100,0);
    SystemClockHSI_Config();
    // choose internal voltage scale 2 so that external VDD12 is effectively used
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2);

    vTaskDelay(1);
    sharpMemoryLCD_init();
    vTaskDelay(100);

    led_init();
    led_blinkTest();

    buttons_init();
    capacitive_init();
    vTaskDelay(500); //ensure all OPAMPS are ON

    SEGGER_RTT_WriteString(0, "-- Board startup --\r\n");

    sharpMemoryLCD_printTextLine(0, "calibrating", 11);
    vTaskDelay(10);

    uint16_t rawADC;
    uint16_t rawValsCalib[NB_CAP_CHAN][NB_ADC_MEAS_AVG_CALIB];
    uint16_t calibs[NB_CAP_CHAN];

    for(uint8_t i=0; i<NB_ADC_MEAS_AVG_CALIB; i++)
    {
        for(uint8_t chan=0; chan<NB_CAP_CHAN; chan++)
        {
            capacitive_getADCvalue(chan, &rawADC);
            rawValsCalib[chan][i] = rawADC;
        }
    }

    // compute mean values
    for(uint8_t chan=0; chan<NB_CAP_CHAN; chan++)
    {
        uint32_t mean = 0;
        for(uint8_t i=0; i<NB_ADC_MEAS_AVG_CALIB; i++)
        {
            mean +=  rawValsCalib[chan][i];
        }
        calibs[chan] = mean/NB_ADC_MEAS_AVG_CALIB;
    }

    SEGGER_RTT_WriteString(0, "calib values\r\n");
    SEGGER_RTT_WriteString(0, "1    2    3    4    5    6    7    8    a    b    c    d    e    f    g    h\r\n");
    for(uint8_t i=0; i<NB_ADC_MEAS_AVG_CALIB; i++)
    {
        for(uint8_t chan=0; chan<NB_CAP_CHAN; chan++)
        {
            SEGGER_RTT_printf(0, "%d ",rawValsCalib[chan][i]);
        }
        SEGGER_RTT_WriteString(0, "\r\n");
        vTaskDelay(1);
    }

    char printBuffer[11];

    for(uint8_t i=0; i<NB_CAP_CHAN/2; i++)
    {
        // one line and one colums value per screen line
        sprintf(printBuffer, "%1i %4i %4i", i, calibs[i], calibs[i+NB_CAP_CHAN/2]);
        sharpMemoryLCD_printTextLine(i, printBuffer, 11);
        vTaskDelay(10);
    }
    vTaskDelay(500);


    xTaskCreate(led_squareTask, "led_squareTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);


    while(1)
    {
        SEGGER_RTT_WriteString(0, "SEGGER Hello World\r\n");
        SEGGER_RTT_printf(0, "test %d\r\n", 1234);

        uint16_t rawMeas[NB_CAP_CHAN][NB_ADC_MEAS_AVG_DETECT];
        uint16_t meanMeas[NB_CAP_CHAN];
        for(uint8_t i=0; i<NB_ADC_MEAS_AVG_DETECT; i++)
        {
            for(uint8_t chan=0; chan<NB_CAP_CHAN; chan++)
            {
                capacitive_getADCvalue(chan, &rawADC);
                rawMeas[chan][i] = rawADC;
            }
        }


        // compute mean values
        for(uint8_t chan=0; chan<NB_CAP_CHAN; chan++)
        {
            uint32_t mean = 0;
            for(uint8_t i=0; i<NB_ADC_MEAS_AVG_DETECT; i++)
            {
                mean +=  rawMeas[chan][i];
            }
            meanMeas[chan] = mean/NB_ADC_MEAS_AVG_DETECT;
        }



        for(uint8_t i=0; i<NB_CAP_CHAN/2; i++)
        {
            // one line and one colums value per screen line
            sprintf(printBuffer, "%1i %4i %4i", i, meanMeas[i]-calibs[i], meanMeas[i+NB_CAP_CHAN/2]-calibs[i+NB_CAP_CHAN/2]);
            sharpMemoryLCD_printTextLine(i, printBuffer, 11);
            vTaskDelay(10);
        }


        vTaskDelay(50);
    }
}

int main(void)
{
    HAL_Init();

    led_squareTaskInfo.led_STI_mutexHandle = xSemaphoreCreateMutex();
    // wait for mutex infinite time
    xSemaphoreTake(led_squareTaskInfo.led_STI_mutexHandle, portMAX_DELAY);
    led_squareTaskInfo.led_STI_row = 5;
    led_squareTaskInfo.led_STI_col = 2;
    led_squareTaskInfo.led_STI_isOn = true;
    xSemaphoreGive(led_squareTaskInfo.led_STI_mutexHandle);

    xTaskCreate(mainTask, "mainTask", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();

    while(1);
    return 0;
}



/**
  * @brief  Switch the PLL source from MSI  to HSI, and select the PLL as SYSCLK
  *         source.
  *         The system Clock is configured as follows :
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 120000000
  *            HCLK(Hz)                       = 120000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            MSI Frequency(Hz)              = 4000000
  *            HSI Frequency(Hz)              = 16000000
  *            PLLM                           = 2
  *            PLLN                           = 30
  *            PLLP                           = 7
  *            PLLQ                           = 4
  *            PLLR                           = 2
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClockHSI_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* -1- Select MSI as system clock source to allow modification of the PLL configuration */
  RCC_ClkInitStruct.ClockType       = RCC_CLOCKTYPE_SYSCLK;
  RCC_ClkInitStruct.SYSCLKSource    = RCC_SYSCLKSOURCE_MSI;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* -2- Enable voltage range 1 boost mode for frequency above 80 Mhz */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  __HAL_RCC_PWR_CLK_DISABLE();

  /* -3- Enable HSI Oscillator, select it as PLL source and finally activate the PLL */
  RCC_OscInitStruct.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState             = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState         = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM             = 2;
  RCC_OscInitStruct.PLL.PLLN             = 30;
  RCC_OscInitStruct.PLL.PLLP             = 7;
  RCC_OscInitStruct.PLL.PLLQ             = 4;
  RCC_OscInitStruct.PLL.PLLR             = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* -4- To avoid undershoot due to maximum frequency, select PLL as system clock source */
  /* with AHB prescaler divider 2 as first step */
  RCC_ClkInitStruct.ClockType       = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider  = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider  = RCC_HCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* -5- AHB prescaler divider at 1 as second step */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* -6- Optional: Disable MSI Oscillator */
  RCC_OscInitStruct.OscillatorType  = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState        = RCC_MSI_OFF;
  RCC_OscInitStruct.PLL.PLLState    = RCC_PLL_NONE;  /* No update on PLL */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
}


static void Error_Handler(void)
{
    while(1)
    {
        ;
    }
}

void assert_failed(uint8_t *file, uint32_t line)
{
    sharpMemoryLCD_printTextLine(0,"  ASSERT", 8);
    // TODO print file and line
    while(1)
    {
        ;
    }
}
