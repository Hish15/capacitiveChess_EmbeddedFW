#include "led.h"
#include <stm32l4xx_hal.h>
#include <FreeRTOS.h>
#include <task.h>

#define NB_LED_PER_EDGE (9)

typedef struct {
    GPIO_TypeDef *const port;
    uint32_t pin;
} bspIO_s;

led_squareTaskInfo_st led_squareTaskInfo;

/**
 * @brief list of ANODE LED IOs from LED_A to LED_I
 */
static const bspIO_s anodeLedIOtable[NB_LED_PER_EDGE]={
    {GPIOD, GPIO_PIN_14},
    {GPIOD, GPIO_PIN_12},
    {GPIOD, GPIO_PIN_10},
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_12},
    {GPIOE, GPIO_PIN_15},
    {GPIOE, GPIO_PIN_13},
    {GPIOE, GPIO_PIN_11},
    {GPIOE, GPIO_PIN_9}
};


/**
 * @brief list of CATHODE LED IOs from LED_1 to LED_9
 */
static const bspIO_s cathodeLedIOtable[NB_LED_PER_EDGE]={
    {GPIOE, GPIO_PIN_7},
    {GPIOH, GPIO_PIN_1},
    {GPIOC, GPIO_PIN_15},
    {GPIOC, GPIO_PIN_13},
    {GPIOE, GPIO_PIN_5},
    {GPIOE, GPIO_PIN_3},
    {GPIOB, GPIO_PIN_9},
    {GPIOB, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_5}
};

/**
 * @brief simply OFF all LED
 *
 */
static void led_allOff(void)
{
    // all cathodes need to be set high before any anode to avoid any blinking during powerup
    for(uint8_t i=0; i<NB_LED_PER_EDGE; i++)
    {
        HAL_GPIO_WritePin(cathodeLedIOtable[i].port, cathodeLedIOtable[i].pin, GPIO_PIN_SET);
    }
    for(uint8_t i=0; i<NB_LED_PER_EDGE; i++)
    {
        HAL_GPIO_WritePin(anodeLedIOtable[i].port, anodeLedIOtable[i].pin, GPIO_PIN_RESET);
    }
}

void led_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    // all cathodes need to be set high before any anode to avoid any blinking during powerup
    for(uint8_t i=0; i<NB_LED_PER_EDGE; i++)
    {
        GPIO_InitStruct.Pin = cathodeLedIOtable[i].pin;
        HAL_GPIO_Init(cathodeLedIOtable[i].port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(cathodeLedIOtable[i].port, cathodeLedIOtable[i].pin, GPIO_PIN_SET);
    }
    for(uint8_t i=0; i<NB_LED_PER_EDGE; i++)
    {
        GPIO_InitStruct.Pin = anodeLedIOtable[i].pin;
        HAL_GPIO_Init(anodeLedIOtable[i].port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(anodeLedIOtable[i].port, anodeLedIOtable[i].pin, GPIO_PIN_RESET);
    }
}

void led_blinkTest(void)
{
    for(uint8_t a=0; a<NB_LED_PER_EDGE; a++)
    {
        for(uint8_t k=0; k<NB_LED_PER_EDGE; k++)
        {
            HAL_GPIO_WritePin(cathodeLedIOtable[k].port, cathodeLedIOtable[k].pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(anodeLedIOtable[a].port, anodeLedIOtable[a].pin, GPIO_PIN_SET);
            vTaskDelay(10);
            HAL_GPIO_WritePin(cathodeLedIOtable[k].port, cathodeLedIOtable[k].pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(anodeLedIOtable[a].port, anodeLedIOtable[a].pin, GPIO_PIN_RESET);
        }
    }
}

void led_squareTask(void *arg)
{
    static uint8_t alternateColLedOn;
    led_squareTaskInfo.led_STI_mutexHandle = xSemaphoreCreateMutex();

    while(1)
    {
        xSemaphoreTake(led_squareTaskInfo.led_STI_mutexHandle, 0);

        led_allOff();

        if(led_squareTaskInfo.led_STI_isOn)
        {
            uint8_t raw = led_squareTaskInfo.led_STI_raw;
            uint8_t col = led_squareTaskInfo.led_STI_col;
            if(alternateColLedOn != 0)
            {
                col++;
            }
            HAL_GPIO_WritePin(cathodeLedIOtable[raw].port, cathodeLedIOtable[raw].pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(cathodeLedIOtable[raw+1].port, cathodeLedIOtable[raw+1].pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(anodeLedIOtable[col].port, anodeLedIOtable[col].pin, GPIO_PIN_SET);

            alternateColLedOn++;
            alternateColLedOn %= 2;

            // TODO if needed only: wait 1ms after LED changes state before releasing mutex to avoid switching noise
            xSemaphoreGive(led_squareTaskInfo.led_STI_mutexHandle);
        }
        else
        {
            //suspend ourself as we do not need to blink the LED
            xSemaphoreGive(led_squareTaskInfo.led_STI_mutexHandle);
            // possible race condition here if task priority are not good AND vTaskResume called only once
            vTaskSuspend(NULL);
        }
        // blink LED after short delay
        vTaskDelay(10);
    }
}
