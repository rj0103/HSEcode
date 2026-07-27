/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           : C40
*   Dependencies         : None
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

#ifndef C40_IP_CFG_H
#define C40_IP_CFG_H

/**
 * @file       C40_Ip_Cfg.h
 *
 * @addtogroup C40_IP
 * implement   C40_Ip_Cfg.h_Artifact
 *
 * @{
 */

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                        INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
#include "OsIf.h"
#include "C40_Ip_Types.h"
#include "S32K312_PFLASH.h"
#include "S32K312_FLASH.h"


/*==================================================================================================
*                              SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define C40_IP_VENDOR_ID_CFG                          43
#define C40_IP_AR_RELEASE_MAJOR_VERSION_CFG           4
#define C40_IP_AR_RELEASE_MINOR_VERSION_CFG           9
#define C40_IP_AR_RELEASE_REVISION_VERSION_CFG        0
#define C40_IP_SW_MAJOR_VERSION_CFG                   7
#define C40_IP_SW_MINOR_VERSION_CFG                   0
#define C40_IP_SW_PATCH_VERSION_CFG                   1

/*==================================================================================================
*                              SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
    /* Check if current file and OsIf.h header file are of the same Autosar version */
    #if ((C40_IP_AR_RELEASE_MAJOR_VERSION_CFG != OSIF_AR_RELEASE_MAJOR_VERSION) || \
         (C40_IP_AR_RELEASE_MINOR_VERSION_CFG != OSIF_AR_RELEASE_MINOR_VERSION) \
        )
        #error "AutoSar Version Numbers of C40_Ip_Cfg.h and OsIf.h are different"
    #endif
#endif

/* Check if current file and C40_Ip_Types header file are of the same vendor */
#if (C40_IP_VENDOR_ID_CFG != C40_IP_TYPES_VENDOR_ID_CFG)
    #error "C40_Ip_Cfg.h and C40_Ip_Types.h have different vendor ids"
#endif
/* Check if current file and C40_Ip_Types header file are of the same Autosar version */
#if ((C40_IP_AR_RELEASE_MAJOR_VERSION_CFG    != C40_IP_TYPES_AR_RELEASE_MAJOR_VERSION_CFG) || \
     (C40_IP_AR_RELEASE_MINOR_VERSION_CFG    != C40_IP_TYPES_AR_RELEASE_MINOR_VERSION_CFG) || \
     (C40_IP_AR_RELEASE_REVISION_VERSION_CFG != C40_IP_TYPES_AR_RELEASE_REVISION_VERSION_CFG) \
    )
    #error "AutoSar Version Numbers of C40_Ip_Cfg.h and C40_Ip_Types.h are different"
#endif
/* Check if current file and C40_Ip_Types header file are of the same Software version */
#if ((C40_IP_SW_MAJOR_VERSION_CFG != C40_IP_TYPES_SW_MAJOR_VERSION_CFG) || \
     (C40_IP_SW_MINOR_VERSION_CFG != C40_IP_TYPES_SW_MINOR_VERSION_CFG) || \
     (C40_IP_SW_PATCH_VERSION_CFG != C40_IP_TYPES_SW_PATCH_VERSION_CFG) \
    )
    #error "Software Version Numbers of C40_Ip_Cfg.h and C40_Ip_Types.h are different"
#endif


/*==================================================================================================
                                       DEFINES AND MACROS
==================================================================================================*/

/* Development error detection for C40_IP API */
#define C40_IP_DEV_ERROR_DETECT                (STD_OFF)
#define C40_IP_UTEST_MODE_API                  (STD_ON)
#define C40_IP_SECTOR_SET_LOCK_API             (STD_ON)

#define C40_IP_ERASE_VERIFICATION_ENABLED      (STD_OFF)
#define C40_IP_PROGRAM_VERIFICATION_ENABLED    (STD_OFF)

