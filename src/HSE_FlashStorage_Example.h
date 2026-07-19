/**
 ******************************************************************************
 * @file     HSE_FlashStorage_Example.h
 * @brief    Example: encrypt data with an HSE AES key and store the ciphertext
 *           in on-chip Data Flash, then read it back and decrypt it.
 * @location /test/src/HSE_FlashStorage_Example.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifndef __HSE_FLASHSTORAGE_EXAMPLE_H__
#define __HSE_FLASHSTORAGE_EXAMPLE_H__

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
==================================================================================================*/
#include "HSE_Main.h"
#include "C40_Ip.h"

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
/* --- NVM key path: encrypt -> write to flash -> read back -> decrypt. Works across a reset,
   since AES_NVM_KEY_HANDLE's value persists in HSE's own flash. --- */
extern hseSrvResponse_t  HSE_Example_NvmKeyEncryptResponse;
extern C40_Ip_StatusType HSE_Example_NvmKeyFlashWriteStatus;
extern C40_Ip_StatusType HSE_Example_NvmKeyFlashReadStatus;
extern hseSrvResponse_t  HSE_Example_NvmKeyDecryptResponse;
extern bool              HSE_Example_NvmKeyRoundTripMatch;

/* --- Generated (ephemeral RAM) key path: same pipeline, but only round-trips within the
   SAME boot - see the warning in HSE_Example_StoreEncryptedDataDemo(). --- */
extern hseSrvResponse_t  HSE_Example_GeneratedKeyEncryptResponse;
extern C40_Ip_StatusType HSE_Example_GeneratedKeyFlashWriteStatus;
extern C40_Ip_StatusType HSE_Example_GeneratedKeyFlashReadStatus;
extern hseSrvResponse_t  HSE_Example_GeneratedKeyDecryptResponse;
extern bool              HSE_Example_GeneratedKeyRoundTripMatch;

/* --- NVM key, CTR mode: a 30-byte message (NOT a multiple of 16) that ECB/CBC could not
   handle without padding. CTR needs no padding, so the ciphertext is exactly 30 bytes, but
   a fresh IV must be generated per message and stored alongside it (see the .c file). --- */
extern bool              HSE_Example_CtrIvGenerated;
extern hseSrvResponse_t  HSE_Example_CtrEncryptResponse;
extern C40_Ip_StatusType HSE_Example_CtrFlashWriteStatus;
extern C40_Ip_StatusType HSE_Example_CtrFlashReadStatus;
extern hseSrvResponse_t  HSE_Example_CtrDecryptResponse;
extern bool              HSE_Example_CtrRoundTripMatch;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*!
 * @brief   Runs both example pipelines (NVM key, generated key). Call this once, after
 *          HSE_Init() has already imported the NVM key and generated the RAM key
 *          (HSE_ImportNvmAESKey() / HSE_GenerateAesKey() must have already run).
 */
void HSE_Example_StoreEncryptedDataDemo(void);

#ifdef __cplusplus
}
#endif

#endif /* __HSE_FLASHSTORAGE_EXAMPLE_H__ */

/** @} */
