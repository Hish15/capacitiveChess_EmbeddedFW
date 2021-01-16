#include "capacitive.h"
#include "led.h"
#include <stm32l4xx_hal.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/**
 * @brief This is just to remove the magic "16"
 */
#define NB_MULTIPLEX_PINS    (16)

#define OPAMP_STABILIZATION_DELAY_MS (2)

typedef struct {
    GPIO_TypeDef *const port;
    uint32_t pin;
} bspIO_s;

typedef struct {
    GPIO_TypeDef *const port;
    uint32_t pin;
    uint32_t adcChan;
} bspIOan_s;

/**
 * @brief list of EN IOs from EN_1 to EN_8 and EN_A to EN_H
 */
static const bspIO_s enIOtable[NB_MULTIPLEX_PINS]={
    {GPIOB, GPIO_PIN_2},
    {GPIOH, GPIO_PIN_0},
    {GPIOC, GPIO_PIN_14},
    {GPIOE, GPIO_PIN_6},
    {GPIOE, GPIO_PIN_4},
    {GPIOE, GPIO_PIN_0},
    {GPIOB, GPIO_PIN_8},
    {GPIOB, GPIO_PIN_6},
    {GPIOD, GPIO_PIN_13},
    {GPIOD, GPIO_PIN_11},
    {GPIOD, GPIO_PIN_9},
    {GPIOB, GPIO_PIN_13},
    {GPIOB, GPIO_PIN_10},
    {GPIOE, GPIO_PIN_14},
    {GPIOE, GPIO_PIN_12},
    {GPIOE, GPIO_PIN_10}
};

/**
 * @brief list of TOP IOs from TOP_1 to TOP_8 and TOP_A to TOP_H
 */
static const bspIOan_s topIOtable[NB_MULTIPLEX_PINS]={
    {GPIOA, GPIO_PIN_3, ADC_CHANNEL_8},
    {GPIOA, GPIO_PIN_2, ADC_CHANNEL_7},
    {GPIOA, GPIO_PIN_1, ADC_CHANNEL_6},
    {GPIOA, GPIO_PIN_0, ADC_CHANNEL_5},
    {GPIOC, GPIO_PIN_3, ADC_CHANNEL_4},
    {GPIOC, GPIO_PIN_2, ADC_CHANNEL_3},
    {GPIOC, GPIO_PIN_1, ADC_CHANNEL_2},
    {GPIOC, GPIO_PIN_0, ADC_CHANNEL_1},
    {GPIOB, GPIO_PIN_1, ADC_CHANNEL_16},
    {GPIOB, GPIO_PIN_0, ADC_CHANNEL_15},
    {GPIOC, GPIO_PIN_5, ADC_CHANNEL_14},
    {GPIOC, GPIO_PIN_4, ADC_CHANNEL_13},
    {GPIOA, GPIO_PIN_7, ADC_CHANNEL_12},
    {GPIOA, GPIO_PIN_6, ADC_CHANNEL_11},
    {GPIOA, GPIO_PIN_5, ADC_CHANNEL_10},
    {GPIOA, GPIO_PIN_4, ADC_CHANNEL_9}
};


ADC_HandleTypeDef AdcHandle;
ADC_ChannelConfTypeDef sConfig;




/**
 * @brief configure ADC for selected channel and make ADC measurement sequence
 *
 * @param ADCrawMeas pointer to the output ADC value
 * @param HAL_ADC_chan the ADC channel to convert
 * @return true if everything went fine
 */
static bool convertADCchannel(uint16_t *ADCrawMeas, uint32_t HAL_ADC_chan)
{
    bool resultSuccess = true;

    sConfig.Channel = HAL_ADC_chan;

    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK)
    {
        resultSuccess = false;
    }

    if (HAL_ADC_Start(&AdcHandle) != HAL_OK)
    {
        resultSuccess = false;
    }

    if (HAL_ADC_PollForConversion(&AdcHandle, 10) != HAL_OK || ADCrawMeas==NULL)
    {
        resultSuccess = false;
    }
    else
    {
        *ADCrawMeas = HAL_ADC_GetValue(&AdcHandle);
    }

    return resultSuccess;
}