#define C40_IP_TIMEOUT_SUPERVISION_ENABLED     (STD_OFF)
#if (STD_ON == C40_IP_TIMEOUT_SUPERVISION_ENABLED)
#define C40_IP_TIMEOUT_TYPE                    (OSIF_COUNTER_DUMMY)
#define C40_IP_ERASE_TIMEOUT                   (0U)
#define C40_IP_WRITE_TIMEOUT                   (0U)
#define C40_IP_ABORT_TIMEOUT                   (0U)
#if (STD_ON == C40_IP_UTEST_MODE_API)
#define C40_IP_UTEST_ARRAYINTEGRITYCHECK_TIMEOUT   (0U)
#define C40_IP_UTEST_SUSPEND_TIMEOUT               (0U)
#define C40_IP_UTEST_RESUME_TIMEOUT                (0U)
#endif  /*(STD_ON == C40_IP_UTEST_MODE_API)*/
#endif  /*(STD_ON == C40_IP_TIMEOUT_SUPERVISION_ENABLED)*/

/* Enable Multi-Core support when using MemAcc*/
#define C40_IP_MULTICORE_ENABLED               (STD_OFF)
#define C40_IP_ENABLE_USER_MODE_SUPPORT        (STD_OFF)
#ifndef MCAL_ENABLE_USER_MODE_SUPPORT
    #if (STD_ON == C40_IP_ENABLE_USER_MODE_SUPPORT)
        #error MCAL_ENABLE_USER_MODE_SUPPORT is not enabled. For running driver in user mode the MCAL_ENABLE_USER_MODE_SUPPORT needs to be defined
    #endif
#endif

/* Enable the use of cache invalidate */
#define C40_IP_SYNCRONIZE_CACHE                (STD_OFF)

#define C40_IP_ECC_DATA_ERROR_SUPPRESSION      (STD_OFF)

#define C40_IP_BLOCK_4_PIPE_SELECTABLE         (STD_ON)
#if (STD_ON == C40_IP_BLOCK_4_PIPE_SELECTABLE)
#define C40_IP_BLOCK4_PIPE_SELECT              (0x00U)
#endif


#define C40_IP_ERASED_VALUE_32                 (0xffffffffU)
#define C40_IP_ERASED_VALUE_16                 (C40_IP_ERASED_VALUE_32 & 0xFFFFU)
#define C40_IP_ERASED_VALUE_8                  (C40_IP_ERASED_VALUE_32 & 0xFFU)

#define C40_IP_ECC_CHECK                       (STD_OFF)
#if (C40_IP_ECC_CHECK == STD_ON)
    #define C40_IP_ECC_VALUE                   (0x55155515U)
#endif

/**
 * @brief  number of sector in a code block2M.
 */
#define C40_IP_NUM_OF_SECTOR_PER_BLOCK         (128U)
/**
 * @brief  number of supper sector in a code block2M.
 */
#define C40_IP_NUM_OF_SUPPER_SECTOR_PER_BLOCK  (12U)
/**
 * @brief  Default offset for single PFLASH configuration when MultiPFlash is not supported
 */
#define C40_IP_PFCBLK_LOCKMASTER_SS_PFLASH     (0x10U)

/**
 * @brief  number of data sector
 */
#define C40_IP_MAX_DATA_SECTOR                 (16U)

/* Code block 0 addresses */
#define C40_IP_CODE_BLOCK_0_BASE_ADDR          (0x00400000U)
#define C40_IP_CODE_BLOCK_0_END_ADDR           (0x004FFFFFU)

/* Code block 1 addresses */
#define C40_IP_CODE_BLOCK_1_BASE_ADDR          (0x00500000U)
#define C40_IP_CODE_BLOCK_1_END_ADDR           (0x005FFFFFU)







/* Data block addresses */
#define C40_IP_DATA_BLOCK_BASE_ADDR            (0x10000000U)
#define C40_IP_DATA_BLOCK_END_ADDR             (0x1001FFFFU)

/* Utest block addresses */
#define C40_IP_UTEST_BLOCK_BASE_ADDR           (0x1B000000U)
#define C40_IP_UTEST_BLOCK_END_ADDR            (0x1B001FFFU)

