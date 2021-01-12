#include "SMPS.h"

#include <stm32l4xx_hal.h>
#include <FreeRTOS.h>
#include <task.h>


#define PORT_SMPS               GPIOC
//#define PIN_SMPS_ENABLE         GPIO_PIN_4
//#define PIN_SMPS_V1             GPIO_PIN_5
//#define PIN_SMPS_POWERGOOD      GPIO_PIN_6
#define PIN_SMPS_SWITCH_ENABLE  GPIO_PIN_6

#define PWR_GPIO_SMPS           PWR_GPIO_C
//#define PWR_GPIO_ENABLE         PWR_GPIO_BIT_4
#define PWR_GPIO_SWITCH_ENABLE  PWR_GPIO_BIT_6
#define PWR_PU_REG              PUCRC

#define NUCLEO_SMPS_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()
#define NUCLEO_SMPS_GPIO_CLK_DISABLE() __HAL_RCC_GPIOC_CLK_DISABLE()

#define PWR_AND_CLK_SMPS()   do { __HAL_RCC_PWR_CLK_ENABLE(); \
                                  __HAL_RCC_GPIOC_CLK_ENABLE(); } while(0)



uint32_t BSP_SMPS_DeInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  PWR_AND_CLK_SMPS();

  /* --------------------------------------------------------------------------------------  */
  /* Added for Deinit if No PIN_SMPS_ENABLE & PIN_SMPS_SWITCH_ENABLE are not disabled before */

  /* Disable SMPS SWITCH */
  HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_SWITCH_ENABLE, GPIO_PIN_RESET);

  vTaskDelay(1);

  /* Disable SMPS */
//  HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_ENABLE, GPIO_PIN_RESET);

  /* --------------------------------------------------------------------------------------  */
  /* Set all GPIO in output push/pull pulldown state to reduce power consumption  */
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Pull =  GPIO_PULLDOWN;

  /* Consider all SMPS pins but V1, not used on ADP5301ACBZ */
  GPIO_InitStruct.Pin = /*PIN_SMPS_ENABLE | */PIN_SMPS_SWITCH_ENABLE /*| PIN_SMPS_POWERGOOD*/;
  HAL_GPIO_Init(PORT_SMPS, &GPIO_InitStruct);

  return SMPS_OK;
}


uint32_t BSP_SMPS_Init(uint32_t VoltageRange)
{
  PWR_AND_CLK_SMPS();

  GPIO_InitTypeDef GPIO_InitStruct;

  /* Reconfigure PWR_PUCRx/PDCRx registers only when not coming */
  /* back from Standby or Shutdown states.                      */
  /* Consider as well non-SMPS related pins.                     */
  if (!(READ_BIT(PWR->PWR_PU_REG, PWR_GPIO_SWITCH_ENABLE)))
  {
    HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_SMPS, PWR_GPIO_SWITCH_ENABLE);
    //HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_SMPS, PWR_GPIO_ENABLE);

    /* HW limitation: Level shifter consumes because of dangling, so pull PA2 up
      (LPUART1_TX), PA13 (SWD/TMS) and PB3 (SWO) */
    //HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_A, GPIO_PIN_2); /* LPUART1_TX */
    HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_A, GPIO_PIN_13); /* SWD/TMS    */
    HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_B, GPIO_PIN_3); /* SWO        */

    /* Don't set PWR_CR3 APC bit at this time as it increases power
      consumption in non-Standby/Shutdown modes. It will have to be
      set with HAL_PWREx_EnablePullUpPullDownConfig() API upon
      Standby or Shutdown modes entering */
  }
  /* ------------------------------------------------------------------------ */
//  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//  GPIO_InitStruct.Pull = GPIO_PULLUP;
//
//  GPIO_InitStruct.Pin = PIN_SMPS_POWERGOOD;
//  HAL_GPIO_Init(PORT_SMPS, &GPIO_InitStruct);

  /* ------------------------------------------------------------------------ */

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  GPIO_InitStruct.Pin = /*PIN_SMPS_ENABLE |*/ PIN_SMPS_SWITCH_ENABLE;
  HAL_GPIO_Init(PORT_SMPS, &GPIO_InitStruct);

  /* --------- SMPS VOLTAGE RANGE SELECTION ----------------------------------*/
  /* ######################################################################## */
  /* - > Not applicable to ADP5301ACBZ on MB1319 */
  /* ######################################################################## */
  /* - > Applicable to ST1PS02D1QTR */
  /* Control to be added */

  /* ST1PS02D1QTR on MB1312 */
  /* if (VoltageRange == ST1PS02D1QTR_VOUT_1_25) */
  /* HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_V1, GPIO_PIN_SET); */
  /* 1.25V                  */
  /* D0/D1/D2 = H/L/L       */
  /* else */

  /* */
  /* ST1PS02D1QTR on MB1312 */
  /* ST1PS02D1QTR_VOUT_1_05 */
  /* 1.05V                  */
  /* D0/D1/D2 = L/L/L       */
  /* HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_V1, GPIO_PIN_RESET); */
  /* ######################################################################## */
  return SMPS_OK;
}


uint32_t BSP_SMPS_Enable(uint32_t Delay, uint32_t Power_Good_Check)
{
//  PWR_AND_CLK_SMPS();
//
//  HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_ENABLE, GPIO_PIN_SET);
//
  /* Delay upon request */
  if (Delay != 0)
  {
    vTaskDelay(Delay);
  }
//
//  /* CHECK POWER GOOD or NOT */
//  if (Power_Good_Check != 0)
//  {
//    if (GPIO_PIN_RESET == (HAL_GPIO_ReadPin(PORT_SMPS, PIN_SMPS_POWERGOOD)))
//    {
//      /* POWER GOOD KO */
//      return SMPS_KO;
//    }
//  }

  /* SMPS ENABLED */
  return SMPS_OK;
}


uint32_t BSP_SMPS_Disable(void)
{

  PWR_AND_CLK_SMPS();

  /* Check if SMPS SWITCH is disabled */
  if (HAL_GPIO_ReadPin(PORT_SMPS, PIN_SMPS_SWITCH_ENABLE) != GPIO_PIN_RESET)
  {
    /* ERROR AS SWITCH SHOULD BE DISABLED */
    return SMPS_KO;
  }

  /* Disable SMPS */
  //HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_ENABLE, GPIO_PIN_RESET);

  /* SMPS DISABLED */
  return SMPS_OK;
}

uint32_t BSP_SMPS_Supply_Enable(uint32_t Delay, uint32_t Power_Good_Check)
{
  PWR_AND_CLK_SMPS();

  if (Delay != 0)
  {
    vTaskDelay(Delay);
  }
  /* CHECK POWER GOOD or NOT */
  //if (Power_Good_Check != 0)
  //{
  //  if (GPIO_PIN_RESET == (HAL_GPIO_ReadPin(PORT_SMPS, PIN_SMPS_POWERGOOD)))
  //  {
  //    /* POWER GOOD KO */
  //    return SMPS_KO;
  //  }
  //}

  /* SMPS SWITCH ENABLE */
  HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_SWITCH_ENABLE, GPIO_PIN_SET);


  return SMPS_OK;
}

uint32_t BSP_SMPS_Supply_Disable(void)
{
  PWR_AND_CLK_SMPS();

  /* SMPS SWITCH DISABLE */
  HAL_GPIO_WritePin(PORT_SMPS, PIN_SMPS_SWITCH_ENABLE, GPIO_PIN_RESET);

  return SMPS_OK;
}
