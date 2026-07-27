/**
 ******************************************************************************
 * @file     HSE_FlashStorage_Example.c
 * @brief    Example: encrypt data with an HSE AES key and store the ciphertext
 *           in on-chip Data Flash, then read it back and decrypt it.
 * @location /test/src/HSE_FlashStorage_Example.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifdef __cplusplus
extern "C"
{
#endif

#include "HSE_FlashStorage_Example.h"
#include "C40_Ip_Cfg.h"
#include "string.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
/* Data Flash is a separate flash bank from Program Flash (where the app code/vector table live) -
   see Project_Settings/Linker_Files/linker_flash_s32k312.ld's int_dflash region (0x10000000, 128KB)
   and generate/include/C40_Ip_Cfg.h's C40_DATA_ARRAY_0_BLOCK_2_S0xx virtual sectors. Nothing else
   in this project's linker script places any section there, so it's safe to use for application
   data storage. Each sector is 8KB (0x2000), the minimum erase granularity for this flash IP. */
#define FLASH_NVM_KEY_DATA_ADDR                  (0x10000000U)                     /* Data Flash sector 0 */
#define FLASH_NVM_KEY_DATA_VIRTUAL_SECTOR        (C40_DATA_ARRAY_0_BLOCK_2_S000)
#define FLASH_GENERATED_KEY_DATA_ADDR            (0x10002000U)                     /* Data Flash sector 1 */
#define FLASH_GENERATED_KEY_DATA_VIRTUAL_SECTOR  (C40_DATA_ARRAY_0_BLOCK_2_S001)
#define FLASH_CTR_DATA_ADDR                       (0x10004000U)                     /* Data Flash sector 2 */
#define FLASH_CTR_DATA_VIRTUAL_SECTOR             (C40_DATA_ARRAY_0_BLOCK_2_S002)

/* Single application core in this project - no sector-lock contention to arbitrate */
#define FLASH_DOMAIN_ID_U8  (0U)

/* CTR needs no padding, so the message itself can be any length - 30 bytes here, deliberately
   NOT a multiple of 16, to prove this (unlike ECB/CBC). The IV is not secret, but it must be
   unique per message and the same value is needed again at decrypt time, so it's stored on
   flash right alongside the ciphertext: [16-byte IV][30-byte ciphertext][2 bytes pad to reach
   the 8-byte write alignment C40_Ip requires]. */
#define CTR_IV_SIZE           (16U)
#define CTR_MESSAGE_SIZE      (30U)
#define CTR_FLASH_BLOB_SIZE   (48U) /* CTR_IV_SIZE + CTR_MESSAGE_SIZE, rounded up to a multiple of 8 */

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/
hseSrvResponse_t  HSE_Example_NvmKeyEncryptResponse    = HSE_SRV_RSP_GENERAL_ERROR;
C40_Ip_StatusType HSE_Example_NvmKeyFlashWriteStatus   = C40_IP_STATUS_ERROR;
C40_Ip_StatusType HSE_Example_NvmKeyFlashReadStatus    = C40_IP_STATUS_ERROR;
hseSrvResponse_t  HSE_Example_NvmKeyDecryptResponse    = HSE_SRV_RSP_GENERAL_ERROR;
bool              HSE_Example_NvmKeyRoundTripMatch     = false;

hseSrvResponse_t  HSE_Example_GeneratedKeyEncryptResponse  = HSE_SRV_RSP_GENERAL_ERROR;
C40_Ip_StatusType HSE_Example_GeneratedKeyFlashWriteStatus = C40_IP_STATUS_ERROR;
C40_Ip_StatusType HSE_Example_GeneratedKeyFlashReadStatus  = C40_IP_STATUS_ERROR;
hseSrvResponse_t  HSE_Example_GeneratedKeyDecryptResponse  = HSE_SRV_RSP_GENERAL_ERROR;
bool              HSE_Example_GeneratedKeyRoundTripMatch   = false;

