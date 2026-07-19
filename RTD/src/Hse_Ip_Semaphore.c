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
#include "Hse_Ip.h"
#include "Hse_Ip_Semaphore.h"

/*==================================================================================================
*                                 SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define HSE_IP_SEMAPHORE_VENDOR_ID_C                    43
#define HSE_IP_SEMAPHORE_SW_MAJOR_VERSION_C             7
#define HSE_IP_SEMAPHORE_SW_MINOR_VERSION_C             0
#define HSE_IP_SEMAPHORE_SW_PATCH_VERSION_C             1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
/* Check if Hse_Ip_Semaphore source file and Hse_Ip header file are of the same vendor */
#if (HSE_IP_SEMAPHORE_VENDOR_ID_C != HSE_IP_VENDOR_ID_H)
    #error "Hse_Ip_Semaphore.c and Hse_Ip.h have different vendor ids"
#endif

/* Check if Hse_Ip_Semaphore source file and Hse_Ip header file are of the same Software version */
#if ((HSE_IP_SEMAPHORE_SW_MAJOR_VERSION_C != HSE_IP_SW_MAJOR_VERSION_H) || \
     (HSE_IP_SEMAPHORE_SW_MINOR_VERSION_C != HSE_IP_SW_MINOR_VERSION_H) || \
     (HSE_IP_SEMAPHORE_SW_PATCH_VERSION_C != HSE_IP_SW_PATCH_VERSION_H)    \
    )
    #error "Software Version Numbers of Hse_Ip_Semaphore.c and Hse_Ip.h are different"
#endif

/* Check if Hse_Ip_Semaphore source file and Hse_Ip_Semaphore header file are of the same vendor */
#if (HSE_IP_SEMAPHORE_VENDOR_ID_C != HSE_IP_SEMAPHORE_VENDOR_ID_H)
    #error "Hse_Ip_Semaphore.c and Hse_Ip_Semaphore.h have different vendor ids"
#endif

/* Check if Hse_Ip_Semaphore source file and Hse_Ip_Semaphore header file are of the same Software version */
#if ((HSE_IP_SEMAPHORE_SW_MAJOR_VERSION_C != HSE_IP_SEMAPHORE_SW_MAJOR_VERSION_H) || \
     (HSE_IP_SEMAPHORE_SW_MINOR_VERSION_C != HSE_IP_SEMAPHORE_SW_MINOR_VERSION_H) || \
     (HSE_IP_SEMAPHORE_SW_PATCH_VERSION_C != HSE_IP_SEMAPHORE_SW_PATCH_VERSION_H)    \
    )
    #error "Software Version Numbers of Hse_Ip_Semaphore.c and Hse_Ip_Semaphore.h are different"
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

#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)
#define CRYPTO_43_HSE_START_SEC_CONST_32
#include "Crypto_43_HSE_MemMap.h"

volatile uint32* const pu32Crypto_Software_Semaphore_Gate = (uint32*) HSE_IP_SOFTWARE_SEMA42_ADDRESS;

#define CRYPTO_43_HSE_STOP_SEC_CONST_32
#include "Crypto_43_HSE_MemMap.h"

#define CRYPTO_43_HSE_START_SEC_VAR_CLEARED_8_NO_CACHEABLE
#include "Crypto_43_HSE_MemMap.h"

static uint8 u32CountingSEMA4 = 0;

#define CRYPTO_43_HSE_STOP_SEC_VAR_CLEARED_8_NO_CACHEABLE
#include "Crypto_43_HSE_MemMap.h"
#endif /* (STD_ON == HSE_IP_SOFTWARE_SEMA42) */
/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)

#define CRYPTO_43_HSE_START_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

static inline uint32  Hse_Ip_SWSemaphore_LockValue
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
);

static inline boolean Hse_Ip_SWSemaphore_ReleaseLock
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
);

static inline boolean Hse_Ip_SWSemaphore_RequestLock
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
);

#define CRYPTO_43_HSE_STOP_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

#endif /* (STD_ON == HSE_IP_SOFTWARE_SEMA42) */
/*==================================================================================================
*                                         LOCAL FUNCTIONS
==================================================================================================*/
#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)

#define CRYPTO_43_HSE_START_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

