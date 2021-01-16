#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include <FreeRTOS.h>
#include <semphr.h>

// TODO comment
typedef struct
{
    SemaphoreHandle_t led_STI_mutexHandle;
    uint8_t led_STI_raw;
    uint8_t led_STI_col;
    bool led_STI_isOn;
} led_squareTaskInfo_st;

// TODO comment
extern led_squareTaskInfo_st led_squareTaskInfo;

/**
 * @brief Init needed peripherals for LED usage
 */
void led_init(void);

/**
 * @brief Light every LED one after other
 */
void led_blinkTest(void);

// TODO comment
void led_squareTask(void *arg);

#endif