bool              HSE_Example_CtrIvGenerated     = false;
hseSrvResponse_t  HSE_Example_CtrEncryptResponse = HSE_SRV_RSP_GENERAL_ERROR;
C40_Ip_StatusType HSE_Example_CtrFlashWriteStatus = C40_IP_STATUS_ERROR;
C40_Ip_StatusType HSE_Example_CtrFlashReadStatus  = C40_IP_STATUS_ERROR;
hseSrvResponse_t  HSE_Example_CtrDecryptResponse = HSE_SRV_RSP_GENERAL_ERROR;
bool              HSE_Example_CtrRoundTripMatch  = false;

/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/
static uint8_t HSE_Example_PlainText[16]              = "FlashDemoMsg16B";

static uint8_t HSE_Example_NvmCipherText[16]           = {0};  /* fresh encrypt output          */
static uint8_t HSE_Example_NvmCipherFromFlash[16]      = {0};  /* read back from flash           */
static uint8_t HSE_Example_NvmDecryptedText[16]        = {0};  /* decrypt of the flash-read data  */

static uint8_t HSE_Example_GeneratedCipherText[16]      = {0};
static uint8_t HSE_Example_GeneratedCipherFromFlash[16] = {0};
static uint8_t HSE_Example_GeneratedDecryptedText[16]   = {0};

/* 30 bytes, deliberately not a multiple of 16 - spells "CTR 30-byte non-aligned msg!!!" */
static const uint8_t HSE_Example_CtrPlainText[CTR_MESSAGE_SIZE] =
{
	'C','T','R',' ','3','0','-','b','y','t','e',' ','n','o','n','-','a','l','i','g','n','e','d',' ','m','s','g','!','!','!'
};

static uint8_t HSE_Example_CtrIv[CTR_IV_SIZE]                   = {0};
static uint8_t HSE_Example_CtrCipherText[CTR_MESSAGE_SIZE]       = {0};
static uint8_t HSE_Example_CtrFlashBlob[CTR_FLASH_BLOB_SIZE]     = {0}; /* [IV][ciphertext][pad], written/read as one piece */
static uint8_t HSE_Example_CtrDecryptedText[CTR_MESSAGE_SIZE]    = {0};

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
static C40_Ip_StatusType HSE_Example_FlashWrite(uint32 address, C40_Ip_VirtualSectorsType virtualSector, const uint8_t* pData, uint32 length);
static C40_Ip_StatusType HSE_Example_FlashRead(uint32 address, uint8_t* pData, uint32 length);

/*!
 * @brief       Erases the given Data Flash sector, then writes pData into it.
 * @details     C40_Ip's erase and write are asynchronous: each call starts the hardware job and
 *              this busy-waits on the matching *Status() function until it's no longer BUSY.
 *              Always erasing before writing keeps this example simple; a real application that
 *              writes repeatedly should track whether an erase is actually needed to reduce flash
 *              wear (each sector has a limited number of erase/write cycles).
 * @details     Data Flash sectors are program/erase-lock protected by default - C40_Ip_ClearLock()
 *              unlocks the sector before erase/write are attempted (erase and write both fail
 *              otherwise), and C40_Ip_SetLock() re-locks it afterward regardless of outcome, so a
 *              sector is never left unprotected longer than the operation actually needs.
 *
 * @param[in]   address        Destination address, must be 8-byte aligned.
 * @param[in]   virtualSector  Virtual sector index containing that address (see C40_Ip_Cfg.h).
 * @param[in]   pData          Source buffer.
 * @param[in]   length         Length in bytes, must be 8-byte aligned and at most 128 bytes.
 *
 * @return      C40_Ip_StatusType
 */
