/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2023 NXP
 *
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "lwip/tcpip.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "wpl.h"
#include "timers.h"

#include "fsl_debug_console.h"
#include "mqtt_freertos.h"

#include <stdio.h>

#include "FreeRTOS.h"

#include "fsl_power.h"

#include "doorbell_sound.h"

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#ifndef AP_SSID
#define AP_SSID "piap"
#endif

#ifndef AP_PASSWORD
#define AP_PASSWORD "austin00"
#endif

#define WIFI_NETWORK_LABEL "my_wifi"

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/* Link lost callback */
static void LinkStatusChangeCallback(bool linkState)
{
    if (linkState == false)
    {
        /* -------- LINK LOST -------- */
        /* DO SOMETHING */
        PRINTF("-------- LINK LOST --------\r\n");
    }
    else
    {
        /* -------- LINK REESTABLISHED -------- */
        /* DO SOMETHING */
        PRINTF("-------- LINK REESTABLISHED --------\r\n");
    }
}

/* Connect to the external AP */
static void ConnectTo()
{
    int32_t result;

    /* Add Wi-Fi network */
    result = WPL_AddNetwork(AP_SSID, AP_PASSWORD, WIFI_NETWORK_LABEL);
    if (result == WPLRET_SUCCESS)
    {
    	do {
            PRINTF("[i] Connecting as client to ssid: %s with password %s\r\n", AP_SSID, AP_PASSWORD);
            result = WPL_Join(WIFI_NETWORK_LABEL);
    		if (result != WPLRET_SUCCESS)
    		{
    			PRINTF("[!] Failed to connect to Wi-Fi - ssid: %s passphrase: %s\r\n", AP_SSID, AP_PASSWORD);
    			vTaskDelay(5000);
    		}
    		else
    		{
    			PRINTF("[i] Connected to Wi-Fi - ssid: %s passphrase: %s\r\n", AP_SSID, AP_PASSWORD);
    			char ip[16];
    			WPL_GetIP(ip, 1);
    		}
    	}while(result != WPLRET_SUCCESS);
    }
    else {
    	PRINTF("[!] Failed to add network to WPL - ssid: %s passphrase: %s\r\n", AP_SSID, AP_PASSWORD);
    }
}

/*!
 * @brief The main task function
 */
static void main_task(void *arg)
{
    uint32_t result = 0;

    PRINTF("\r\n************************************************\r\n");
    PRINTF(" MQTT Door chime example\r\n");
    PRINTF("************************************************\r\n");

    /* Initialize Wi-Fi board */
    PRINTF("[i] Initializing Wi-Fi connection... \r\n");

    result = WPL_Init();
    if (result != WPLRET_SUCCESS)
    {
        PRINTF("[!] WPL Init failed: %d\r\n", (uint32_t)result);
        __BKPT(0);
    }

    result = WPL_Start(LinkStatusChangeCallback);
    if (result != WPLRET_SUCCESS)
    {
        PRINTF("[!] WPL Start failed %d\r\n", (uint32_t)result);
        __BKPT(0);
    }

    PRINTF("[i] Successfully initialized Wi-Fi module\r\n");

    ConnectTo();
    /// wait_dns

    mqtt_freertos_run_thread(netif_default);

    vTaskDelete(NULL);
}



/*!
 * @brief Main function.
 */
int main(void)
{
    /* Initialize the hardware */
    BOARD_InitBootPins();
    if (BOARD_IS_XIP())
    {
    	BOARD_InitBootClocks();
		POWER_DisableGDetVSensors();
        CLOCK_InitT3RefClk(kCLOCK_T3MciIrc48m);
        CLOCK_EnableClock(kCLOCK_T3PllMci256mClk);
		POWER_EnableGDetVSensors();
        CLOCK_EnableClock(kCLOCK_Otp);
        CLOCK_EnableClock(kCLOCK_Els);
        CLOCK_EnableClock(kCLOCK_ElsApb);
        RESET_PeripheralReset(kOTP_RST_SHIFT_RSTn);
        RESET_PeripheralReset(kELS_APB_RST_SHIFT_RSTn);
    }
    else
    {
        BOARD_InitBootClocks();

        CLOCK_EnableClock(kCLOCK_Flexspi);
        RESET_ClearPeripheralReset(kFLEXSPI_RST_SHIFT_RSTn);
        /* Use aux0_pll_clk / 2 */
        BOARD_SetFlexspiClock(FLEXSPI, 2U, 2U);
    }
    BOARD_InitDebugConsole();
    /* Init the doorbell sound engine */
    DoorbellSound_Init();
    /* Reset GMDA */
    RESET_PeripheralReset(kGDMA_RST_SHIFT_RSTn);
    /* Keep CAU sleep clock here. */
    /* CPU1 uses Internal clock when in low power mode. */
    POWER_ConfigCauInSleep(false);
    BOARD_InitSleepPinConfig();

    /* Create the main Task */
    if (xTaskCreate(main_task, "main_task", 1024, NULL, configMAX_PRIORITIES - 4, NULL) != pdPASS)
    {
        PRINTF("[!] MAIN Task creation failed!\r\n");
        while (1)
            ;
    }

    /* Run RTOS */
    vTaskStartScheduler();

    /* Should not reach this statement */
    for (;;)
        ;
}
