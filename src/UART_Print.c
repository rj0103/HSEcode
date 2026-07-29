/**
 ******************************************************************************
 * @file     UART_Print.c
 * @brief    Minimal blocking UART debug print helper (LPUART1, PTA18/PTA19,
 *           ~115200 8N1 - see generate/src/Lpuart_Uart_Ip_Sa_PBcfg.c), so HSE
 *           response variables can be observed live over serial instead of
 *           only via debugger.
 * @details  No sprintf/printf here on purpose - this project links the
 *           newlib-nano "noio" C library variant, which doesn't reliably
 *           support them. Numbers are formatted by hand (hex only, 8 digits).
 *
 *           Each logical line is assembled into ONE buffer and sent via a
 *           SINGLE Lpuart_Uart_Ip_SyncSend() call, not several small ones -
 *           that call enables the LPUART transmitter, sends, then disables it
 *           again every time it's invoked, and splitting one line across
 *           several calls multiplied how often any transmit glitch occurred.
 *           A throwaway CRLF prefix (UART_PRINT_SYNC_PREFIX) is also sent
 *           ahead of the real text on every line, so that if a glitch still
 *           happens, it eats disposable padding rather than the message.
 *
 *           The line buffer itself is a static module-level buffer, not a
 *           per-call stack-local one - see UART_Print_LineBuf's own comment
 *           for why (this project's DTCM stack is small, and a large local
 *           buffer reused right after deeply-nested HSE calls is exactly the
 *           kind of thing that can pick up corrupted stack content).
 * @location /test/src/UART_Print.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifdef __cplusplus
extern "C"
{
#endif

#include "UART_Print.h"
#include "Lpuart_Uart_Ip.h"
#include "string.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
#define UART_PRINT_INSTANCE    (LPUART_UART_IP_INSTANCE_USING_1)  /* LPUART1, PTA18(TX)/PTA19(RX) */
#define UART_PRINT_TIMEOUT_US  (100000U)                          /* 100ms - generous for a short debug line */
#define UART_PRINT_LINE_MAX    (192U)                             /* big enough for the longest label + the longer sync prefix below, with margin */

/* Sacrificial padding sent ahead of every real line - see file @details. Plain CRLFs so even if
   partially garbled, nothing meaningful is lost and no stray visible characters land mid-line.
   Confirmed via a real capture that a 6x-CRLF (12 byte) prefix wasn't quite long enough - some of
   the real text's leading edge still landed inside the glitch window. Widened with margin. */
#define UART_PRINT_SYNC_PREFIX  "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"

/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/
/* Static, not stack-local: this project's DTCM stack is only 4KB
   (linker_flash_s32k312_Release.ld), and HSE_Init() calls a long chain of HSE service functions
   before any of these print functions run for the first time after it. A 192-byte local buffer
   reused across deeply-nested calls is exactly the kind of thing that can pick up leftover/
   corrupted stack content from whatever ran immediately before it. Making it static removes that
   risk entirely - safe here since every call is sequential/blocking, never concurrent. */
static char UART_Print_LineBuf[UART_PRINT_LINE_MAX];

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
static void     UART_Print_Hex32(uint32_t value, char pOutHex[9]);
static uint32_t UART_Print_AppendStr(char *pBuf, uint32_t offset, const char *pSrc);
static void     UART_Print_RawSend(const char *pBuf, uint32_t length);

/*!
 * @brief       Formats value as 8 uppercase hex digits (no "0x" prefix) into pOutHex, which must
 *              be at least 9 bytes (8 digits + NUL).
 */
static void UART_Print_Hex32(uint32_t value, char pOutHex[9])
{
	static const char hexDigits[16] = "0123456789ABCDEF";
	uint8_t i;

	for (i = 0U; i < 8U; i++)
	{
		pOutHex[i] = hexDigits[(value >> (28U - (4U * i))) & 0xFU];
	}
	pOutHex[8] = '\0';
}

/*!
 * @brief       Appends pSrc onto pBuf starting at offset, stopping short of UART_PRINT_LINE_MAX
 *              (leaving room for a NUL this function does not itself write - callers that need a
 *              NUL-terminated result should not rely on this leaving one in place of real data).
 *
 * @return      The new offset (index of the next free byte in pBuf).
 */
static uint32_t UART_Print_AppendStr(char *pBuf, uint32_t offset, const char *pSrc)
{
	while (('\0' != *pSrc) && (offset < (UART_PRINT_LINE_MAX - 1U)))
	{
		pBuf[offset] = *pSrc;
		offset++;
		pSrc++;
	}

	return offset;
}

/*!
 * @brief       The one place that actually calls into the LPUART driver - a single blocking send
 *              of exactly length bytes from pBuf.
 */
static void UART_Print_RawSend(const char *pBuf, uint32_t length)
{
	(void)Lpuart_Uart_Ip_SyncSend(UART_PRINT_INSTANCE, (const uint8_t *)pBuf, length, UART_PRINT_TIMEOUT_US);
}