/* Memory flash sector characteristics (this value is maximum index (included Utest sector)) */
#define C40_IP_MAX_VIRTUAL_SECTOR              (272U)

#define C40_IP_CODE_BLOCK_SIZE                 (0x200000U)
#define C40_IP_CODE_BLOCK_END_ADDR             (C40_IP_CODE_BLOCK_0_BASE_ADDR + C40_IP_CODE_BLOCK_SIZE - 1U)

/* Failing address based on the flash memory addressing scheme */
#define C40_IP_REGION_0_ADDR                   (C40_IP_CODE_BLOCK_0_BASE_ADDR)
#define C40_IP_REGION_1_ADDR                   (C40_IP_CODE_BLOCK_1_BASE_ADDR)
#define C40_IP_REGION_2_ADDR                   (C40_IP_DATA_BLOCK_BASE_ADDR)
#define C40_IP_REGION_3_ADDR                   (C40_IP_INVALID_ADDR)
#define C40_IP_REGION_4_ADDR                   (C40_IP_INVALID_ADDR)
#define C40_IP_REGION_5_ADDR                   (C40_IP_INVALID_ADDR)

#define C40_IP_INVALID_ADDR                    (0xFFFFFFFFU)

#define IP_PFLASH_COMMON                             (IP_PFLASH)
#define IP_PFLASH_TYPE_COMMON                        PFLASH_Type
/** @brief  PFlash register base address of PFlash */
#define PFLASH_BASEADDR                             (IP_PFLASH_BASE)
#define PFLASH_PFCBLK_DATA_ORDER                    (PFLASH_PFCBLKI_SPELOCK_COUNT - 1U)

/** @brief  Order of data flash PFCBLK Registers Arrays */
#define PFLASH_PFCBLK_DATA_LOCKMASTER_S0_ADDR32     ((uint32)&(IP_PFLASH->PFCBLK_LOCKMASTER_S[PFLASH_PFCBLKI_LOCKMASTER_S_COUNT - 1U][0U]))
/** @brief  Block a Lock Master Sector */
#define PFLASH_PFCBLK0_LOCKMASTER_S0_ADDR32         ((uint32)&(IP_PFLASH->PFCBLK_LOCKMASTER_S[0U][0U]))
/** @brief   Block UTEST Lock Master Sector */
#define PFLASH_PFCBLKU_LOCKMASTER_S_ADDR32          ((uint32)&(IP_PFLASH->PFCBLKU_LOCKMASTER_S[0U]))
/** @brief   Block a Lock Master Super Sector */
#define PFLASH_PFCBLK0_LOCKMASTER_SS0_ADDR32        ((uint32)(PFLASH_BASEADDR + (uint32)0x484UL))

/*==================================================================================================
                                 ENUM TYPEDEFS
==================================================================================================*/