static C40_Ip_StatusType HSE_Example_FlashWrite(uint32 address, C40_Ip_VirtualSectorsType virtualSector, const uint8_t* pData, uint32 length)
{
	C40_Ip_StatusType status;
	status = C40_Ip_ClearLock(virtualSector, FLASH_DOMAIN_ID_U8);
	if (C40_IP_STATUS_SUCCESS != status)
	{
		return status;
	}

	status = C40_Ip_MainInterfaceSectorErase(virtualSector, FLASH_DOMAIN_ID_U8);
	if (C40_IP_STATUS_SUCCESS == status)
	{
		do
		{
			status = C40_Ip_MainInterfaceSectorEraseStatus();
		} while (C40_IP_STATUS_BUSY == status);
	}

	if (C40_IP_STATUS_SUCCESS == status)
	{
		status = C40_Ip_MainInterfaceWrite(address, length, pData, FLASH_DOMAIN_ID_U8);
		if (C40_IP_STATUS_SUCCESS == status)
		{
			do
			{
				status = C40_Ip_MainInterfaceWriteStatus();
			} while (C40_IP_STATUS_BUSY == status);
		}
	}

	/* Re-lock regardless of outcome - never leave the sector unprotected */
	(void)C40_Ip_SetLock(virtualSector, FLASH_DOMAIN_ID_U8);

	return status;
}

/*!
 * @brief       Reads length bytes from address into pData.
 *
 * @return      C40_Ip_StatusType
 */
static C40_Ip_StatusType HSE_Example_FlashRead(uint32 address, uint8_t* pData, uint32 length)
{
	return C40_Ip_Read(address, length, pData);
}

/*!
 * @brief       Runs both example pipelines: encrypt -> write to Data Flash -> read back -> decrypt.
 * @details
 *      1. NVM key (AES_NVM_KEY_HANDLE, via HSE_ImportNvmAESKey()): the key value persists in HSE's
 *         own flash across reset, so this pipeline genuinely survives a reboot - encrypt now, power
 *         cycle the board, and the data read back from flash right after boot can still be decrypted.
 *      2. Generated key (AES_GENERATED_RAM_KEY_HANDLE, via HSE_GenerateAesKey()): this is a RAM-only,
 *         per-boot random key - it does NOT persist. This pipeline only round-trips within the same
 *         boot it ran in. If the board resets, HSE_GenerateAesKey() produces a brand new key, and the
 *         ciphertext already sitting in flash from before the reset becomes permanently undecryptable.
 *         This is intentional here (to show the contrast with the NVM key), not a bug - never use a
 *         generated/ephemeral key to protect data that must survive a reset.
 *      3. NVM key again, but CTR mode instead of ECB: encrypts a 30-byte message (not a multiple of
 *         16) to show CTR needs no padding, unlike pipelines 1/2. A fresh IV is generated per call
 *         and stored in flash right alongside the ciphertext, since the same IV is needed to decrypt.
 *
 * @pre         HSE_Init() must have already run (HSE_ImportNvmAESKey() and HSE_GenerateAesKey() need
 *              to have provisioned AES_NVM_KEY_HANDLE / AES_GENERATED_RAM_KEY_HANDLE beforehand).
 */