/*!
 * @brief       Initializes LPUART1 for blocking debug prints.
 */
void UART_Print_Init(void)
{
	Lpuart_Uart_Ip_Init(UART_PRINT_INSTANCE, &Lpuart_Uart_Ip_xHwConfigPB_1);
}

/*!
 * @brief       Sends a plain, null-terminated string (prefixed with UART_PRINT_SYNC_PREFIX),
 *              blocking until done (or timeout), as a single UART transaction.
 */
void UART_Print_String(const char *str)
{
	uint32_t offset = 0U;

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, str);

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}

/*!
 * @brief       Prints "<label>: 0x<hex> (OK)" or "... (FAIL)" based on HSE_SRV_RSP_OK, then CRLF -
 *              as a single UART transaction (see file @details for why that matters).
 */
void UART_Print_HseResponse(const char *label, hseSrvResponse_t response)
{
	char     hex[9];
	uint32_t offset = 0U;

	UART_Print_Hex32((uint32_t)response, hex);

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, label);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, ": 0x");
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, hex);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, (HSE_SRV_RSP_OK == response) ? " (OK)\r\n" : " (FAIL)\r\n");

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}

/*!
 * @brief       Same shape as UART_Print_HseResponse() but for any other 32-bit status/enum value.
 */
void UART_Print_Status(const char *label, uint32_t status, uint32_t okValue)
{
	char     hex[9];
	uint32_t offset = 0U;

	UART_Print_Hex32(status, hex);

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, label);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, ": 0x");
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, hex);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, (status == okValue) ? " (OK)\r\n" : " (FAIL)\r\n");

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}

/*!
 * @brief       Prints "<label>: true" or "<label>: false", followed by CRLF, as a single UART
 *              transaction.
 */
void UART_Print_Bool(const char *label, bool value)
{
	uint32_t offset = 0U;

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, label);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, value ? ": true\r\n" : ": false\r\n");

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}











///////////////////////////////////////////////////////////////////////////////////////////////////
/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           : FLEXIO_UART
*   Dependencies         :
*
*   Autosar Version      : 4.9.0
*   Autosar Revision     : ASR_REL_4_9_REV_0000
*   Autosar Conf.Variant :
*   SW Version           : 7.0.1
*   Build Version        : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
*   Copyright 2020 - 2026 NXP
*
*   NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

/* User includes */
#include "Lpuart_Uart_Ip.h"
#include "Flexio_Uart_Ip.h"
#include "Lpuart_Uart_Ip_Irq.h"
#include "Flexio_Uart_Ip_Irq.h"
#include "Flexio_Mcl_Ip.h"
#include "Clock_Ip.h"
#include "IntCtrl_Ip.h"
#include "Siul2_Port_Ip.h"
#include "check_example.h"
#include "string.h"

/* Welcome messages displayed at the console */
#define WELCOME_MSG_1 "Hello, This message is sent via Uart!\r\n"
#define WELCOME_MSG_2 "Have a nice day!\r\n"

/* Error message displayed at the console, in case data is received erroneously */
#define ERROR_MSG "An error occurred! The application will stop!\r\n"

/* Length of the message to be received from the console */
#define MSG_LEN  50U

/* Define channel */
#define UART_LPUART_INTERNAL_CHANNEL  3
#define UART_FLEXIO_TX_CHANNEL  0
#define UART_FLEXIO_RX_CHANNEL  1

volatile int exit_code = 0;


/* Define enum which module is receiver */
typedef enum
{
    LPUART_RECEIVER = 0,
    FLEXIO_RECEIVER = 1

} Receiver_Module_Type;

boolean User_Str_Cmp(const uint8 * pBuffer1, const uint8 * pBuffer2, const uint32 length)
{
    uint32 idx = 0;
    for (idx = 0; idx < length; idx++)
    {
        if(pBuffer1[idx] != pBuffer2[idx])
        {
            return FALSE;
        }
    }
    return TRUE;
}

