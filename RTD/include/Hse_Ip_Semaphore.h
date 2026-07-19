/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           : HSE
*   Dependencies         : none
*
*   Autosar Version      : 4.9.0
*   Autosar Revision     : ASR_REL_4_9_REV_0000
*   Autosar Conf.Variant :
*   SW Version           : 7.0.1
*   Build Version        : S32K3_RTD_7_0_1_D2603_ASR_REL_4_9_REV_0000_20260331
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

#ifndef HSE_IP_SEMAPHORE_H
#define HSE_IP_SEMAPHORE_H

/**
*   @file
*
*   @addtogroup HSE_IP
*   @{
*/

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
#include "Hse_Ip_Cfg.h"
#include "StandardTypes.h"
#include "hse_interface.h"
#if (STD_ON == HSE_IP_HARDWARE_SEMA42)
#include "CDD_Rm.h"
#endif /*(STD_ON == HSE_IP_HARDWARE_SEMA42)*/
#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)
#include "OsIf_Software_Semaphore.h"
#endif /*(STD_ON == HSE_IP_SOFTWARE_SEMA42)*/

/*==================================================================================================
*                                 SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define HSE_IP_SEMAPHORE_VENDOR_ID_H                       43
#define HSE_IP_SEMAPHORE_SW_MAJOR_VERSION_H                7
#define HSE_IP_SEMAPHORE_SW_MINOR_VERSION_H                0
#define HSE_IP_SEMAPHORE_SW_PATCH_VERSION_H                1
#define HSE_IP_SEMAPHORE_AR_RELEASE_MAJOR_VERSION_H        4
#define HSE_IP_SEMAPHORE_AR_RELEASE_MINOR_VERSION_H        9

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
/* Check if Hse_Ip_Semaphore header file and Hse_Ip_Cfg header file are of the same vendor */
#if (HSE_IP_SEMAPHORE_VENDOR_ID_H != HSE_IP_CFG_VENDOR_ID_H)
    #error "Hse_Ip_Semaphore.h and Hse_Ip_Cfg.h have different vendor ids"
#endif

/* Check if Hse_Ip_Semaphore header file and Hse_Ip_Cfg header file are of the same Software version */
#if ((HSE_IP_SEMAPHORE_SW_MAJOR_VERSION_H != HSE_IP_CFG_SW_MAJOR_VERSION_H) || \
     (HSE_IP_SEMAPHORE_SW_MINOR_VERSION_H != HSE_IP_CFG_SW_MINOR_VERSION_H) || \
     (HSE_IP_SEMAPHORE_SW_PATCH_VERSION_H != HSE_IP_CFG_SW_PATCH_VERSION_H)    \
    )
    #error "Software Version Numbers of Hse_Ip_Semaphore.h and Hse_Ip_Cfg.h are different"
#endif

#if (STD_ON == HSE_IP_HARDWARE_SEMA42)
/* Check if header file and CDD_Rm.h header file are of the same Autosar version */
#if ((HSE_IP_SEMAPHORE_AR_RELEASE_MAJOR_VERSION_H != RM_AR_RELEASE_MAJOR_VERSION) || \
        (HSE_IP_SEMAPHORE_AR_RELEASE_MINOR_VERSION_H != RM_AR_RELEASE_MINOR_VERSION) \
    )
    #error "AutoSar Version Numbers of Hse_Ip_Semaphore.h and CDD_Rm.h are different"
#endif
#endif

#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
/* Check if the files Hse_Ip.h and StandardTypes.h are of the same AutoSar version */
#if ((HSE_IP_SEMAPHORE_AR_RELEASE_MAJOR_VERSION_H != STD_AR_RELEASE_MAJOR_VERSION) || \
     (HSE_IP_SEMAPHORE_AR_RELEASE_MINOR_VERSION_H != STD_AR_RELEASE_MINOR_VERSION)    \
    )
    #error "AutoSar Version Numbers of Hse_Ip_Semaphore.h and StandardTypes.h are different"
#endif

#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)
/* Check if header file and OsIf_Software_Semaphore.h header file are of the same Autosar version */
#if ((HSE_IP_SEMAPHORE_AR_RELEASE_MAJOR_VERSION_H != OSIF_SOFTWARE_SEMAPHORE_AR_RELEASE_MAJOR_VERSION_H) || \
        (HSE_IP_SEMAPHORE_AR_RELEASE_MINOR_VERSION_H != OSIF_SOFTWARE_SEMAPHORE_AR_RELEASE_MINOR_VERSION_H) \
    )
    #error "AutoSar Version Numbers of Hse_Ip_Semaphore.h and OsIf_Software_Semaphore.h are different"
