/**
 ******************************************************************************
 * @file     HSE_SecureBoot.c
 * @brief    Secure boot (SMR install/verify) proof-of-concept, staged per
 *           SECURE_BOOT_PLAN.md. Stage 1a: prepare a throwaway 20KB test
 *           region in Data Flash to exercise the SMR pipeline against,
 *           before it's ever pointed at the real application image.
 * @location /test/src/HSE_SecureBoot.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifdef __cplusplus
extern "C"
{
#endif

#include "HSE_SecureBoot.h"
#include "C40_Ip_Cfg.h"
#include "string.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
/* Data Flash sectors 0-2 (0x10000000/0x10002000/0x10004000) are already used by
   HSE_FlashStorage_Example.c. This is the next free span: sectors 3-5, 3 x 8KB = 24KB of
   capacity, of which 20KB is actually used - kept as a separate bank from Program Flash so this
   test data can be deliberately corrupted later (SMR negative-control test) with zero risk to the
   real running application. See SECURE_BOOT_PLAN.md for the full rationale. */
#define SMR_TEST_DATA_ADDR          (0x10006000U)                   /* Data Flash sector 3 */
#define SMR_TEST_DATA_FIRST_SECTOR  (C40_DATA_ARRAY_0_BLOCK_2_S003)
#define SMR_TEST_DATA_NUM_SECTORS   (3U)                             /* covers sectors 3, 4, 5 */
#define SMR_TEST_DATA_SIZE          (20480U)                         /* 20KB */

/* C40_Ip_MainInterfaceWrite caps out at 128 bytes per call, on an 8-byte-aligned address/length.
   20480 / 128 = 160 exactly, so the whole region is filled with no partial last chunk. */
#define FLASH_WRITE_CHUNK_SIZE  (128U)

/* Single application core in this project - no sector-lock contention to arbitrate */
#define FLASH_DOMAIN_ID_U8  (0U)

/* RAM catalog group 0 already has 4 slots, only slots 0 (HSE_Main.c demo key) and 1 (generated
   key) are used today - slot 2 is free. NVM catalog group 0 slot 0 is used by HSE_Main.c's
   persistent demo key - slot 1 is free. No catalog re-format needed. See SECURE_BOOT_PLAN.md. */
#define AES_SMR_SIGN_RAM_KEY_HANDLE    GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM, 0, 2)
#define AES_SMR_VERIFY_NVM_KEY_HANDLE  GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_NVM, 0, 1)

/* SMR#0 is special-cased for SHE-boot/BOOT_MAC_KEY - use #1 for this demo entry */
#define SMR_TEST_ENTRY_INDEX  (1U)

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/
C40_Ip_StatusType HSE_SecureBootTestDataWriteStatus = C40_IP_STATUS_ERROR;
bool              HSE_SecureBootTestDataVerified    = false;

hseSrvResponse_t HSE_ImportSmrSignKeyRamResponse    = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_ImportSmrVerifyKeyNvmResponse  = HSE_SRV_RSP_GENERAL_ERROR;
hseKeyInfo_t     HSE_SmrVerifyKeyInfo;
hseSrvResponse_t HSE_GetSmrVerifyKeyInfoResponse    = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_ComputeSmrInstallTagResponse   = HSE_SRV_RSP_GENERAL_ERROR;
uint8_t          HSE_SmrInstallTag[16]              = {0};
uint32_t         HSE_SmrInstallTagLength            = 0U;
hseSrvResponse_t HSE_EraseSmrSignKeyRamResponse     = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_InstallSmrEntryResponse        = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_VerifySmrEntryResponse         = HSE_SRV_RSP_GENERAL_ERROR;

C40_Ip_StatusType HSE_SecureBootCorruptTestDataStatus       = C40_IP_STATUS_ERROR;
hseSrvResponse_t  HSE_VerifySmrEntryAfterCorruptionResponse = HSE_SRV_RSP_GENERAL_ERROR;
bool              HSE_SecureBootNegativeControlPassed       = false;

/* Raw value of the transient sign / persistent verify key - both must hold the same value, since
   CMAC uses the same secret to generate and check the tag. Deliberately separate from
   HSE_Main.c's aes128_key0, so this demo's key lifecycle stays independent of the crypto demo's. */