/*!
 * @brief      This service calculate unique lock value for each crypto request to avoid conflicts in case of concurrent requests.
 *
 * @param[in]  u8MuInstance MU Instance is used to calculate unique locked value in case of multicore/multi-partition
 * @param[in]  u8MuChannel  MU Channel used to calculate unique locked value in case of two concurrent async request
 *
 * @return     Unique lock value for Software semaphore
 */
static inline uint32  Hse_Ip_SWSemaphore_LockValue
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
)
{
    const uint8 u8CoreId     = Hse_Ip_GetPhysicalCoreId();
    uint32      u32LockValue = HSE_IP_SOFTWARE_SEMA42_LOCKED_VALUE;

    /* Masking with core to avoid conflict on crypto lock requests from two different cores at same time */
    u32LockValue |= (((uint32)u8CoreId) << 8);

    /* Masking with MU Instance ID to avoid conflict on crypto lock requests from two different partitions from same core */
    u32LockValue |= (((uint32)u8MuInstance) << 4);

    /* Masking with MU Channel to avoid conflict on crypto lock requests from two asynchronous requests on same partition and core */
    u32LockValue |= ((uint32)u8MuChannel);

    return u32LockValue;
}

/*!
 * @brief      Release Software Semaphore lock
 *
 * @param[in]  u8MuInstance MU Instance is used to calculate unique locked value in case of multicore/multi-partition
 * @param[in]  u8MuChannel  MU Channel used to calculate unique locked value in case of two concurrent async request
 *
 * @return     TRUE when lock is acquired FALSE in other case
 */
static inline boolean Hse_Ip_SWSemaphore_ReleaseLock
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
)
{
    boolean boSwSema4Unlocked = TRUE;
    uint32  u32SwSema4LockVal = Hse_Ip_SWSemaphore_LockValue(u8MuInstance, u8MuChannel);

    /* If the Semaphore is not NULL, try to clear it. */
    if (pu32Crypto_Software_Semaphore_Gate != NULL_PTR)
    {
        if(u32CountingSEMA4>0)
        {
            u32CountingSEMA4--;
        }

        if(u32CountingSEMA4==0)
        {
            boSwSema4Unlocked = (boolean) OsIf_Software_Semaphore_Unlock((uint32 *)pu32Crypto_Software_Semaphore_Gate, u32SwSema4LockVal);
        }
    }
    else
    {
        boSwSema4Unlocked = FALSE;
    }

    return boSwSema4Unlocked;
}

/*!
 * @brief      Request for Software Semaphore lock
 *
 * @param[in]  u8MuInstance MU Instance is used to calculate unique locked value in case of multicore/multi-partition
 * @param[in]  u8MuChannel  MU Channel used to calculate unique locked value in case of two concurrent async request
 *
 * @return     TRUE when lock is released FALSE in other case
 */
static inline boolean Hse_Ip_SWSemaphore_RequestLock
(
    const uint8 u8MuInstance,
    const uint8 u8MuChannel
)
{
    boolean boSwSema4Locked   = (boolean) FALSE;
    uint32  u32SwSema4LockVal = Hse_Ip_SWSemaphore_LockValue(u8MuInstance, u8MuChannel);

    if (NULL_PTR != pu32Crypto_Software_Semaphore_Gate)
    {
        if(OSIF_SOFTWARE_SEMAPHORE_UNLOCKED_VALUE == (*pu32Crypto_Software_Semaphore_Gate))
        {
            u32CountingSEMA4=0;
            boSwSema4Locked = (boolean) OsIf_Software_Semaphore_Lock((uint32 *)pu32Crypto_Software_Semaphore_Gate, u32SwSema4LockVal);
        }

        /* Check if lock is not acquired by another thread or partition to avoid conflict at concurrent lock request */
        if (((boolean) TRUE == boSwSema4Locked) && (u32SwSema4LockVal != (*pu32Crypto_Software_Semaphore_Gate)))
        {
            /* Return false lock is acquired by other driver or crypto request */
            boSwSema4Locked = (boolean) FALSE;
        }
        else if (((boolean) FALSE == boSwSema4Locked) && (u32SwSema4LockVal == (*pu32Crypto_Software_Semaphore_Gate)))
        {
            /* Lock is acquired same instance of crypto driver */
            if(u32CountingSEMA4 < HSE_IP_COUNTINGSEMA4_MAXVALUE)
            {
                u32CountingSEMA4++;
                boSwSema4Locked = (boolean) TRUE;
            }
        }
        else
        {
            u32CountingSEMA4=1;
        }
    }

    return boSwSema4Locked;
}

