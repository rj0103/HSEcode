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

#ifndef MU_IP_MASK_H
#define MU_IP_MASK_H

/**
*   @file
*
*   @internal
*   @addtogroup CRYPTO
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

/*==================================================================================================
*                                 SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define MU_IP_MASK_SW_MAJOR_VERSION_H      7
#define MU_IP_MASK_SW_MINOR_VERSION_H      0
#define MU_IP_MASK_SW_PATCH_VERSION_H      1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/


/*==================================================================================================
*                                            CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                       DEFINES AND MACROS
==================================================================================================*/
/*! @brief Mask for valid bits in MU RCR register */
#define MU_IP_RCR_REG_VALID_BITS_MASK_U32  (0x0000000FUL)
/*! @brief Mask for valid bits in MU RSR register */
#define MU_IP_RSR_REG_VALID_BITS_MASK_U32  (0x0000000FUL)
/*! @brief Mask for valid bits in MU TSR register */
#define MU_IP_TSR_REG_VALID_BITS_MASK_U32  (0x0000000FUL)

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

#endif /* MU_IP_MASK_H */