bool capacitive_init(void)
{
    bool resultSuccess = true;

    /* **** Init the GPIOs **** */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_ADC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    for(uint8_t i=0; i<NB_MULTIPLEX_PINS; i++)
    {
        // init TOP pins to output GND
        GPIO_InitStruct.Pin = topIOtable[i].pin;
        HAL_GPIO_Init(topIOtable[i].port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(topIOtable[i].port, topIOtable[i].pin, GPIO_PIN_RESET);

        // init EN pins to output VCC (MCP6043 has CSN pin)
        GPIO_InitStruct.Pin = enIOtable[i].pin;
        HAL_GPIO_Init(enIOtable[i].port, &GPIO_InitStruct);
        // enable all opamps (they are soldered to have separate shields)
        HAL_GPIO_WritePin(enIOtable[i].port, enIOtable[i].pin, GPIO_PIN_RESET);
    }

    /* **** Init the ADC **** */
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_SYSCLK);

    AdcHandle.Instance          = ADC1;

    if (HAL_ADC_DeInit(&AdcHandle) != HAL_OK)
    {
        resultSuccess = false;
    }

    AdcHandle.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV256;          /* Asynchronous clock mode, input ADC clock not divided */
    AdcHandle.Init.Resolution            = ADC_RESOLUTION_12B;            /* 12-bit resolution for converted data */
    AdcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;           /* Right-alignment for converted data */
    AdcHandle.Init.ScanConvMode          = DISABLE;                       /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
    AdcHandle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;           /* EOC flag picked-up to indicate conversion end */
    AdcHandle.Init.LowPowerAutoWait      = DISABLE;                       /* Auto-delayed conversion feature disabled */
    AdcHandle.Init.ContinuousConvMode    = DISABLE;                       /* Continuous mode disabled to have only 1 conversion at each conversion trig */
    //AdcHandle.Init.NbrOfConversion       = 1;                             /* Parameter discarded because sequencer is disabled */
    //AdcHandle.Init.DiscontinuousConvMode = DISABLE;                       /* Parameter discarded because sequencer is disabled */
    //AdcHandle.Init.NbrOfDiscConversion   = 1;                             /* Parameter discarded because sequencer is disabled */
    AdcHandle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;            /* Software start to trig the 1st conversion manually, without external event */
    //AdcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE; /* Parameter discarded because software trigger chosen */
    AdcHandle.Init.DMAContinuousRequests = DISABLE;                       /* DMA one-shot mode selected (not applied to this example) */
    AdcHandle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;      /* DR register is overwritten with the last conversion result in case of overrun */
    AdcHandle.Init.OversamplingMode      = DISABLE;                       /* No oversampling */

    if (HAL_ADC_Init(&AdcHandle) != HAL_OK)
    {
        resultSuccess = false;
    }

    if (HAL_ADCEx_Calibration_Start(&AdcHandle, ADC_SINGLE_ENDED) != HAL_OK)
    {
        resultSuccess = false;
    }

    sConfig.Rank         = ADC_REGULAR_RANK_1;          /* Rank of sampled channel number ADCx_CHANNEL */
    sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;  /* Sampling time (number of clock cycles unit) */
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;            /* Single-ended input channel */
    sConfig.OffsetNumber = ADC_OFFSET_NONE;             /* No offset subtraction */

    return resultSuccess;
}




bool capacitive_getADCvalue(uint8_t capChannel, uint16_t *ADCrawMeas)
{
    bool resultSuccess = true;

    // 1) charge the correct TOP plate to VCC
    HAL_GPIO_WritePin(topIOtable[capChannel].port, topIOtable[capChannel].pin, GPIO_PIN_SET);
    vTaskDelay(OPAMP_STABILIZATION_DELAY_MS);
    // 2) discharge the sample and hold cap by making a measurement to GND
    // this is made by converting the channel while pin is configured as output
    // doing 2 measurements gives more reproductible results
    resultSuccess &= convertADCchannel(ADCrawMeas, topIOtable[capChannel].adcChan);
    resultSuccess &= convertADCchannel(ADCrawMeas, topIOtable[capChannel].adcChan);
    // 3) the charged TOP pin configures to ADC input
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Pin = topIOtable[capChannel].pin;
    // avoid LED switching during high impedance states
    // wait for mutex infinite time
    xSemaphoreTake(led_squareTaskInfo.led_STI_mutexHandle, portMAX_DELAY);
    HAL_GPIO_Init(topIOtable[capChannel].port, &GPIO_InitStruct);
    // 4) sample and hold the TOP plate (will discharge "parasitic" cap into sample and hold)
    resultSuccess &= convertADCchannel(ADCrawMeas, topIOtable[capChannel].adcChan);
    // 5) put everything back to original configuration
    // pin top is output
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(topIOtable[capChannel].port, &GPIO_InitStruct);
    xSemaphoreGive(led_squareTaskInfo.led_STI_mutexHandle);
    // pin top LOW level
    HAL_GPIO_WritePin(topIOtable[capChannel].port, topIOtable[capChannel].pin, GPIO_PIN_RESET);

    return resultSuccess;
}