/**
    For flash area : Code flash : Start 0040_0000h -> 0x007F_E000h and Data Flash: 1000_0000h -> 1003_FFFFh and Utest Flash :  1B00_0000h -> 1B00_1FFFh
*/
#define C40_DATA_ARRAY_0_BLOCK_2_S000       (0U)    /* 0x10000000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S001       (1U)    /* 0x10002000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S002       (2U)    /* 0x10004000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S003       (3U)    /* 0x10006000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S004       (4U)    /* 0x10008000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S005       (5U)    /* 0x1000A000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S006       (6U)    /* 0x1000C000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S007       (7U)    /* 0x1000E000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S008       (8U)    /* 0x10010000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S009       (9U)    /* 0x10012000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S010       (10U)    /* 0x10014000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S011       (11U)    /* 0x10016000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S012       (12U)    /* 0x10018000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S013       (13U)    /* 0x1001A000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S014       (14U)    /* 0x1001C000 */
#define C40_DATA_ARRAY_0_BLOCK_2_S015       (15U)    /* 0x1001E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S000       (16U)    /* 0x00400000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S001       (17U)    /* 0x00402000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S002       (18U)    /* 0x00404000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S003       (19U)    /* 0x00406000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S004       (20U)    /* 0x00408000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S005       (21U)    /* 0x0040A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S006       (22U)    /* 0x0040C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S007       (23U)    /* 0x0040E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S008       (24U)    /* 0x00410000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S009       (25U)    /* 0x00412000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S010       (26U)    /* 0x00414000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S011       (27U)    /* 0x00416000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S012       (28U)    /* 0x00418000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S013       (29U)    /* 0x0041A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S014       (30U)    /* 0x0041C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S015       (31U)    /* 0x0041E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S016       (32U)    /* 0x00420000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S017       (33U)    /* 0x00422000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S018       (34U)    /* 0x00424000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S019       (35U)    /* 0x00426000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S020       (36U)    /* 0x00428000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S021       (37U)    /* 0x0042A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S022       (38U)    /* 0x0042C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S023       (39U)    /* 0x0042E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S024       (40U)    /* 0x00430000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S025       (41U)    /* 0x00432000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S026       (42U)    /* 0x00434000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S027       (43U)    /* 0x00436000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S028       (44U)    /* 0x00438000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S029       (45U)    /* 0x0043A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S030       (46U)    /* 0x0043C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S031       (47U)    /* 0x0043E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S032       (48U)    /* 0x00440000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S033       (49U)    /* 0x00442000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S034       (50U)    /* 0x00444000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S035       (51U)    /* 0x00446000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S036       (52U)    /* 0x00448000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S037       (53U)    /* 0x0044A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S038       (54U)    /* 0x0044C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S039       (55U)    /* 0x0044E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S040       (56U)    /* 0x00450000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S041       (57U)    /* 0x00452000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S042       (58U)    /* 0x00454000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S043       (59U)    /* 0x00456000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S044       (60U)    /* 0x00458000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S045       (61U)    /* 0x0045A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S046       (62U)    /* 0x0045C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S047       (63U)    /* 0x0045E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S048       (64U)    /* 0x00460000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S049       (65U)    /* 0x00462000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S050       (66U)    /* 0x00464000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S051       (67U)    /* 0x00466000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S052       (68U)    /* 0x00468000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S053       (69U)    /* 0x0046A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S054       (70U)    /* 0x0046C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S055       (71U)    /* 0x0046E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S056       (72U)    /* 0x00470000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S057       (73U)    /* 0x00472000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S058       (74U)    /* 0x00474000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S059       (75U)    /* 0x00476000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S060       (76U)    /* 0x00478000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S061       (77U)    /* 0x0047A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S062       (78U)    /* 0x0047C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S063       (79U)    /* 0x0047E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S064       (80U)    /* 0x00480000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S065       (81U)    /* 0x00482000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S066       (82U)    /* 0x00484000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S067       (83U)    /* 0x00486000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S068       (84U)    /* 0x00488000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S069       (85U)    /* 0x0048A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S070       (86U)    /* 0x0048C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S071       (87U)    /* 0x0048E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S072       (88U)    /* 0x00490000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S073       (89U)    /* 0x00492000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S074       (90U)    /* 0x00494000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S075       (91U)    /* 0x00496000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S076       (92U)    /* 0x00498000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S077       (93U)    /* 0x0049A000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S078       (94U)    /* 0x0049C000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S079       (95U)    /* 0x0049E000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S080       (96U)    /* 0x004A0000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S081       (97U)    /* 0x004A2000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S082       (98U)    /* 0x004A4000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S083       (99U)    /* 0x004A6000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S084       (100U)    /* 0x004A8000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S085       (101U)    /* 0x004AA000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S086       (102U)    /* 0x004AC000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S087       (103U)    /* 0x004AE000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S088       (104U)    /* 0x004B0000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S089       (105U)    /* 0x004B2000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S090       (106U)    /* 0x004B4000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S091       (107U)    /* 0x004B6000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S092       (108U)    /* 0x004B8000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S093       (109U)    /* 0x004BA000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S094       (110U)    /* 0x004BC000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S095       (111U)    /* 0x004BE000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S096       (112U)    /* 0x004C0000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S097       (113U)    /* 0x004C2000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S098       (114U)    /* 0x004C4000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S099       (115U)    /* 0x004C6000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S100       (116U)    /* 0x004C8000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S101       (117U)    /* 0x004CA000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S102       (118U)    /* 0x004CC000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S103       (119U)    /* 0x004CE000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S104       (120U)    /* 0x004D0000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S105       (121U)    /* 0x004D2000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S106       (122U)    /* 0x004D4000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S107       (123U)    /* 0x004D6000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S108       (124U)    /* 0x004D8000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S109       (125U)    /* 0x004DA000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S110       (126U)    /* 0x004DC000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S111       (127U)    /* 0x004DE000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S112       (128U)    /* 0x004E0000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S113       (129U)    /* 0x004E2000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S114       (130U)    /* 0x004E4000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S115       (131U)    /* 0x004E6000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S116       (132U)    /* 0x004E8000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S117       (133U)    /* 0x004EA000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S118       (134U)    /* 0x004EC000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S119       (135U)    /* 0x004EE000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S120       (136U)    /* 0x004F0000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S121       (137U)    /* 0x004F2000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S122       (138U)    /* 0x004F4000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S123       (139U)    /* 0x004F6000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S124       (140U)    /* 0x004F8000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S125       (141U)    /* 0x004FA000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S126       (142U)    /* 0x004FC000 */
#define C40_CODE_ARRAY_0_BLOCK_0_S127       (143U)    /* 0x004FE000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S128       (144U)    /* 0x00500000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S129       (145U)    /* 0x00502000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S130       (146U)    /* 0x00504000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S131       (147U)    /* 0x00506000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S132       (148U)    /* 0x00508000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S133       (149U)    /* 0x0050A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S134       (150U)    /* 0x0050C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S135       (151U)    /* 0x0050E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S136       (152U)    /* 0x00510000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S137       (153U)    /* 0x00512000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S138       (154U)    /* 0x00514000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S139       (155U)    /* 0x00516000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S140       (156U)    /* 0x00518000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S141       (157U)    /* 0x0051A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S142       (158U)    /* 0x0051C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S143       (159U)    /* 0x0051E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S144       (160U)    /* 0x00520000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S145       (161U)    /* 0x00522000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S146       (162U)    /* 0x00524000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S147       (163U)    /* 0x00526000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S148       (164U)    /* 0x00528000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S149       (165U)    /* 0x0052A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S150       (166U)    /* 0x0052C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S151       (167U)    /* 0x0052E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S152       (168U)    /* 0x00530000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S153       (169U)    /* 0x00532000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S154       (170U)    /* 0x00534000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S155       (171U)    /* 0x00536000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S156       (172U)    /* 0x00538000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S157       (173U)    /* 0x0053A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S158       (174U)    /* 0x0053C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S159       (175U)    /* 0x0053E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S160       (176U)    /* 0x00540000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S161       (177U)    /* 0x00542000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S162       (178U)    /* 0x00544000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S163       (179U)    /* 0x00546000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S164       (180U)    /* 0x00548000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S165       (181U)    /* 0x0054A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S166       (182U)    /* 0x0054C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S167       (183U)    /* 0x0054E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S168       (184U)    /* 0x00550000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S169       (185U)    /* 0x00552000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S170       (186U)    /* 0x00554000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S171       (187U)    /* 0x00556000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S172       (188U)    /* 0x00558000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S173       (189U)    /* 0x0055A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S174       (190U)    /* 0x0055C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S175       (191U)    /* 0x0055E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S176       (192U)    /* 0x00560000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S177       (193U)    /* 0x00562000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S178       (194U)    /* 0x00564000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S179       (195U)    /* 0x00566000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S180       (196U)    /* 0x00568000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S181       (197U)    /* 0x0056A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S182       (198U)    /* 0x0056C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S183       (199U)    /* 0x0056E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S184       (200U)    /* 0x00570000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S185       (201U)    /* 0x00572000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S186       (202U)    /* 0x00574000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S187       (203U)    /* 0x00576000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S188       (204U)    /* 0x00578000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S189       (205U)    /* 0x0057A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S190       (206U)    /* 0x0057C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S191       (207U)    /* 0x0057E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S192       (208U)    /* 0x00580000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S193       (209U)    /* 0x00582000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S194       (210U)    /* 0x00584000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S195       (211U)    /* 0x00586000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S196       (212U)    /* 0x00588000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S197       (213U)    /* 0x0058A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S198       (214U)    /* 0x0058C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S199       (215U)    /* 0x0058E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S200       (216U)    /* 0x00590000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S201       (217U)    /* 0x00592000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S202       (218U)    /* 0x00594000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S203       (219U)    /* 0x00596000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S204       (220U)    /* 0x00598000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S205       (221U)    /* 0x0059A000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S206       (222U)    /* 0x0059C000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S207       (223U)    /* 0x0059E000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S208       (224U)    /* 0x005A0000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S209       (225U)    /* 0x005A2000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S210       (226U)    /* 0x005A4000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S211       (227U)    /* 0x005A6000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S212       (228U)    /* 0x005A8000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S213       (229U)    /* 0x005AA000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S214       (230U)    /* 0x005AC000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S215       (231U)    /* 0x005AE000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S216       (232U)    /* 0x005B0000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S217       (233U)    /* 0x005B2000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S218       (234U)    /* 0x005B4000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S219       (235U)    /* 0x005B6000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S220       (236U)    /* 0x005B8000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S221       (237U)    /* 0x005BA000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S222       (238U)    /* 0x005BC000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S223       (239U)    /* 0x005BE000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S224       (240U)    /* 0x005C0000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S225       (241U)    /* 0x005C2000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S226       (242U)    /* 0x005C4000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S227       (243U)    /* 0x005C6000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S228       (244U)    /* 0x005C8000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S229       (245U)    /* 0x005CA000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S230       (246U)    /* 0x005CC000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S231       (247U)    /* 0x005CE000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S232       (248U)    /* 0x005D0000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S233       (249U)    /* 0x005D2000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S234       (250U)    /* 0x005D4000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S235       (251U)    /* 0x005D6000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S236       (252U)    /* 0x005D8000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S237       (253U)    /* 0x005DA000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S238       (254U)    /* 0x005DC000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S239       (255U)    /* 0x005DE000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S240       (256U)    /* 0x005E0000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S241       (257U)    /* 0x005E2000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S242       (258U)    /* 0x005E4000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S243       (259U)    /* 0x005E6000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S244       (260U)    /* 0x005E8000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S245       (261U)    /* 0x005EA000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S246       (262U)    /* 0x005EC000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S247       (263U)    /* 0x005EE000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S248       (264U)    /* 0x005F0000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S249       (265U)    /* 0x005F2000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S250       (266U)    /* 0x005F4000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S251       (267U)    /* 0x005F6000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S252       (268U)    /* 0x005F8000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S253       (269U)    /* 0x005FA000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S254       (270U)    /* 0x005FC000 */
#define C40_CODE_ARRAY_0_BLOCK_1_S255       (271U)    /* 0x005FE000 */
#define C40_UTEST_ARRAY_0_S000              (272U)    /* 0x1B000000 */
#define C40_IP_SECTOR_ERROR                 (273U)    /* default and error return value */


/*==================================================================================================
                                 GLOBAL CONSTANT DECLARATIONS
==================================================================================================*/

#define MEM_43_INFLS_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "Mem_43_INFLS_MemMap.h"

extern const C40_Ip_ConfigType C40_Ip_InitCfg;

#define MEM_43_INFLS_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "Mem_43_INFLS_MemMap.h"


#ifdef __cplusplus
}
#endif

/** @} */

#endif /* C40_IP_CFG_H */

