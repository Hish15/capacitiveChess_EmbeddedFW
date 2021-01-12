#include "LS013B7DH03.h"

#include <stm32l4xx_hal.h>
#include <string.h>
#include "fonts.h"
#include <FreeRTOS.h>
#include <task.h>

/** Update pixel data */
#define CMD_DATA_UPDATE (0x01)
/** Clear internal memory */
#define CMD_CLEAR       (0x04)

#define SCREEN_HEIGHT   (128)
#define SCREEN_WIDTH   (128)

#define NB_BIT_PER_BYTE (8)

#define SPI_TIMEOUT_MS (1000)

SPI_HandleTypeDef SpiHandle;

/**
 * @brief set or unset SS (Slave Select) pin
 *
 * @param ss slave select if true, unselect if false
 */
static void LCDslaveSelect(bool ss)
{
    if(ss)
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);
    }
}

/**
 * @brief set DISP pin
 *
 * @param en enable true to display info from memory
 */
static void LCDdisplayEnable(bool en)
{
    if(en)
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_RESET);
    }
}

/**
 * @brief adapt the bit and byte order so thats screen prints everything in expected direction
 * Bits need to be reversed in all bytes for this screen.
 * And everything needs to be rotated because screen is upside down.
 *
 * @param pixelBuf display buffer where one bit is one pixel.
 * @param nbBytePixelAsciiLine length of buffer. This must be multiple of 2
 */
static void reorderBitsToScreen(uint8_t *pixelBuf, uint16_t nbBytePixelAsciiLine)
{
    //TODO this could be optimized doing 4 bytes at a time (with __REV instruction)
    // check length is multiple of 2
    assert_param((nbBytePixelAsciiLine%2) == 0);

    for(uint16_t b=0; b<nbBytePixelAsciiLine/2; b++)
    {
        uint8_t tmp = pixelBuf[b];
        pixelBuf[b] = pixelBuf[nbBytePixelAsciiLine-1-b];
        pixelBuf[nbBytePixelAsciiLine-1-b] = tmp;
    }
}

/**
 * @brief update the required lines in the screen
 * Update must be done at line boundary
 *
 * @param screenLine the line number (1 to 128) of first line to update
 * @param pixelBuf   one or several lines of pixel data
 * @param nbBytes    number of bytes to update (must be on complete line boundary)
 * @return true if everything was fine
 */
static bool LCDupdateDisplay(uint8_t screenLine, uint8_t *pixelBuf, uint16_t nbBytes)
// TODO add const like => static bool LCDupdateDisplay(uint8_t screenLine, const uint8_t *pixelBuf, uint16_t nbBytes)
// when ST will finally change the HAL_SPI_Transmit API to const
{
    bool returnSuccess = true;
    uint8_t nbScreenLines = (nbBytes*NB_BIT_PER_BYTE)/SCREEN_WIDTH;

    // check the data fits into screen and ends on line boundary
    bool lineFitVertically = screenLine>0 && (screenLine+nbScreenLines-1)<=SCREEN_HEIGHT;
    bool lineFitHorizontally = (nbBytes%(SCREEN_WIDTH/NB_BIT_PER_BYTE))==0;
    if(lineFitVertically && lineFitHorizontally)
    {
        reorderBitsToScreen(pixelBuf, nbBytes);
        LCDslaveSelect(true);
        vTaskDelay(1); //TODO this delay could be much less, but need a delay_us function
        uint8_t cmd_buffer[2] = {CMD_DATA_UPDATE, 0x00};
        for(uint16_t i=0; i<nbScreenLines ; i++)
        {
            // screen is mounted upside down so we need to calculate the actual line number correcponding to the one requested by user
            uint8_t lineNumber = (((SCREEN_HEIGHT-screenLine)+1)-nbScreenLines)+i;
            cmd_buffer[1] = lineNumber;
            // send "data update" command to correct line
            if(HAL_SPI_Transmit(&SpiHandle, cmd_buffer, sizeof(cmd_buffer), SPI_TIMEOUT_MS) != HAL_OK)
            {
                returnSuccess = false;
            }
            // send one line of data from buffer
            if(HAL_SPI_Transmit(&SpiHandle, pixelBuf+(i*SCREEN_WIDTH/NB_BIT_PER_BYTE), SCREEN_WIDTH/NB_BIT_PER_BYTE, SPI_TIMEOUT_MS) != HAL_OK)
            {
                returnSuccess= false;
            }
        }
        // send 16 dummy bits
        if(HAL_SPI_Transmit(&SpiHandle, cmd_buffer, sizeof(cmd_buffer), SPI_TIMEOUT_MS) != HAL_OK)
        {
            returnSuccess= false;
        }
        vTaskDelay(1); //TODO this delay could be much less, but need a delay_us function
        LCDslaveSelect(false);
    }
    else
    {
        returnSuccess = false;
    }
    return returnSuccess;
}

