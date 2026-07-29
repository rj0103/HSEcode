/**
 ******************************************************************************
 * @file     UART_Print.h
 * @brief    Minimal blocking UART debug print helper (LPUART1, PTA18/PTA19,
 *           ~115200 8N1 - see generate/src/Lpuart_Uart_Ip_Sa_PBcfg.c), so HSE
 *           response variables can be observed live over serial instead of
 *           only via debugger.
 * @location /test/src/UART_Print.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifndef __UART_PRINT_H__
#define __UART_PRINT_H__

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
==================================================================================================*/
#include "HSE_Main.h"

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*!
 * @brief   Initializes LPUART1 for blocking debug prints. Call once, after LED_Init() (which is
 *          what actually configures the PTA18/PTA19 pin muxing for this UART - see
 *          board/Siul2_Port_Ip_Cfg.c).
 */
void UART_Print_Init(void);

/*!
 * @brief   Sends a plain, null-terminated string, blocking until done (or timeout).
 */
void UART_Print_String(const char *str);

/*!
 * @brief   Prints "<label>: 0x<8-digit hex> (OK)" or "... (FAIL)" depending on whether response
 *          equals HSE_SRV_RSP_OK, followed by CRLF.
 */
void UART_Print_HseResponse(const char *label, hseSrvResponse_t response);

/*!
 * @brief   Same shape as UART_Print_HseResponse() but for any other 32-bit status/enum value
 *          (e.g. C40_Ip_StatusType, HSE_STAT_t) - okValue is whatever counts as success for it.
 */
void UART_Print_Status(const char *label, uint32_t status, uint32_t okValue);

/*!
 * @brief   Prints "<label>: true" or "<label>: false", followed by CRLF.
 */
void UART_Print_Bool(const char *label, bool value);

#ifdef __cplusplus
}
#endif

#endif /* __UART_PRINT_H__ */

/** @} */
