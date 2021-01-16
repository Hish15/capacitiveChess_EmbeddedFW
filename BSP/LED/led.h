#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include <FreeRTOS.h>
#include <semphr.h>

/**
 * @brief structure containign all information needed for configuring LED task
 */
typedef struct
{
    /** Mutex to take when using any of following variables */
    SemaphoreHandle_t led_STI_mutexHandle;
    /** The row of the cell to be highlighted (0 to 7) */
    uint8_t led_STI_row;
    /** The column of the cell to be highlighted (0 to 7) */
    uint8_t led_STI_col;
    /** True when a cell needs to be highlighted */
    bool led_STI_isOn;
} led_squareTaskInfo_st;

/**
 * @brief This variable is used to exchange information with ethe LED managing task
 *
 */
extern led_squareTaskInfo_st led_squareTaskInfo;

/**
 * @brief Init needed peripherals for LED usage
 */
void led_init(void);

/**
 * @brief Light every LED one after other
 */
void led_blinkTest(void);

/**
 * @brief this task manages the LED blinking.
 * Which cell to highlight is managed by @ref led_squareTaskInfo
 * The task suspends itself when no LED needs to be active and then needs to be resumed
 *
 * @param arg for respect of OS format
 */
void led_squareTask(void *arg);

#endif