bool sharpMemoryLCD_init(void)
{
    bool returnSuccess= true;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    /* Pin B0 is EXTCOMIN driven by TIM3_CH1 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    /* Pin D1 is SPI2 CLK */
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    /* Pin D4 is SPI2 MOSI */
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    /* Pin D0 is SPI2 NSS */
    /* It is not driven by SPI2 peripheral because need to be enabled with high level */
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    LCDslaveSelect(false);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    /* Pin D6 is DISP (for enabling display) */
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    LCDdisplayEnable(false);


    /* Output a 60kHz signal to EXTCOMIN pin */
    TIM_HandleTypeDef TimHandle;
    TimHandle.Instance = TIM3;
    TimHandle.Init.Period        = 65535;
    TimHandle.Init.Prescaler     = 60000; // counter is counting at 120M/60k => 2kHz
    TimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; // 1=2kHZ 4=500Hz => TODO this statement has no effect on frequency.
    TimHandle.Init.CounterMode   = TIM_COUNTERMODE_UP;
    TimHandle.Init.Period        = 32; //2000/32 => 60Hz period
    TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&TimHandle) != HAL_OK)
    {
        returnSuccess= false;
    }

    TIM_OC_InitTypeDef sConfig;
    sConfig.OCMode       = TIM_OCMODE_PWM1;
    sConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    sConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfig.Pulse = 1;
    if (HAL_TIM_PWM_ConfigChannel(&TimHandle, &sConfig, TIM_CHANNEL_1) != HAL_OK)
    {
        returnSuccess= false;
    }

    if (HAL_TIM_PWM_Start(&TimHandle, TIM_CHANNEL_1) != HAL_OK)
    {
        returnSuccess= false;
    }

    SpiHandle.Instance               = SPI2;
    SpiHandle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128; // ===> TODO set correct baudrate to 1Mbps
    SpiHandle.Init.Direction         = SPI_DIRECTION_1LINE;
    SpiHandle.Init.CLKPhase          = SPI_PHASE_1EDGE;
    SpiHandle.Init.CLKPolarity       = SPI_POLARITY_LOW;
    SpiHandle.Init.DataSize          = SPI_DATASIZE_8BIT;
    SpiHandle.Init.FirstBit          = SPI_FIRSTBIT_LSB;
    SpiHandle.Init.TIMode            = SPI_TIMODE_DISABLE;
    SpiHandle.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    SpiHandle.Init.NSS               = SPI_NSS_SOFT;
    SpiHandle.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    SpiHandle.Init.Mode              = SPI_MODE_MASTER;
    if(HAL_SPI_Init(&SpiHandle) != HAL_OK)
    {
        returnSuccess= false;
    }

    returnSuccess&= sharpMemoryLCD_clearScreen();

    LCDdisplayEnable(true);

    return returnSuccess;
}


bool sharpMemoryLCD_clearScreen(void)
{
    bool returnSuccess= true;
    /* To clear sreen send CMD_CLEAR (3bit) + 16 additional dummy bits */
    uint8_t buffer[] = {CMD_CLEAR, 0x00};
    LCDslaveSelect(true);
    if(HAL_SPI_Transmit(&SpiHandle, buffer, sizeof(buffer), SPI_TIMEOUT_MS) != HAL_OK)
    {
        returnSuccess= false;
    }
    LCDslaveSelect(false);

    return returnSuccess;
}


bool sharpMemoryLCD_printTextLine(uint8_t line, const char *text, uint8_t nbChar)
{
    bool returnSuccess = true;
    uint16_t nbBytePixelAsciiLine = Font16.Height*SCREEN_WIDTH/NB_BIT_PER_BYTE;
    uint8_t pixelBuf[nbBytePixelAsciiLine]  __attribute__((aligned (4)));
    memset(pixelBuf, 0xFF, nbBytePixelAsciiLine);

    if(line < (SCREEN_HEIGHT/Font16.Height))
    {
        uint8_t screenX=0;
        uint16_t XbytesPerFontChar = (Font16.Width+NB_BIT_PER_BYTE-1)/NB_BIT_PER_BYTE;
        uint16_t bytesPerFontChar = XbytesPerFontChar*Font16.Height;
        //end display at first character out of ascii (most common will be \0) or out of screen
        for(uint8_t c=0; (c<nbChar) && (' '<=text[c]) && (text[c]<='~') && (c<(SCREEN_WIDTH/Font16.Width)); c++)
        {
            // here SIMD instructions could be used to process 4 lines in one instruction
            for(uint8_t x=0; x<Font16.Width; x++)
            {
                for(uint8_t y=0; y<Font16.Height; y++)
                {
                    // search for the byte in font table where to find the info of the required pixel
                    // the font table is a 1D array but it could be 3D
                    // 1D is for the current ASCII char in the table
                    // 1D is for the line inside the ASCII char description
                    // 1D is for the byte inside the pixel line of the the ASCII char description
                    uint16_t indexCurrentByteInFont = bytesPerFontChar*(text[c]-' ') + XbytesPerFontChar*y + x/8;
                    // TODO could be optimized to do several pixel at one time to go until byte alignement
                    // we need to update pixels inside a byte.
                    // take the correct bit in the correct byte of font table
                    uint8_t indexCurrentBitInFont = 1<<((NB_BIT_PER_BYTE-1)-(x % NB_BIT_PER_BYTE));
                    bool fontPixelBit = indexCurrentBitInFont & Font16.table[indexCurrentByteInFont];
                    if(fontPixelBit)
                    {
                        // modify the pixel/bit in the correct byte of the buffer
                        uint16_t indexCurrentByteInScreen = (y*SCREEN_WIDTH + screenX)/NB_BIT_PER_BYTE;
                        uint8_t indexCurretnBitInScreen = 1<<((NB_BIT_PER_BYTE-1) - (screenX%NB_BIT_PER_BYTE));
                        pixelBuf[indexCurrentByteInScreen] ^= indexCurretnBitInScreen;
                    }

                }
                screenX++;
            }
        }
        returnSuccess &= LCDupdateDisplay(line*Font16.Height + 1, pixelBuf, sizeof(pixelBuf));
    }
    else
    {
        returnSuccess = false;
    }

    return returnSuccess;
}
