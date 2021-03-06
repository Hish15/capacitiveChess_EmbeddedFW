#include <stm32l4xx_hal.h>

#include "LS013B7DH03.h"
#include "buttons.h"
#include "capacitive.h"
#include "led.h"
#include "SMPS.h"
#include "SEGGER_RTT.h"
#include <stdio.h>

#define NB_ADC_MEAS_AVG_CALIB (64)
#define NB_ADC_MEAS_AVG_DETECT (8)

static void SystemClockHSI_Config(void);
static void Error_Handler(void);

int main(void)
{
    HAL_Init();
    BSP_SMPS_Init(0);
    // connect VDD12 after 100ms
    BSP_SMPS_Supply_Enable(100,0);
    SystemClockHSI_Config();
    // choose internal voltage scale 2 so that external VDD12 is effectively used
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2);

    HAL_Delay(100);
    sharpMemoryLCD_init();
    HAL_Delay(100);




    //TODO test the buttons
    buttons_init();
    //TODO test the SMPS
    //TODO test the USB
    capacitive_init();
    HAL_Delay(500); //ensure all OPAMPS are ON
    //TODO test the EPD screen
    //TODO write of find some delay function because it will be needed


    led_init();
    led_blinkTest();

    SEGGER_RTT_WriteString(0, "-- Board startup --\r\n");

    sharpMemoryLCD_printTextLine(0, "calibrating", 11);
    HAL_Delay(10);

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
        HAL_Delay(1);
    }

    char printBuffer[11];

    for(uint8_t i=0; i<NB_CAP_CHAN/2; i++)
    {
        // one line and one colums value per screen line
        sprintf(printBuffer, "%1i %4i %4i", i, calibs[i], calibs[i+NB_CAP_CHAN/2]);
        sharpMemoryLCD_printTextLine(i, printBuffer, 11);
        HAL_Delay(10);
    }
    HAL_Delay(10000);

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
            HAL_Delay(10);
        }


        //uint8_t c = buttons_isPressed(BUTTON_CENTER) ? 1:0;
        //uint8_t u = buttons_isPressed(BUTTON_UP) ? 1:0;
        //uint8_t d = buttons_isPressed(BUTTON_DOWN) ? 1:0;
        //uint8_t l = buttons_isPressed(BUTTON_LEFT) ? 1:0;
        //uint8_t r = buttons_isPressed(BUTTON_RIGHT) ? 1:0;
        //sprintf(printBuffer, "%i %i %i %i %i",c,u,d,l,r);
        //sharpMemoryLCD_printTextLine(5, "C U D L R", 11);
        //HAL_Delay(10);
        //sharpMemoryLCD_printTextLine(6, printBuffer, 11);
        //HAL_Delay(10);

        HAL_Delay(50);
    }

    return 0;
}


void SysTick_Handler(void)
{
  HAL_IncTick();
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
    // TODO print on screen before dying in while
    while(1)
    {
        ;
    }
}
