/**
 ******************************************************************************
 * @file     UART_Print.c
 * @brief    Minimal blocking UART debug print helper (LPUART1, PTA18/PTA19,
 *           ~115200 8N1 - see generate/src/Lpuart_Uart_Ip_Sa_PBcfg.c), so HSE
 *           response variables can be observed live over serial instead of
 *           only via debugger.
 * @details  No sprintf/printf here on purpose - this project links the
 *           newlib-nano "noio" C library variant, which doesn't reliably
 *           support them. Numbers are formatted by hand (hex only, 8 digits),
 *           and lines are assembled by sending multiple short strings in
 *           sequence rather than building one buffer.
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

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
static void UART_Print_Hex32(uint32_t value, char pOutHex[9]);

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
 * @brief       Initializes LPUART1 for blocking debug prints.
 */
void UART_Print_Init(void)
{
	Lpuart_Uart_Ip_Init(UART_PRINT_INSTANCE, &Lpuart_Uart_Ip_xHwConfigPB_1);
}

/*!
 * @brief       Sends a plain, null-terminated string, blocking until done (or timeout).
 */
void UART_Print_String(const char *str)
{
	uint32_t length = (uint32_t)strlen(str);

	(void)Lpuart_Uart_Ip_SyncSend(UART_PRINT_INSTANCE, (const uint8_t *)str, length, UART_PRINT_TIMEOUT_US);
}

/*!
 * @brief       Prints "<label>: 0x<hex> (OK)" or "... (FAIL)" based on HSE_SRV_RSP_OK, then CRLF.
 */
void UART_Print_HseResponse(const char *label, hseSrvResponse_t response)
{
	char hex[9];

	UART_Print_Hex32((uint32_t)response, hex);

	UART_Print_String(label);
	UART_Print_String(": 0x");
	UART_Print_String(hex);
	UART_Print_String((HSE_SRV_RSP_OK == response) ? " (OK)\r\n" : " (FAIL)\r\n");
}

/*!
 * @brief       Same shape as UART_Print_HseResponse() but for any other 32-bit status/enum value.
 */
void UART_Print_Status(const char *label, uint32_t status, uint32_t okValue)
{
	char hex[9];

	UART_Print_Hex32(status, hex);

	UART_Print_String(label);
	UART_Print_String(": 0x");
	UART_Print_String(hex);
	UART_Print_String((status == okValue) ? " (OK)\r\n" : " (FAIL)\r\n");
}

/*!
 * @brief       Prints "<label>: true" or "<label>: false", followed by CRLF.
 */
void UART_Print_Bool(const char *label, bool value)
{
	UART_Print_String(label);
	UART_Print_String(value ? ": true\r\n" : ": false\r\n");
}

#ifdef __cplusplus
}
#endif

/** @} */