boolean Send_Data(Receiver_Module_Type receiver, const uint8* pBuffer, uint32 length)
{
    volatile Lpuart_Uart_Ip_StatusType lpuartStatus = LPUART_UART_IP_STATUS_ERROR;
    volatile Flexio_Uart_Ip_StatusType flexioStatus = FLEXIO_UART_IP_STATUS_ERROR;
    uint32 remainingBytes;
    uint32 T_timeout = 0xFFFFFF;
    uint8 Rx_Buffer[MSG_LEN];

    memset(Rx_Buffer, 0 , MSG_LEN);

    if (receiver == FLEXIO_RECEIVER)
    {
        /* Uart_AsyncReceive transmit data */
        flexioStatus = Flexio_Uart_Ip_AsyncReceive(UART_FLEXIO_RX_CHANNEL, Rx_Buffer, length);
        if (FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus)
        {
            return FALSE;
        }
        /* Uart_AsyncSend transmit data */
        lpuartStatus = Lpuart_Uart_Ip_AsyncSend(UART_LPUART_INTERNAL_CHANNEL, pBuffer, length);
        if (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus)
        {
            return FALSE;
        }

        /* Check for no on-going transmission */
        do
        {
            lpuartStatus = Lpuart_Uart_Ip_GetTransmitStatus(UART_LPUART_INTERNAL_CHANNEL, &remainingBytes);
        } while (LPUART_UART_IP_STATUS_BUSY == lpuartStatus && 0 < T_timeout--);

        T_timeout = 0xFFFFFF;

        do
        {
            flexioStatus = Flexio_Uart_Ip_GetStatus(UART_FLEXIO_RX_CHANNEL, &remainingBytes);
        } while (FLEXIO_UART_IP_STATUS_BUSY == flexioStatus && 0 < T_timeout--);

        if ((FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus) || (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus))
        {
            return FALSE;
        }
    }
    else
    {
        /* Uart_AsyncReceive transmit data */
        lpuartStatus = Lpuart_Uart_Ip_AsyncReceive(UART_LPUART_INTERNAL_CHANNEL, Rx_Buffer, length);
        if (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus)
        {
            return FALSE;
        }
        /* Uart_AsyncSend transmit data */
        flexioStatus = Flexio_Uart_Ip_AsyncSend(UART_FLEXIO_TX_CHANNEL, pBuffer, length);
        if (FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus)
        {
            return FALSE;
        }

        /* Check for no on-going transmission */
        do
        {
            flexioStatus = Flexio_Uart_Ip_GetStatus(UART_FLEXIO_TX_CHANNEL, &remainingBytes);
        } while (FLEXIO_UART_IP_STATUS_BUSY == flexioStatus && 0 < T_timeout--);

        do
        {
            lpuartStatus = Lpuart_Uart_Ip_GetReceiveStatus(UART_LPUART_INTERNAL_CHANNEL, &remainingBytes);
        } while (LPUART_UART_IP_STATUS_BUSY == lpuartStatus && 0 < T_timeout--);

        if ((FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus) || (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus))
        {
            return FALSE;
        }
    }

    if(!User_Str_Cmp(pBuffer, (const uint8 *)Rx_Buffer, length))
    {
        return FALSE;
    }

    return TRUE;
}
/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
    /* Write your code here */
    volatile boolean isSendMsg1Success = FALSE;
    volatile boolean isSendMsg2Success = FALSE;

    /* Init clock  */
    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);

#if defined (FEATURE_CLOCK_IP_HAS_SPLL_CLK)
    while ( CLOCK_IP_PLL_LOCKED != Clock_Ip_GetPllStatus() )
    {
        /* Busy wait until the System PLL is locked */
    }

    Clock_Ip_DistributePll();
#endif
    /* Initialize all pins */
    Siul2_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0, g_pin_mux_InitConfigArr_PortContainer_0_VS_0);

    /* Init Flexio common Mcl  */
    Flexio_Mcl_Ip_InitDevice(&Flexio_Ip_Sa_xFlexioInit_VS_0);

    /* Initialize IRQs */
    IntCtrl_Ip_Init(&IntCtrlConfig_0);
    //IntCtrl_Ip_ConfigIrqRouting(&intRouteConfig);
    /* Initializes an UART driver*/
    Lpuart_Uart_Ip_Init(UART_LPUART_INTERNAL_CHANNEL, &Lpuart_Uart_Ip_xHwConfigPB_3_VS_0);
    Flexio_Uart_Ip_Init(UART_FLEXIO_TX_CHANNEL, &Flexio_Uart_Ip_xHwConfigPB_0_VS_0);
    Flexio_Uart_Ip_Init(UART_FLEXIO_RX_CHANNEL, &Flexio_Uart_Ip_xHwConfigPB_1_VS_0);

    isSendMsg1Success = Send_Data(FLEXIO_RECEIVER, (const uint8 *)WELCOME_MSG_1, strlen(WELCOME_MSG_1));

    isSendMsg2Success = Send_Data(LPUART_RECEIVER, (const uint8 *)WELCOME_MSG_2, strlen(WELCOME_MSG_2));

    (void)Lpuart_Uart_Ip_Deinit(UART_LPUART_INTERNAL_CHANNEL);

    (void)Flexio_Uart_Ip_Deinit(UART_FLEXIO_RX_CHANNEL);
    (void)Flexio_Uart_Ip_Deinit(UART_FLEXIO_TX_CHANNEL);

    /* Deinit Flexio common Mcl  */
    Flexio_Mcl_Ip_DeinitDevice(&Flexio_Ip_Sa_xFlexioInit_VS_0);

    Exit_Example((isSendMsg1Success == TRUE) && (isSendMsg2Success == TRUE));
    return exit_code;
}

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