#define CRYPTO_43_HSE_STOP_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

#endif /* (STD_ON == HSE_IP_SOFTWARE_SEMA42) */
/*==================================================================================================
*                                        GLOBAL FUNCTIONS PROTOTYPES
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL FUNCTIONS
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
)
{
    boolean bLockRequired    = FALSE;
    uint8   u8ServiceClass   = (uint8)((hseSrvId & HSE_IP_SERVICE_CLASS_MASK_U32) >> 8u);

    if ((HSE_IP_ADMINISTRATIVE_SERVICE_CLASS_MASK == u8ServiceClass)
     || (HSE_IP_KEY_MANAGEMENT_SERVICE_CLASS_MASK == u8ServiceClass)
     || (HSE_IP_KEY_MONOTONIC_COUNTERS_CLASS_MASK == u8ServiceClass)
     || (HSE_IP_SECURE_MEM_REGIONS_MGT_CLASS_MASK == u8ServiceClass)
#ifdef HSE_SRV_ID_SHE_LOAD_KEY
     || (HSE_SRV_ID_SHE_LOAD_KEY == hseSrvId)
#endif
    )
    {
        bLockRequired = TRUE;
    }
    else
    {
       /* The current service does not required the usage of semaphore, therefore nothing to do */
    }

    return bLockRequired;
}

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
)
{
    boolean boSema4Unlocked = FALSE;

#if (STD_ON == HSE_IP_HARDWARE_SEMA42)
    boSema4Unlocked = ((Std_ReturnType)(Rm_SemaphoreUnlockGate((uint8)HSE_IP_HARDWARE_SEMA42_GATE)) == E_OK) ? (TRUE) : (FALSE);
#endif /*(STD_ON == HSE_IP_HARDWARE_SEMA42)*/
#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)
    boSema4Unlocked = Hse_Ip_SWSemaphore_ReleaseLock(u8MuInstance, u8MuChannel);
#else
    (void) u8MuInstance;
    (void) u8MuChannel;
#endif /*(STD_ON == HSE_IP_SOFTWARE_SEMA42)*/

    return boSema4Unlocked;
}

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
)
{
    boolean boSema4Locked = FALSE;

#if (STD_ON == HSE_IP_HARDWARE_SEMA42)
    boSema4Locked = ((Std_ReturnType)(Rm_SemaphoreLockGate((uint8)HSE_IP_HARDWARE_SEMA42_GATE)) == E_OK) ? (TRUE) : (FALSE);
    if(TRUE == boSema4Locked)
#endif /*(STD_ON == HSE_IP_HARDWARE_SEMA42)*/
    {
#if (STD_ON == HSE_IP_SOFTWARE_SEMA42)
        boSema4Locked = Hse_Ip_SWSemaphore_RequestLock(u8MuInstance, u8MuChannel);
#if (STD_ON == HSE_IP_HARDWARE_SEMA42)
        if(TRUE != boSema4Locked)
        {
            /* Release locked hardware semaphore in COMBINE mode when request software lock failed */
            (void)Rm_SemaphoreUnlockGate((uint8)HSE_IP_HARDWARE_SEMA42_GATE);
        }
#endif /*(STD_ON == HSE_IP_HARDWARE_SEMA42)*/
#else
        (void) u8MuInstance;
        (void) u8MuChannel;
#endif /*(STD_ON == HSE_IP_SOFTWARE_SEMA42)*/
    }

    return boSema4Locked;
}

#define CRYPTO_43_HSE_STOP_SEC_CODE
#include "Crypto_43_HSE_MemMap.h"

#endif /* ((STD_ON == HSE_IP_HARDWARE_SEMA42) || (STD_ON == HSE_IP_SOFTWARE_SEMA42)) */

#ifdef __cplusplus
}
#endif

/** @} */

