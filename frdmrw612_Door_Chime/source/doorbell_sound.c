/*
 *  Copyright 2023 NXP
 *  All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "board.h"
#include "fsl_dma.h"
#include "fsl_i2c.h"
#include "fsl_i2s.h"
#include "fsl_i2s_dma.h"
#include "doorbell.h"

#include <stdbool.h>
#include "doorbell_sound.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DEMO_I2S_MASTER_CLOCK_FREQUENCY CLOCK_GetMclkClkFreq()
#ifndef DEMO_AUDIO_BIT_WIDTH
#define DEMO_AUDIO_BIT_WIDTH            (24)
#endif
#define DEMO_AUDIO_SAMPLE_RATE          (48000)
#define DEMO_AUDIO_PROTOCOL             kCODEC_BusI2S
#define DEMO_I2S_TX                     (I2S1)
#define DEMO_DMA                        (DMA1)
#define DEMO_I2S_TX_CHANNEL             (3)
#define DEMO_I2S_CLOCK_DIVIDER          (DEMO_I2S_MASTER_CLOCK_FREQUENCY / DEMO_AUDIO_SAMPLE_RATE / DEMO_AUDIO_BIT_WIDTH / 2U)
#define DEMO_I2S_TX_MODE                kI2S_MasterSlaveNormalMaster
#ifndef DEMO_CODEC_VOLUME
#define DEMO_CODEC_VOLUME 30U
#endif
#if (DEMO_AUDIO_BIT_WIDTH == 16)
#define DEMO_CODEC_BIT_WIDTH (kWM8904_BitWidth16)
#elif (DEMO_AUDIO_BIT_WIDTH == 24)
#define DEMO_CODEC_BIT_WIDTH (kWM8904_BitWidth24)
#elif (DEMO_AUDIO_BIT_WIDTH == 32)
#define DEMO_CODEC_BIT_WIDTH (kWM8904_BitWidth32)
#else
#error Unsupported audio bit width
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

static dma_handle_t s_DmaTxHandle;
static i2s_config_t s_TxConfig;
static i2s_dma_handle_t s_TxHandle;
static i2s_transfer_t s_TxTransfer;
static int period_count = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*******************************************************************************
 * Static functions
 ******************************************************************************/
static void i2c_release_bus_delay(void)
{
    uint32_t i = 0;
    for (i = 0; i < 100; i++)
    {
        __NOP();
    }
}

void BOARD_I2C_ReleaseBus(void)
{
    uint8_t i = 0;

    GPIO_PortInit(GPIO, BOARD_CODEC_I2C_SDA_PORT);
    GPIO_PortInit(GPIO, BOARD_CODEC_I2C_SCL_PORT);

    BOARD_InitI2CPinsAsGPIO();

    /* Drive SDA low first to simulate a start */
    GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SDA_PORT, BOARD_CODEC_I2C_SDA_PIN, 0U);
    i2c_release_bus_delay();

    /* Send 9 pulses on SCL */
    for (i = 0; i < 9; i++)
    {
        GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SCL_PORT, BOARD_CODEC_I2C_SCL_PIN, 0U);
        i2c_release_bus_delay();

        GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SDA_PORT, BOARD_CODEC_I2C_SDA_PIN, 1U);
        i2c_release_bus_delay();

        GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SCL_PORT, BOARD_CODEC_I2C_SCL_PIN, 1U);
        i2c_release_bus_delay();
        i2c_release_bus_delay();
    }

    /* Send stop */
    GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SCL_PORT, BOARD_CODEC_I2C_SCL_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SDA_PORT, BOARD_CODEC_I2C_SDA_PIN, 0U);
    i2c_release_bus_delay();

    GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SCL_PORT, BOARD_CODEC_I2C_SCL_PIN, 1U);
    i2c_release_bus_delay();

    GPIO_PinWrite(GPIO, BOARD_CODEC_I2C_SDA_PORT, BOARD_CODEC_I2C_SDA_PIN, 1U);
    i2c_release_bus_delay();
}

static void TxCallback(I2S_Type *base, i2s_dma_handle_t *handle, status_t completionStatus, void *userData)
{

}

/*******************************************************************************
 * Interface functions
 ******************************************************************************/
/*!
 * @brief Main function
 */
int DoorbellSound_Init(void)
{
    BOARD_I2C_ReleaseBus();
    BOARD_InitI2CPins();

    CLOCK_EnableClock(kCLOCK_InputMux);

    /* attach SFRO clock to I2C2 */
    CLOCK_AttachClk(kSFRO_to_FLEXCOMM2);

    /* attach AUDIO PLL clock to FLEXCOMM1 (I2S1) */
    CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM1);

    /* attach AUDIO PLL clock to MCLK */
    CLOCK_AttachClk(kAUDIO_PLL_to_MCLK_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivMclkClk, 1);
    SYSCTL1->MCLKPINDIR = SYSCTL1_MCLKPINDIR_MCLKPINDIR_MASK;


    PRINTF("Configure I2S\r\n");

    /*
     * masterSlave = kI2S_MasterSlaveNormalMaster;
     * mode = kI2S_ModeI2sClassic;
     * rightLow = false;
     * leftJust = false;
     * pdmData = false;
     * sckPol = false;
     * wsPol = false;
     * divider = 1;
     * oneChannel = false;
     * dataLength = 16;
     * frameLength = 32;
     * position = 0;
     * watermark = 4;
     * txEmptyZero = true;
     * pack48 = false;
     */
    I2S_TxGetDefaultConfig(&s_TxConfig);
    s_TxConfig.divider     = DEMO_I2S_CLOCK_DIVIDER;
    s_TxConfig.masterSlave = DEMO_I2S_TX_MODE;
    s_TxConfig.dataLength = DEMO_AUDIO_BIT_WIDTH;
    s_TxConfig.frameLength = DEMO_AUDIO_BIT_WIDTH*2;

    I2S_TxInit(DEMO_I2S_TX, &s_TxConfig);

    DMA_Init(DEMO_DMA);

    DMA_EnableChannel(DEMO_DMA, DEMO_I2S_TX_CHANNEL);
    DMA_SetChannelPriority(DEMO_DMA, DEMO_I2S_TX_CHANNEL, kDMA_ChannelPriority3);
    DMA_CreateHandle(&s_DmaTxHandle, DEMO_DMA, DEMO_I2S_TX_CHANNEL);
    s_TxTransfer.data     = &g_doorbell[0];
    s_TxTransfer.dataSize = sizeof(g_doorbell);

    I2S_TxTransferCreateHandleDMA(DEMO_I2S_TX, &s_TxHandle, &s_DmaTxHandle, TxCallback, (void *)&s_TxTransfer);
}

void DoorbellSound_Play(void)
{
    PRINTF("Play\r\n");
    I2S_TxTransferSendDMA(DEMO_I2S_TX, &s_TxHandle, s_TxTransfer);
}