static const uint8_t aes128_smrKey[16] = {0x53, 0x4D, 0x52, 0x2D, 0x54, 0x45, 0x53, 0x54,
                                           0x2D, 0x4B, 0x45, 0x59, 0x2D, 0x30, 0x31, 0x21};

/* This translation unit obtains its own MU channels via Hse_Ip_GetFreeChannel(), so it needs its
   own host-side request/descriptor storage - same shape as the arrays in HSE_Main.c, but private
   to this file since C statics aren't shared across translation units. */
static Hse_Ip_ReqType    HseIp_aRequest[HSE_IP_NUM_OF_CHANNELS_PER_MU];
static hseSrvDescriptor_t Hse_aSrvDescriptor[HSE_IP_NUM_OF_CHANNELS_PER_MU];

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
static C40_Ip_StatusType HSE_SecureBoot_EraseTestRegion(void);
static C40_Ip_StatusType HSE_SecureBoot_WriteTestPattern(void);
static bool              HSE_SecureBoot_VerifyTestPattern(void);

/*!
 * @brief       Erases all sectors covering SMR_TEST_DATA_ADDR..+SMR_TEST_DATA_SIZE.
 * @details     Each C40_Ip erase call covers exactly one virtual sector, so the 3 sectors backing
 *              the 20KB region are erased one at a time, busy-waiting on the async status after
 *              each - same pattern as HSE_FlashStorage_Example.c's flash helpers.
 *
 * @return      C40_Ip_StatusType
 */
static C40_Ip_StatusType HSE_SecureBoot_EraseTestRegion(void)
{
	C40_Ip_StatusType status = C40_IP_STATUS_ERROR;
	uint8_t sectorOffset;

	for (sectorOffset = 0U; sectorOffset < SMR_TEST_DATA_NUM_SECTORS; sectorOffset++)
	{
		status = C40_Ip_MainInterfaceSectorErase((C40_Ip_VirtualSectorsType)(SMR_TEST_DATA_FIRST_SECTOR + sectorOffset), FLASH_DOMAIN_ID_U8);
		if (C40_IP_STATUS_SUCCESS != status)
		{
			return status;
		}
		do
		{
			status = C40_Ip_MainInterfaceSectorEraseStatus();
		} while (C40_IP_STATUS_BUSY == status);
		if (C40_IP_STATUS_SUCCESS != status)
		{
			return status;
		}
	}

	return status;
}

/*!
 * @brief       Fills SMR_TEST_DATA_ADDR..+SMR_TEST_DATA_SIZE with a repeating 0x00-0xFF
 *              byte-counter pattern (byte[i] = i & 0xFF), standing in for "application data".
 * @details     Generates and writes one 128-byte chunk at a time rather than holding the whole
 *              20KB in RAM at once - int_sram is only 32KB total, shared with everything else.
 *
 * @return      C40_Ip_StatusType
 */
static C40_Ip_StatusType HSE_SecureBoot_WriteTestPattern(void)
{
	uint8_t chunk[FLASH_WRITE_CHUNK_SIZE];
	uint32_t offset;
	uint32_t i;
	C40_Ip_StatusType status = C40_IP_STATUS_ERROR;

	for (offset = 0U; offset < SMR_TEST_DATA_SIZE; offset += FLASH_WRITE_CHUNK_SIZE)
	{
		for (i = 0U; i < FLASH_WRITE_CHUNK_SIZE; i++)
		{
			chunk[i] = (uint8_t)((offset + i) & 0xFFU);
		}

		status = C40_Ip_MainInterfaceWrite(SMR_TEST_DATA_ADDR + offset, FLASH_WRITE_CHUNK_SIZE, chunk, FLASH_DOMAIN_ID_U8);
		if (C40_IP_STATUS_SUCCESS != status)
		{
			return status;
		}
		do
		{
			status = C40_Ip_MainInterfaceWriteStatus();
		} while (C40_IP_STATUS_BUSY == status);
		if (C40_IP_STATUS_SUCCESS != status)
		{
			return status;
		}
	}

	return status;
}

/*!
 * @brief       Reads back SMR_TEST_DATA_ADDR..+SMR_TEST_DATA_SIZE and confirms it matches the
 *              same 0x00-0xFF byte-counter pattern written by HSE_SecureBoot_WriteTestPattern().
 * @details     Same chunked approach as the write side - one 128-byte buffer reused per chunk.
 *
 * @return      true if every byte in the region matches the expected pattern
 */
