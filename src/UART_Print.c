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
 *           SINGLE Lpuart_Uart_Ip_SyncSend() call, not several small ones.
 *           That call enables the LPUART transmitter, sends, then disables
 *           it again, every time it's invoked - observed in practice to
 *           corrupt the first several bytes of whatever it sends (a settling
 *           glitch right after enabling TX), while the rest of the same call
 *           comes through clean. Splitting one line across multiple calls
 *           multiplied how often that glitch window occurred. Consolidating
 *           to one call per line, plus a short throwaway prefix
 *           (UART_PRINT_SYNC_PREFIX) ahead of the real text, means the glitch
 *           - if it happens - only ever eats disposable padding, not the
 *           actual message.
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
#define UART_PRINT_LINE_MAX    (128U)                             /* big enough for the longest label used today, with margin */

/* Sacrificial padding sent ahead of every real line - see file @details. Plain CRLFs so even if
   partially garbled, nothing meaningful is lost and no stray visible characters land mid-line. */
#define UART_PRINT_SYNC_PREFIX  "\r\n\r\n\r\n\r\n\r\n\r\n"

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
	char     line[UART_PRINT_LINE_MAX];
	uint32_t offset = 0U;

	offset = UART_Print_AppendStr(line, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(line, offset, str);

	UART_Print_RawSend(line, offset);
}

/*!
 * @brief       Prints "<label>: 0x<hex> (OK)" or "... (FAIL)" based on HSE_SRV_RSP_OK, then CRLF -
 *              as a single UART transaction (see file @details for why that matters).
 */
void UART_Print_HseResponse(const char *label, hseSrvResponse_t response)
{
	char     line[UART_PRINT_LINE_MAX];
	char     hex[9];
	uint32_t offset = 0U;

	UART_Print_Hex32((uint32_t)response, hex);

	offset = UART_Print_AppendStr(line, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(line, offset, label);
	offset = UART_Print_AppendStr(line, offset, ": 0x");
	offset = UART_Print_AppendStr(line, offset, hex);
	offset = UART_Print_AppendStr(line, offset, (HSE_SRV_RSP_OK == response) ? " (OK)\r\n" : " (FAIL)\r\n");

	UART_Print_RawSend(line, offset);
}

/*!
 * @brief       Same shape as UART_Print_HseResponse() but for any other 32-bit status/enum value.
 */
void UART_Print_Status(const char *label, uint32_t status, uint32_t okValue)
{
	char     line[UART_PRINT_LINE_MAX];
	char     hex[9];
	uint32_t offset = 0U;

	UART_Print_Hex32(status, hex);

	offset = UART_Print_AppendStr(line, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(line, offset, label);
	offset = UART_Print_AppendStr(line, offset, ": 0x");
	offset = UART_Print_AppendStr(line, offset, hex);
	offset = UART_Print_AppendStr(line, offset, (status == okValue) ? " (OK)\r\n" : " (FAIL)\r\n");

	UART_Print_RawSend(line, offset);
}

/*!
 * @brief       Prints "<label>: true" or "<label>: false", followed by CRLF, as a single UART
 *              transaction.
 */
void UART_Print_Bool(const char *label, bool value)
{
	char     line[UART_PRINT_LINE_MAX];
	uint32_t offset = 0U;

	offset = UART_Print_AppendStr(line, offset, UART_PRINT_SYNC_PREFIX);
	offset = UART_Print_AppendStr(line, offset, label);
	offset = UART_Print_AppendStr(line, offset, value ? ": true\r\n" : ": false\r\n");

	UART_Print_RawSend(line, offset);
}

#ifdef __cplusplus
}
#endif

/** @} */
