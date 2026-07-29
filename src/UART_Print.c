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
 *           This does NOT use Lpuart_Uart_Ip_SyncSend(). That function enables
 *           the transmitter (CTRL[TE]), sends, and disables the transmitter
 *           again on every single call - and its internal send loop only
 *           waits for TDRE (Tx Data Register Empty, i.e. the last byte has
 *           moved into the shift register) before returning, never for TC
 *           (Transmission Complete, i.e. the last byte has actually finished
 *           shifting out on the wire - see RTD/src/Lpuart_Uart_Ip.c's own
 *           Lpuart_Uart_Ip_TxCompleteIrqHandler(), which is the only place in
 *           that driver that disables TE after genuinely waiting for TC).
 *           Disabling TE while the last bit is still physically shifting out
 *           left the shift register/baud counter in a state that needed a
 *           byte or two to resynchronize on the next call - exactly the
 *           observed pattern (garbled head, clean tail, worse with a longer
 *           message). NXP's own RTD example (FlexIO_UART demo) never has this
 *           problem because it never toggles TE between messages - it enables
 *           the transmitter once and leaves it enabled.
 *
 *           So here the transmitter is enabled exactly once, in
 *           UART_Print_Init(), and never disabled again. Bytes are written
 *           directly via the same inline register-access helpers
 *           RTD/include/Lpuart_Uart_Ip_HwAccess.h exposes to the driver
 *           itself (Lpuart_Uart_Ip_Putchar()/Lpuart_Uart_Ip_GetStatusFlag()),
 *           polling TDRE between bytes - no vendor file was modified, no new
 *           interrupt or S32 Config Tool change was needed.
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
#include "Lpuart_Uart_Ip_HwAccess.h"
#include "string.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
#define UART_PRINT_INSTANCE    (LPUART_UART_IP_INSTANCE_USING_1)  /* LPUART1, PTA18(TX)/PTA19(RX) */
#define UART_PRINT_LINE_MAX    (128U)                             /* big enough for the longest label, with margin */
#define UART_PRINT_TDRE_SPINS  (100000U)                          /* bounded busy-wait per byte on TDRE, so a stuck line can't hang the app forever */

/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/
/* Same base-pointer lookup RTD/src/Lpuart_Uart_Ip.c itself uses internally (Lpuart_Uart_Ip_apBases[]
   is private to that file) - LPUART_IP_BASE_PTRS is the vendor macro both derive from. */
static LPUART_Type * const UART_Print_apBases[] = LPUART_IP_BASE_PTRS;
#define UART_PRINT_BASE  (UART_Print_apBases[UART_PRINT_INSTANCE])

/* Static, not stack-local: this project's DTCM stack is only 4KB
   (linker_flash_s32k312_Release.ld). Safe here since every call is sequential/blocking, never
   concurrent. */
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
 * @brief       The one place that actually writes bytes to the LPUART - a raw, direct register
 *              send (see file @details for why this bypasses Lpuart_Uart_Ip_SyncSend()). The
 *              transmitter itself is enabled once in UART_Print_Init() and never disabled here.
 */
static void UART_Print_RawSend(const char *pBuf, uint32_t length)
{
	uint32_t i;
	uint32_t spins;

	for (i = 0U; i < length; i++)
	{
		spins = 0U;
		while ((FALSE == Lpuart_Uart_Ip_GetStatusFlag(UART_PRINT_BASE, LPUART_UART_IP_TX_DATA_REG_EMPTY)) &&
		       (spins < UART_PRINT_TDRE_SPINS))
		{
			spins++;
		}

		Lpuart_Uart_Ip_Putchar(UART_PRINT_BASE, (uint8_t)pBuf[i]);
	}
}

/*!
 * @brief       Initializes LPUART1 for blocking debug prints and enables its transmitter exactly
 *              once - see file @details for why it is never disabled again after this.
 */
void UART_Print_Init(void)
{
	Lpuart_Uart_Ip_Init(UART_PRINT_INSTANCE, &Lpuart_Uart_Ip_xHwConfigPB_1);
	Lpuart_Uart_Ip_SetTransmitterCmd(UART_PRINT_BASE, TRUE);
}

/*!
 * @brief       Sends a plain, null-terminated string.
 */
void UART_Print_String(const char *str)
{
	uint32_t offset = 0U;

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, str);

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}

/*!
 * @brief       Prints "<label>: 0x<hex> (OK)" or "... (FAIL)" based on HSE_SRV_RSP_OK, then CRLF.
 */
void UART_Print_HseResponse(const char *label, hseSrvResponse_t response)
{
	char     hex[9];
	uint32_t offset = 0U;

	UART_Print_Hex32((uint32_t)response, hex);

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

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, label);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, ": 0x");
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, hex);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, (status == okValue) ? " (OK)\r\n" : " (FAIL)\r\n");

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}

/*!
 * @brief       Prints "<label>: true" or "<label>: false", followed by CRLF.
 */
void UART_Print_Bool(const char *label, bool value)
{
	uint32_t offset = 0U;

	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, label);
	offset = UART_Print_AppendStr(UART_Print_LineBuf, offset, value ? ": true\r\n" : ": false\r\n");

	UART_Print_RawSend(UART_Print_LineBuf, offset);
}

#ifdef __cplusplus
}
#endif

/** @} */