static bool HSE_SecureBoot_VerifyTestPattern(void)
{
	uint8_t chunk[FLASH_WRITE_CHUNK_SIZE];
	uint32_t offset;
	uint32_t i;

	for (offset = 0U; offset < SMR_TEST_DATA_SIZE; offset += FLASH_WRITE_CHUNK_SIZE)
	{
		if (C40_IP_STATUS_SUCCESS != C40_Ip_Read(SMR_TEST_DATA_ADDR + offset, FLASH_WRITE_CHUNK_SIZE, chunk))
		{
			return false;
		}

		for (i = 0U; i < FLASH_WRITE_CHUNK_SIZE; i++)
		{
			if (chunk[i] != (uint8_t)((offset + i) & 0xFFU))
			{
				return false;
			}
		}
	}

	return true;
}

/*!
 * @brief       Erases, fills, and verifies the 20KB Stage 1a test region in Data Flash.
 *              Leaves HSE_SecureBootTestDataWriteStatus / HSE_SecureBootTestDataVerified for
 *              inspection. Once this reads back clean, the next step is computing a CMAC over
 *              this region and installing it as an SMR entry (see the functions below).
 */
void HSE_SecureBoot_PrepareTestData(void)
{
	HSE_SecureBootTestDataWriteStatus = HSE_SecureBoot_EraseTestRegion();
	if (C40_IP_STATUS_SUCCESS == HSE_SecureBootTestDataWriteStatus)
	{
		HSE_SecureBootTestDataWriteStatus = HSE_SecureBoot_WriteTestPattern();
	}

	HSE_SecureBootTestDataVerified = (C40_IP_STATUS_SUCCESS == HSE_SecureBootTestDataWriteStatus) && HSE_SecureBoot_VerifyTestPattern();
}

/*!
 * @brief       Imports aes128_smrKey as a transient AES-128 key (SIGN | VERIFY) into the RAM key
 *              catalog slot identified by AES_SMR_SIGN_RAM_KEY_HANDLE. Used only long enough to
 *              produce the SMR installation tag (HSE_ComputeSmrInstallTag()), then erased by
 *              HSE_EraseSmrSignKey_Ram() - no long-lived key on the device should carry
 *              HSE_KF_USAGE_SIGN.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_ImportSmrSignKey_Ram(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseImportKeySrv_t*  pImportKeyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
	static hseKeyInfo_t KeyInfo;

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	KeyInfo.keyFlags  = (HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY);
	KeyInfo.keyBitLen = 128U;
	KeyInfo.keyType   = HSE_KEY_TYPE_AES;

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

	pHseSrvDescriptor->srvId                  = HSE_SRV_ID_IMPORT_KEY;
	pImportKeyReq->targetKeyHandle            = AES_SMR_SIGN_RAM_KEY_HANDLE;
	pImportKeyReq->pKeyInfo                   = (HOST_ADDR)&KeyInfo;
	pImportKeyReq->pKey[0]                    = 0U;
	pImportKeyReq->pKey[1]                    = 0U;
	pImportKeyReq->pKey[2]                    = (HOST_ADDR)aes128_smrKey;
	pImportKeyReq->keyLen[0]                  = 0U;
	pImportKeyReq->keyLen[1]                  = 0U;
	pImportKeyReq->keyLen[2]                  = sizeof(aes128_smrKey);
	pImportKeyReq->cipher.cipherKeyHandle      = HSE_INVALID_KEY_HANDLE;
	pImportKeyReq->keyContainer.authKeyHandle  = HSE_INVALID_KEY_HANDLE;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Reads back the stored properties of AES_SMR_VERIFY_NVM_KEY_HANDLE (flags, key
 *              type, bit length) without exposing the raw key value. HSE_SRV_RSP_KEY_EMPTY means
 *              the slot has never been provisioned yet. Result is left in HSE_SmrVerifyKeyInfo.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_GetSmrVerifyKeyInfo(void)
{
	hseSrvDescriptor_t*   pHseSrvDescriptor;
	hseGetKeyInfoSrv_t*   pGetKeyInfoReq;
	hseSrvResponse_t      RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t               u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	memset(&HSE_SmrVerifyKeyInfo, 0, sizeof(HSE_SmrVerifyKeyInfo));

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pGetKeyInfoReq = &(pHseSrvDescriptor->hseSrv.getKeyInfoReq);

	pHseSrvDescriptor->srvId  = HSE_SRV_ID_GET_KEY_INFO;
	pGetKeyInfoReq->keyHandle = AES_SMR_VERIFY_NVM_KEY_HANDLE;
	pGetKeyInfoReq->pKeyInfo  = (HOST_ADDR)&HSE_SmrVerifyKeyInfo;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Imports aes128_smrKey (the SAME raw value as the transient sign key - CMAC
 *              verify must use the same secret that produced the tag) as a VERIFY-only AES-128
 *              key into the NVM key catalog slot identified by AES_SMR_VERIFY_NVM_KEY_HANDLE.
 *              This key persists across reset and is the SMR's actual authKeyHandle.
 * @details     Guarded the same way as HSE_ImportNvmAESKey(): checks HSE_GetSmrVerifyKeyInfo()
 *              first and skips the import if the slot is already provisioned from an earlier boot.
 *
 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK also covers the "already provisioned" case.
 */