void HSE_Example_StoreEncryptedDataDemo(void)
{
	/* ============================================================================================
	 *  Persistent NVM key path
	 * ============================================================================================ */
	HSE_Example_NvmKeyEncryptResponse = HSE_AesEncryptNvm(HSE_Example_PlainText, HSE_Example_NvmCipherText, sizeof(HSE_Example_PlainText));

	HSE_Example_NvmKeyFlashWriteStatus = HSE_Example_FlashWrite(FLASH_NVM_KEY_DATA_ADDR, FLASH_NVM_KEY_DATA_VIRTUAL_SECTOR,
	                                                             HSE_Example_NvmCipherText, sizeof(HSE_Example_NvmCipherText));

	HSE_Example_NvmKeyFlashReadStatus = HSE_Example_FlashRead(FLASH_NVM_KEY_DATA_ADDR, HSE_Example_NvmCipherFromFlash,
	                                                           sizeof(HSE_Example_NvmCipherFromFlash));

	HSE_Example_NvmKeyDecryptResponse = HSE_AesDecryptNvm(HSE_Example_NvmCipherFromFlash, HSE_Example_NvmDecryptedText,
	                                                       sizeof(HSE_Example_NvmCipherFromFlash));

	HSE_Example_NvmKeyRoundTripMatch = (0 == memcmp(HSE_Example_PlainText, HSE_Example_NvmDecryptedText, sizeof(HSE_Example_PlainText)));

	/* ============================================================================================
	 *  Ephemeral generated (RAM) key path - only valid within this same boot, see @details above
	 * ============================================================================================ */
	HSE_Example_GeneratedKeyEncryptResponse = HSE_AesEncryptGenerated(HSE_Example_PlainText, HSE_Example_GeneratedCipherText, sizeof(HSE_Example_PlainText));

	HSE_Example_GeneratedKeyFlashWriteStatus = HSE_Example_FlashWrite(FLASH_GENERATED_KEY_DATA_ADDR, FLASH_GENERATED_KEY_DATA_VIRTUAL_SECTOR,
	                                                                   HSE_Example_GeneratedCipherText, sizeof(HSE_Example_GeneratedCipherText));

	HSE_Example_GeneratedKeyFlashReadStatus = HSE_Example_FlashRead(FLASH_GENERATED_KEY_DATA_ADDR, HSE_Example_GeneratedCipherFromFlash,
	                                                                 sizeof(HSE_Example_GeneratedCipherFromFlash));

	HSE_Example_GeneratedKeyDecryptResponse = HSE_AesDecryptGenerated(HSE_Example_GeneratedCipherFromFlash, HSE_Example_GeneratedDecryptedText,
	                                                                   sizeof(HSE_Example_GeneratedCipherFromFlash));

	HSE_Example_GeneratedKeyRoundTripMatch = (0 == memcmp(HSE_Example_PlainText, HSE_Example_GeneratedDecryptedText, sizeof(HSE_Example_PlainText)));

	/* ============================================================================================
	 *  NVM key, CTR mode - 30-byte message, not a multiple of 16, no padding needed
	 * ============================================================================================ */
	HSE_Example_CtrIvGenerated = HSE_GetTRNG(HSE_Example_CtrIv, sizeof(HSE_Example_CtrIv));

	HSE_Example_CtrEncryptResponse = HSE_AesCtrEncryptNvm(HSE_Example_CtrIv, HSE_Example_CtrPlainText, HSE_Example_CtrCipherText, sizeof(HSE_Example_CtrPlainText));

	/* Build the on-flash blob: [16-byte IV][30-byte ciphertext][2 bytes zero pad to reach the
	   8-byte write-length alignment C40_Ip requires]. HSE_Example_CtrFlashBlob is already
	   zero-initialized, so only the IV and ciphertext need to be copied in. */
	memcpy(&HSE_Example_CtrFlashBlob[0], HSE_Example_CtrIv, sizeof(HSE_Example_CtrIv));
	memcpy(&HSE_Example_CtrFlashBlob[sizeof(HSE_Example_CtrIv)], HSE_Example_CtrCipherText, sizeof(HSE_Example_CtrCipherText));

	HSE_Example_CtrFlashWriteStatus = HSE_Example_FlashWrite(FLASH_CTR_DATA_ADDR, FLASH_CTR_DATA_VIRTUAL_SECTOR,
	                                                          HSE_Example_CtrFlashBlob, sizeof(HSE_Example_CtrFlashBlob));

	HSE_Example_CtrFlashReadStatus = HSE_Example_FlashRead(FLASH_CTR_DATA_ADDR, HSE_Example_CtrFlashBlob, sizeof(HSE_Example_CtrFlashBlob));

	/* Split the blob back into IV + ciphertext, then decrypt using that same IV */
	HSE_Example_CtrDecryptResponse = HSE_AesCtrDecryptNvm(&HSE_Example_CtrFlashBlob[0], &HSE_Example_CtrFlashBlob[sizeof(HSE_Example_CtrIv)],
	                                                       HSE_Example_CtrDecryptedText, sizeof(HSE_Example_CtrDecryptedText));

	HSE_Example_CtrRoundTripMatch = (0 == memcmp(HSE_Example_CtrPlainText, HSE_Example_CtrDecryptedText, sizeof(HSE_Example_CtrPlainText)));
}

#ifdef __cplusplus
}
#endif

/** @} */
