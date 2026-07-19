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

/**
*   @file
*
*   @addtogroup CRYPTO_43_HSE
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

/*==================================================================================================
*                                 SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define HSE_IP_CFG_VENDOR_ID_C                       43
#define HSE_IP_CFG_SW_MAJOR_VERSION_C                7
#define HSE_IP_CFG_SW_MINOR_VERSION_C                0
#define HSE_IP_CFG_SW_PATCH_VERSION_C                1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
/* Check if Hse Ip configuration source file and Hse Ip configuration header file are of the same vendor */
#if (HSE_IP_CFG_VENDOR_ID_C != HSE_IP_CFG_VENDOR_ID_H)
    #error "Hse_Ip_Cfg.c and Hse_Ip_Cfg.h have different vendor ids"
#endif

/* Check if Hse Ip configuration source file and Hse Ip configuration header file are of the same software version */
#if ((HSE_IP_CFG_SW_MAJOR_VERSION_C != HSE_IP_CFG_SW_MAJOR_VERSION_H) || \
     (HSE_IP_CFG_SW_MINOR_VERSION_C != HSE_IP_CFG_SW_MINOR_VERSION_H) || \
     (HSE_IP_CFG_SW_PATCH_VERSION_C != HSE_IP_CFG_SW_PATCH_VERSION_H) \
    )
    #error "Software Version Numbers of Hse_Ip_Cfg.c and Hse_Ip_Cfg.h are different"
#endif

/*==================================================================================================
*                           LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/

/*==================================================================================================
*                                         LOCAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/

/*==================================================================================================
*                                         LOCAL FUNCTIONS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL FUNCTIONS
==================================================================================================*/

#ifdef __cplusplus
}
#endif

/** @} */