hseSrvResponse_t HSE_ImportSmrVerifyKey_Nvm(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseImportKeySrv_t*  pImportKeyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel;
	static hseKeyInfo_t KeyInfo;

	HSE_GetSmrVerifyKeyInfoResponse = HSE_GetSmrVerifyKeyInfo();
	if (HSE_SRV_RSP_OK == HSE_GetSmrVerifyKeyInfoResponse)
	{
		/* Already provisioned by an earlier boot - nothing to do. */
		return HSE_SRV_RSP_OK;
	}

	u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	KeyInfo.keyFlags   = HSE_KF_USAGE_VERIFY;
	KeyInfo.keyBitLen  = 128U;
	KeyInfo.keyCounter = 0U;
	KeyInfo.keyType    = HSE_KEY_TYPE_AES;

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

	pHseSrvDescriptor->srvId                  = HSE_SRV_ID_IMPORT_KEY;
	pImportKeyReq->targetKeyHandle            = AES_SMR_VERIFY_NVM_KEY_HANDLE;
	pImportKeyReq->pKeyInfo                   = (HOST_ADDR)&KeyInfo;
	pImportKeyReq->pKey[0]                    = 0U;
	pImportKeyReq->pKey[1]                    = 0U;
	pImportKeyReq->pKey[2]                    = (HOST_ADDR)aes128_smrKey;
	pImportKeyReq->keyLen[0]                  = 0U;
	pImportKeyReq->keyLen[1]                  = 0U;
	pImportKeyReq->keyLen[2]                  = sizeof(aes128_smrKey);
	pImportKeyReq->cipher.cipherKeyHandle      = HSE_INVALID_KEY_HANDLE;
	pImportKeyReq->keyContainer.authKeyHandle  = HSE_INVALID_KEY_HANDLE;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	/* Refresh the cached key info now that the key has (hopefully) been written */
	HSE_GetSmrVerifyKeyInfoResponse = HSE_GetSmrVerifyKeyInfo();

	return RetVal;
}