#endif
#endif

#endif

/*==================================================================================================
*                                            CONSTANTS
==================================================================================================*/
#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)
#define CRYPTO_43_HSE_START_SEC_CONST_32
#include "Crypto_43_HSE_MemMap.h"

extern volatile uint32* const pu32Crypto_Software_Semaphore_Gate;

#define CRYPTO_43_HSE_STOP_SEC_CONST_32
#include "Crypto_43_HSE_MemMap.h"
#endif /* (STD_ON == HSE_IP_SOFTWARE_SEMA42) */


/*==================================================================================================
*                                       DEFINES AND MACROS
==================================================================================================*/

#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)

/*! @brief K3 Devices have 2 MU instances each having 4 channels,for averaging we will keep 4 as value of this variable instead of 8. */
#define HSE_IP_COUNTINGSEMA4_MAXVALUE      (4U)

#endif /* (STD_ON == HSE_IP_SOFTWARE_SEMA42) */

#if ((STD_ON == HSE_IP_HARDWARE_SEMA42) || (STD_ON == HSE_IP_SOFTWARE_SEMA42))

/*! @brief Mask for HSE Service Class */
#define HSE_IP_SERVICE_CLASS_MASK_U32                    ((uint32)0x0000FF00U)

/*! @brief Mask for HSE Service Id */
#define HSE_IP_SERVICE_ID_MASK_U32                       ((uint32)0x000000FFU)

/*! @brief Mask for HSE Administrative Service Class. */
#define HSE_IP_ADMINISTRATIVE_SERVICE_CLASS_MASK         ((uint8)0x00U)

/*! @brief Mask for HSE Key Management Service Class. */
#define HSE_IP_KEY_MANAGEMENT_SERVICE_CLASS_MASK         ((uint8)0x01U)

/*! @brief Mask for HSE Monotonic counters Class. */
#define HSE_IP_KEY_MONOTONIC_COUNTERS_CLASS_MASK         ((uint8)0x04U)

/*! @brief Mask for Secure memory region management Class. */
#define HSE_IP_SECURE_MEM_REGIONS_MGT_CLASS_MASK         ((uint8)0x05U)

/*! @brief Mask for HSE Extended Service Class */
#define HSE_IP_EXTENDED_SERVICE_CLASS_MASK_U8            ((uint8)0xF0U)

#endif /* ((STD_ON == HSE_IP_HARDWARE_SEMA42) || (STD_ON == HSE_IP_SOFTWARE_SEMA42)) */

/*==================================================================================================
*                                              ENUMS
==================================================================================================*/

/*==================================================================================================
*                                  STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/

/*==================================================================================================
*                                  GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/


/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/
#if ((STD_ON == HSE_IP_HARDWARE_SEMA42) || (STD_ON == HSE_IP_SOFTWARE_SEMA42))

#define CRYPTO_43_HSE_START_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

/*!
 * @brief       Checks if HSE service require flash semaphore lock
 *
 * @param[in]   hseSrvId    HSE Service ID
 *
 * @return      TRUE or FALSE
 */
boolean Hse_Ip_Semaphore_IsLockRequired
(
    hseSrvId_t hseSrvId
);

/*!
 * @brief      Release request for Semaphore lock
 *             Driver supports both hardware and software semaphore locks enabled during configuration time.
 *
 * @param[in]  u8MuInstance MU Instance specific to partition configured with driver object requesting to unlock
 * @param[in]  u8MuChannel  MU Channel on hse service request is arrived.
 *
 * @return     TRUE when lock is released FALSE in other case
 */
boolean Hse_Ip_Semaphore_ReleaseLock
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
);

/*!
 * @brief      Lock request for Semaphore
 *             Driver supports both hardware and software semaphore locks enabled during configuration time.
 *
 * @param[in]  u8MuInstance MU Instance specific to partition configured with driver object requesting for lock
 * @param[in]  u8MuChannel  MU Channel on hse service request is arrived.
 *
 * @return     TRUE when lock is acquired FALSE in other case
 */
boolean Hse_Ip_Semaphore_RequestLock
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
);

#define CRYPTO_43_HSE_STOP_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

#endif /* ((STD_ON == HSE_IP_HARDWARE_SEMA42) || (STD_ON == HSE_IP_SOFTWARE_SEMA42)) */

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* HSE_IP_H */