/*!
 * @brief       Computes a CMAC (AES-128) tag over [pRegionStart, pRegionStart + regionLen) using
 *              the transient RAM sign key (AES_SMR_SIGN_RAM_KEY_HANDLE), one-pass / generate
 *              mode. Result (and its length) is left in HSE_SmrInstallTag / HSE_SmrInstallTagLength
 *              for HSE_InstallSmrEntry() to consume.
 * @details     pTagLength follows HSE's "pointer-to-length" pattern (same as HSE_genreateSHA()'s
 *              p_length): on input it holds the buffer capacity, on output the actual tag length.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_ComputeSmrInstallTag(const uint8_t *pRegionStart, uint32_t regionLen)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseMacSrv_t*        pMacReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	HSE_SmrInstallTagLength = sizeof(HSE_SmrInstallTag);

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pMacReq = &(pHseSrvDescriptor->hseSrv.macReq);

	pHseSrvDescriptor->srvId                    = HSE_SRV_ID_MAC;
	pMacReq->accessMode                         = HSE_ACCESS_MODE_ONE_PASS;
	pMacReq->streamId                           = 0U;
	pMacReq->authDir                            = HSE_AUTH_DIR_GENERATE;
	pMacReq->sgtOption                          = HSE_SGT_OPTION_NONE;
	pMacReq->macScheme.macAlgo                  = HSE_MAC_ALGO_CMAC;
	pMacReq->macScheme.sch.cmac.cipherAlgo      = HSE_CIPHER_ALGO_AES;
	pMacReq->keyHandle                          = AES_SMR_SIGN_RAM_KEY_HANDLE;
	pMacReq->inputLength                        = regionLen;
	pMacReq->pInput                             = (HOST_ADDR)pRegionStart;
	pMacReq->pTagLength                         = (HOST_ADDR)&HSE_SmrInstallTagLength;
	pMacReq->pTag                               = (HOST_ADDR)HSE_SmrInstallTag;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Erases the transient RAM sign key (AES_SMR_SIGN_RAM_KEY_HANDLE) - same shape as
 *              HSE_EraseNvmAesKey(), just targeting the RAM catalog instead.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_EraseSmrSignKey_Ram(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseEraseKeySrv_t*   pEraseKeyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pEraseKeyReq = &(pHseSrvDescriptor->hseSrv.eraseKeyReq);

	pHseSrvDescriptor->srvId      = HSE_SRV_ID_ERASE_KEY;
	pEraseKeyReq->keyHandle       = AES_SMR_SIGN_RAM_KEY_HANDLE;
	pEraseKeyReq->eraseKeyOptions = HSE_ERASE_KEYGROUP_ON_MU_IF;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Installs an SMR entry at entryIndex covering [pRegionStart, pRegionStart +
 *              regionLen), authenticated with AES_SMR_VERIFY_NVM_KEY_HANDLE (CMAC/AES-128) and
 *              the tag already computed by HSE_ComputeSmrInstallTag() (HSE_SmrInstallTag /
 *              HSE_SmrInstallTagLength). In-place auth (pSmrDest = 0, no copy), on-demand only
 *              (checkPeriod = 0 - no background periodic re-check), version tracking unused.
 * @details     Installing this SMR does not, by itself, change boot behavior - it only teaches
 *              HSE how to check the region on request (see HSE_VerifySmrEntryOnDemand()).
 *              Linking it to actual core-release enforcement is Phase 2, not part of this file.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_InstallSmrEntry(uint8_t entryIndex, const uint8_t *pRegionStart, uint32_t regionLen)
{
	hseSrvDescriptor_t*      pHseSrvDescriptor;
	hseSmrEntryInstallSrv_t* pSmrInstallReq;
	hseSrvResponse_t         RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t                  u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
	static hseSmrEntry_t     SmrEntry;

	memset(&SmrEntry, 0, sizeof(SmrEntry));
	SmrEntry.pSmrSrc                        = (uint32_t)pRegionStart;
	SmrEntry.smrSize                        = regionLen;
	SmrEntry.pSmrDest                       = 0U;
	SmrEntry.configFlags                    = HSE_SMR_CFG_FLAG_INSTALL_AUTH;
	SmrEntry.checkPeriod                    = 0U;
	SmrEntry.authKeyHandle                  = AES_SMR_VERIFY_NVM_KEY_HANDLE;
	SmrEntry.authScheme.macScheme.macAlgo             = HSE_MAC_ALGO_CMAC;
	SmrEntry.authScheme.macScheme.sch.cmac.cipherAlgo = HSE_CIPHER_ALGO_AES;
	SmrEntry.pInstAuthTag[0]                = (uint32_t)HSE_SmrInstallTag;
	SmrEntry.versionOffset                  = HSE_SMR_VERSION_NOT_USED;

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pSmrInstallReq = &(pHseSrvDescriptor->hseSrv.smrEntryInstallReq);

	pHseSrvDescriptor->srvId       = HSE_SRV_ID_SMR_ENTRY_INSTALL;
	pSmrInstallReq->accessMode     = HSE_ACCESS_MODE_ONE_PASS;
	pSmrInstallReq->entryIndex     = entryIndex;
	pSmrInstallReq->pSmrEntry      = (HOST_ADDR)&SmrEntry;
	pSmrInstallReq->pSmrData       = (HOST_ADDR)pRegionStart;
	pSmrInstallReq->smrDataLength  = regionLen;
	pSmrInstallReq->pAuthTag[0]    = (HOST_ADDR)HSE_SmrInstallTag;
	pSmrInstallReq->authTagLength[0] = (uint16_t)HSE_SmrInstallTagLength;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Triggers on-demand verification of the SMR entry at entryIndex and returns the
 *              raw response - HSE_SRV_RSP_OK on match, HSE_SRV_RSP_VERIFY_FAILED if the region no
 *              longer matches the tag installed by HSE_InstallSmrEntry().
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_VerifySmrEntryOnDemand(uint8_t entryIndex)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseSmrVerifySrv_t*  pSmrVerifyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pSmrVerifyReq = &(pHseSrvDescriptor->hseSrv.smrVerifyReq);

	pHseSrvDescriptor->srvId  = HSE_SRV_ID_SMR_VERIFY;
	pSmrVerifyReq->entryIndex = entryIndex;
	pSmrVerifyReq->reserved   = 0U;
	pSmrVerifyReq->options    = HSE_SMR_VERIFICATION_OPTION_NONE;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Plan step 7: deliberately corrupts the first 128-byte chunk of the Stage 1a test
 *              region with all-zero bytes (a valid NOR flash write with no erase cycle needed,
 *              since it only clears bits) and re-runs on-demand verification. Confirms the check
 *              now reports HSE_SRV_RSP_VERIFY_FAILED - proving it isn't a rubber stamp.
 */
void HSE_SecureBoot_NegativeControlTest(uint8_t entryIndex)
{
	uint8_t zeroChunk[FLASH_WRITE_CHUNK_SIZE] = {0};

	HSE_SecureBootCorruptTestDataStatus = C40_Ip_MainInterfaceWrite(SMR_TEST_DATA_ADDR, FLASH_WRITE_CHUNK_SIZE, zeroChunk, FLASH_DOMAIN_ID_U8);
	if (C40_IP_STATUS_SUCCESS == HSE_SecureBootCorruptTestDataStatus)
	{
		do
		{
			HSE_SecureBootCorruptTestDataStatus = C40_Ip_MainInterfaceWriteStatus();
		} while (C40_IP_STATUS_BUSY == HSE_SecureBootCorruptTestDataStatus);
	}

	HSE_VerifySmrEntryAfterCorruptionResponse = HSE_VerifySmrEntryOnDemand(entryIndex);
	HSE_SecureBootNegativeControlPassed = (HSE_SRV_RSP_VERIFY_FAILED == HSE_VerifySmrEntryAfterCorruptionResponse);
}

/*!
 * @brief       Single orchestrating entry point for the Stage 1a demo (plan steps 1-7, see
 *              SECURE_BOOT_PLAN.md): prepares the throwaway test region, imports both keys,
 *              computes and installs the SMR entry, erases the transient sign key, verifies
 *              on-demand (expect PASS), then runs the negative-control corruption test (expect
 *              FAIL). Nothing here changes boot behavior - the SMR is not yet linked to a Core
 *              Reset entry (Phase 2, a separate, higher-risk step not implemented here).
 */
void HSE_SecureBoot_Phase1_Demo(void)
{
	HSE_SecureBoot_PrepareTestData();
	if (!HSE_SecureBootTestDataVerified)
	{
		return;
	}

	HSE_ImportSmrSignKeyRamResponse = HSE_ImportSmrSignKey_Ram();
	HSE_ComputeSmrInstallTagResponse = HSE_ComputeSmrInstallTag((const uint8_t *)SMR_TEST_DATA_ADDR, SMR_TEST_DATA_SIZE);
	HSE_ImportSmrVerifyKeyNvmResponse = HSE_ImportSmrVerifyKey_Nvm();
	HSE_InstallSmrEntryResponse = HSE_InstallSmrEntry(SMR_TEST_ENTRY_INDEX, (const uint8_t *)SMR_TEST_DATA_ADDR, SMR_TEST_DATA_SIZE);
	HSE_EraseSmrSignKeyRamResponse = HSE_EraseSmrSignKey_Ram();
	HSE_VerifySmrEntryResponse = HSE_VerifySmrEntryOnDemand(SMR_TEST_ENTRY_INDEX);

	HSE_SecureBoot_NegativeControlTest(SMR_TEST_ENTRY_INDEX);
}

#ifdef __cplusplus
}
#endif

/** @} */
