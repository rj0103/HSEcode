/**
 ******************************************************************************
 * @file     HSE_Mac_Ecc_Example.c
 * @brief    Example: CMAC (AES-128) tag generate/verify, and ECC (secp256r1)
 *           key-pair generate/sign/verify, each followed by a Data Flash
 *           storage-integrity check (write, read back, re-verify against the
 *           flash-read data).
 * @location /test/src/HSE_Mac_Ecc_Example.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifdef __cplusplus
extern "C"
{
#endif

#include "HSE_Mac_Ecc_Example.h"
#include "C40_Ip_Cfg.h"
#include "string.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
/* RAM catalog group 0 slot 3 is the next spare AES slot (0: HSE_Main.c demo key, 1: generated
   key, 2: transient SMR sign key). Groups 1/2 are new - added to Hse_aRamKeyCatalog in
   HSE_Main.c specifically for this example (see the comments there on the reformat requirement). */
#define AES_MAC_RAM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM, 0, 3)
#define ECC_PAIR_RAM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM, 1, 0)
#define ECC_PUB_RAM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM, 2, 0)

#define MAC_MESSAGE_SIZE (32U)
#define MAC_TAG_SIZE (16U)
#define MAC_FLASH_BLOB_SIZE (MAC_MESSAGE_SIZE + MAC_TAG_SIZE) /* 48B  */
#define MAC_FLASH_ADDR (0x1000C000U)						  /* Data Flash sector 6 */
#define MAC_FLASH_VIRTUAL_SECTOR (C40_DATA_ARRAY_0_BLOCK_2_S006)

#define ECC_CURVE_BYTE_LEN (32U)				  /* secp256r1: 256-bit curve, 32B components */
#define ECC_PUBKEY_SIZE (2U * ECC_CURVE_BYTE_LEN) /* 64B  */
#define ECC_MESSAGE_SIZE (32U)
#define ECC_FLASH_BLOB_SIZE (ECC_MESSAGE_SIZE + ECC_PUBKEY_SIZE + ECC_CURVE_BYTE_LEN + ECC_CURVE_BYTE_LEN) /* 160B */
#define ECC_FLASH_ADDR (0x1000E000U)																	   /* Data Flash sector 7 */
#define ECC_FLASH_VIRTUAL_SECTOR (C40_DATA_ARRAY_0_BLOCK_2_S007)

/* Blob layout offsets */
#define MAC_BLOB_OFFSET_MESSAGE (0U)
#define MAC_BLOB_OFFSET_TAG (MAC_MESSAGE_SIZE)

#define ECC_BLOB_OFFSET_MESSAGE (0U)
#define ECC_BLOB_OFFSET_PUBKEY (ECC_MESSAGE_SIZE)
#define ECC_BLOB_OFFSET_R (ECC_BLOB_OFFSET_PUBKEY + ECC_PUBKEY_SIZE)
#define ECC_BLOB_OFFSET_S (ECC_BLOB_OFFSET_R + ECC_CURVE_BYTE_LEN)

/* C40_Ip_MainInterfaceWrite caps out at 128 bytes per call, on an 8-byte-aligned address/length -
   both blobs below are written in one or more such chunks. */
#define FLASH_WRITE_CHUNK_SIZE (128U)

/* Single application core in this project - no sector-lock contention to arbitrate */
#define FLASH_DOMAIN_ID_U8 (0U)

	/*==================================================================================================
	*                                        GLOBAL VARIABLES
	==================================================================================================*/
	hseSrvResponse_t HSE_Mac_ImportKeyResponse = HSE_SRV_RSP_GENERAL_ERROR;
	hseSrvResponse_t HSE_Mac_GenerateTagResponse = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t HSE_Mac_Tag[16] = {0};
	uint32_t HSE_Mac_TagLength = 0U;
	hseSrvResponse_t HSE_Mac_VerifyTagResponse = HSE_SRV_RSP_GENERAL_ERROR;

	C40_Ip_StatusType HSE_Mac_FlashWriteStatus = C40_IP_STATUS_ERROR;
	C40_Ip_StatusType HSE_Mac_FlashReadStatus = C40_IP_STATUS_ERROR;
	uint8_t HSE_Mac_TagFromFlash[16] = {0};
	hseSrvResponse_t HSE_Mac_VerifyFromFlashResponse = HSE_SRV_RSP_GENERAL_ERROR;
	bool HSE_Mac_FlashRoundTripVerified = false;

	hseSrvResponse_t HSE_Ecc_GenerateKeyPairResponse = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t HSE_EccPublicKey[64] = {0};
	hseSrvResponse_t HSE_Ecc_SignResponse = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t HSE_EccSignatureR[32] = {0};
	uint8_t HSE_EccSignatureS[32] = {0};
	uint32_t HSE_EccSignatureRLength = 0U;
	uint32_t HSE_EccSignatureSLength = 0U;
	hseSrvResponse_t HSE_Ecc_VerifyResponse = HSE_SRV_RSP_GENERAL_ERROR;

	C40_Ip_StatusType HSE_Ecc_FlashWriteStatus = C40_IP_STATUS_ERROR;
	C40_Ip_StatusType HSE_Ecc_FlashReadStatus = C40_IP_STATUS_ERROR;
	hseSrvResponse_t HSE_Ecc_ImportPubKeyFromFlashResponse = HSE_SRV_RSP_GENERAL_ERROR;
	hseSrvResponse_t HSE_Ecc_VerifyFromFlashResponse = HSE_SRV_RSP_GENERAL_ERROR;
	bool HSE_Ecc_FlashRoundTripVerified = false;

	/*==================================================================================================
	*                                         LOCAL VARIABLES
	==================================================================================================*/
	/* Raw value of the MAC example's dedicated AES-128 key - separate from HSE_Main.c's demo keys
	   and HSE_SecureBoot.c's transient SMR sign key, so this example's key lifecycle stays independent. */
	static const uint8_t aes128_macKey[16] = {0x4D, 0x41, 0x43, 0x2D, 0x45, 0x58, 0x41, 0x4D,
											  0x50, 0x4C, 0x45, 0x2D, 0x4B, 0x45, 0x59, 0x21};

	/* Test message buffers - filled with a deterministic rotating-letter pattern at demo start
	   (see HSE_Mac_StorageCheckDemo() / HSE_Ecc_StorageCheckDemo()), not read from anywhere sensitive. */
	static uint8_t HSE_Mac_TestMessage[MAC_MESSAGE_SIZE] = {0};
	static uint8_t HSE_Ecc_TestMessage[ECC_MESSAGE_SIZE] = {0};

	/* Scratch buffers for the flash round-trip blobs (MAC: 48B, ECC: 160B) */
	static uint8_t HSE_Mac_FlashBlob[MAC_FLASH_BLOB_SIZE] = {0};
	static uint8_t HSE_Ecc_FlashBlob[ECC_FLASH_BLOB_SIZE] = {0};

	/* This translation unit obtains its own MU channels via Hse_Ip_GetFreeChannel(), so it needs its
	   own host-side request/descriptor storage - private to this file (see HSE_SecureBoot.c for the
	   same reasoning). */
	static Hse_Ip_ReqType HseIp_aRequest[HSE_IP_NUM_OF_CHANNELS_PER_MU];
	static hseSrvDescriptor_t Hse_aSrvDescriptor[HSE_IP_NUM_OF_CHANNELS_PER_MU];

	/*==================================================================================================
	*                                    LOCAL FUNCTION PROTOTYPES
	==================================================================================================*/
	static C40_Ip_StatusType HSE_MacEcc_FlashWriteBlob(uint32_t address, C40_Ip_VirtualSectorsType virtualSector,
													   const uint8_t *pData, uint32_t length);
	static C40_Ip_StatusType HSE_MacEcc_FlashRead(uint32_t address, uint8_t *pData, uint32_t length);

	/*!
	 * @brief       Erases virtualSector, then writes [pData, pData+length) starting at address, in
	 *              up-to-128-byte chunks. Unlocks the sector first (Data Flash sectors are
	 *              program/erase-lock protected by default) and re-locks it afterward regardless of
	 *              outcome.
	 *
	 * @return      C40_Ip_StatusType
	 */
	static C40_Ip_StatusType HSE_MacEcc_FlashWriteBlob(uint32_t address, C40_Ip_VirtualSectorsType virtualSector,
													   const uint8_t *pData, uint32_t length)
	{
		C40_Ip_StatusType status;
		uint32_t offset;
		uint32_t chunkLen;

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

		for (offset = 0U; (C40_IP_STATUS_SUCCESS == status) && (offset < length); offset += chunkLen)
		{
			chunkLen = (length - offset) > FLASH_WRITE_CHUNK_SIZE ? FLASH_WRITE_CHUNK_SIZE : (length - offset);

			status = C40_Ip_MainInterfaceWrite(address + offset, chunkLen, &pData[offset], FLASH_DOMAIN_ID_U8);
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
	static C40_Ip_StatusType HSE_MacEcc_FlashRead(uint32_t address, uint8_t *pData, uint32_t length)
	{
		return C40_Ip_Read(address, length, pData);
	}

	/*!
	 * @brief       Imports aes128_macKey as a dedicated AES-128 key (SIGN | VERIFY) into
	 *              AES_MAC_RAM_KEY_HANDLE, used only by this MAC example.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_Mac_ImportKey_Ram(void)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseImportKeySrv_t *pImportKeyReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		static hseKeyInfo_t KeyInfo;

		memset(&KeyInfo, 0, sizeof(KeyInfo));
		KeyInfo.keyFlags = (HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY);
		KeyInfo.keyBitLen = 128U;
		KeyInfo.keyType = HSE_KEY_TYPE_AES;

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_IMPORT_KEY;
		pImportKeyReq->targetKeyHandle = AES_MAC_RAM_KEY_HANDLE;
		pImportKeyReq->pKeyInfo = (HOST_ADDR)&KeyInfo;
		pImportKeyReq->pKey[0] = 0U;
		pImportKeyReq->pKey[1] = 0U;
		pImportKeyReq->pKey[2] = (HOST_ADDR)aes128_macKey;
		pImportKeyReq->keyLen[0] = 0U;
		pImportKeyReq->keyLen[1] = 0U;
		pImportKeyReq->keyLen[2] = sizeof(aes128_macKey);
		pImportKeyReq->cipher.cipherKeyHandle = HSE_INVALID_KEY_HANDLE;
		pImportKeyReq->keyContainer.authKeyHandle = HSE_INVALID_KEY_HANDLE;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Generates a CMAC (AES-128) tag over [pData, pData+dataLen) using
	 *              AES_MAC_RAM_KEY_HANDLE. Result left in HSE_Mac_Tag / HSE_Mac_TagLength.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_Mac_GenerateTag(const uint8_t *pData, uint32_t dataLen)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseMacSrv_t *pMacReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		HSE_Mac_TagLength = sizeof(HSE_Mac_Tag);

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pMacReq = &(pHseSrvDescriptor->hseSrv.macReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_MAC;
		pMacReq->accessMode = HSE_ACCESS_MODE_ONE_PASS;
		pMacReq->streamId = 0U;
		pMacReq->authDir = HSE_AUTH_DIR_GENERATE;
		pMacReq->sgtOption = HSE_SGT_OPTION_NONE;
		pMacReq->macScheme.macAlgo = HSE_MAC_ALGO_CMAC;
		pMacReq->macScheme.sch.cmac.cipherAlgo = HSE_CIPHER_ALGO_AES;
		pMacReq->keyHandle = AES_MAC_RAM_KEY_HANDLE;
		pMacReq->inputLength = dataLen;
		pMacReq->pInput = (HOST_ADDR)pData;
		pMacReq->pTagLength = (HOST_ADDR)&HSE_Mac_TagLength;
		pMacReq->pTag = (HOST_ADDR)HSE_Mac_Tag;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Verifies a CMAC (AES-128) tag over [pData, pData+dataLen) against [pTag, tagLen),
	 *              using AES_MAC_RAM_KEY_HANDLE.
	 *
	 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK on match, HSE_SRV_RSP_VERIFY_FAILED otherwise.
	 */
	hseSrvResponse_t HSE_Mac_VerifyTag(const uint8_t *pData, uint32_t dataLen, const uint8_t *pTag, uint32_t tagLen)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseMacSrv_t *pMacReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		static uint32_t VerifyTagLen;

		VerifyTagLen = tagLen;

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pMacReq = &(pHseSrvDescriptor->hseSrv.macReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_MAC;
		pMacReq->accessMode = HSE_ACCESS_MODE_ONE_PASS;
		pMacReq->streamId = 0U;
		pMacReq->authDir = HSE_AUTH_DIR_VERIFY;
		pMacReq->sgtOption = HSE_SGT_OPTION_NONE;
		pMacReq->macScheme.macAlgo = HSE_MAC_ALGO_CMAC;
		pMacReq->macScheme.sch.cmac.cipherAlgo = HSE_CIPHER_ALGO_AES;
		pMacReq->keyHandle = AES_MAC_RAM_KEY_HANDLE;
		pMacReq->inputLength = dataLen;
		pMacReq->pInput = (HOST_ADDR)pData;
		pMacReq->pTagLength = (HOST_ADDR)&VerifyTagLen;
		pMacReq->pTag = (HOST_ADDR)pTag;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Full MAC example: import key, generate a tag over a sample message, verify it
	 *              fresh, then round-trip [message][tag] through a dedicated Data Flash sector and
	 *              re-verify against the flash-read data.
	 */
	void HSE_Mac_StorageCheckDemo(void)
	{
		uint32_t i;

		for (i = 0U; i < MAC_MESSAGE_SIZE; i++)
		{
			HSE_Mac_TestMessage[i] = (uint8_t)('A' + (i % 26U));
		}

		HSE_Mac_ImportKeyResponse = HSE_Mac_ImportKey_Ram();
		HSE_Mac_GenerateTagResponse = HSE_Mac_GenerateTag(HSE_Mac_TestMessage, MAC_MESSAGE_SIZE);
		HSE_Mac_VerifyTagResponse = HSE_Mac_VerifyTag(HSE_Mac_TestMessage, MAC_MESSAGE_SIZE, HSE_Mac_Tag, HSE_Mac_TagLength);

		/* Build the flash blob: [message][tag] */
		memcpy(&HSE_Mac_FlashBlob[MAC_BLOB_OFFSET_MESSAGE], HSE_Mac_TestMessage, MAC_MESSAGE_SIZE);
		memcpy(&HSE_Mac_FlashBlob[MAC_BLOB_OFFSET_TAG], HSE_Mac_Tag, MAC_TAG_SIZE);

		HSE_Mac_FlashWriteStatus = HSE_MacEcc_FlashWriteBlob(MAC_FLASH_ADDR, MAC_FLASH_VIRTUAL_SECTOR, HSE_Mac_FlashBlob, MAC_FLASH_BLOB_SIZE);
		HSE_Mac_FlashReadStatus = HSE_MacEcc_FlashRead(MAC_FLASH_ADDR, HSE_Mac_FlashBlob, MAC_FLASH_BLOB_SIZE);

		memcpy(HSE_Mac_TagFromFlash, &HSE_Mac_FlashBlob[MAC_BLOB_OFFSET_TAG], MAC_TAG_SIZE);

		HSE_Mac_VerifyFromFlashResponse = HSE_Mac_VerifyTag(&HSE_Mac_FlashBlob[MAC_BLOB_OFFSET_MESSAGE], MAC_MESSAGE_SIZE,
															HSE_Mac_TagFromFlash, MAC_TAG_SIZE);

		HSE_Mac_FlashRoundTripVerified = (C40_IP_STATUS_SUCCESS == HSE_Mac_FlashWriteStatus) &&
										 (C40_IP_STATUS_SUCCESS == HSE_Mac_FlashReadStatus) &&
										 (HSE_SRV_RSP_OK == HSE_Mac_VerifyFromFlashResponse);
	}

	/*!
	 * @brief       Generates a secp256r1 ECC key pair into ECC_PAIR_RAM_KEY_HANDLE (SIGN | VERIFY),
	 *              capturing the public key (X || Y, 64 bytes) into HSE_EccPublicKey.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_Ecc_GenerateKeyPair(void)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseKeyGenerateSrv_t *pKeyGenReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		memset(HSE_EccPublicKey, 0, sizeof(HSE_EccPublicKey));

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pKeyGenReq = &(pHseSrvDescriptor->hseSrv.keyGenReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_KEY_GENERATE;
		pKeyGenReq->targetKeyHandle = ECC_PAIR_RAM_KEY_HANDLE;
		pKeyGenReq->keyInfo.keyFlags = (HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY);
		pKeyGenReq->keyInfo.keyBitLen = HSE_KEY256_BITS;
		pKeyGenReq->keyInfo.keyType = HSE_KEY_TYPE_ECC_PAIR;
		pKeyGenReq->keyInfo.specific.eccCurveId = HSE_EC_SEC_SECP256R1;
		pKeyGenReq->keyGenScheme = HSE_KEY_GEN_ECC_KEY_PAIR;
		pKeyGenReq->sch.eccKey.pPubKey = (HOST_ADDR)HSE_EccPublicKey;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       ECDSA-SHA256 signs [pMessage, pMessage+msgLen) using ECC_PAIR_RAM_KEY_HANDLE's
	 *              private component. Result (r,s) left in HSE_EccSignatureR/S and their lengths.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_Ecc_SignMessage(const uint8_t *pMessage, uint32_t msgLen)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseSignSrv_t *pSignReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		HSE_EccSignatureRLength = sizeof(HSE_EccSignatureR);
		HSE_EccSignatureSLength = sizeof(HSE_EccSignatureS);

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pSignReq = &(pHseSrvDescriptor->hseSrv.signReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_SIGN;
		pSignReq->accessMode = HSE_ACCESS_MODE_ONE_PASS;
		pSignReq->streamId = 0U;
		pSignReq->authDir = HSE_AUTH_DIR_GENERATE;
		pSignReq->bInputIsHashed = FALSE;
		pSignReq->signScheme.signSch = HSE_SIGN_ECDSA;
		pSignReq->signScheme.sch.ecdsa.hashAlgo = HSE_HASH_ALGO_SHA2_256;
		pSignReq->keyHandle = ECC_PAIR_RAM_KEY_HANDLE;
		pSignReq->sgtOption = HSE_SGT_OPTION_NONE;
		pSignReq->inputLength = msgLen;
		pSignReq->pInput = (HOST_ADDR)pMessage;
		pSignReq->pSignatureLength[0] = (HOST_ADDR)&HSE_EccSignatureRLength;
		pSignReq->pSignatureLength[1] = (HOST_ADDR)&HSE_EccSignatureSLength;
		pSignReq->pSignature[0] = (HOST_ADDR)HSE_EccSignatureR;
		pSignReq->pSignature[1] = (HOST_ADDR)HSE_EccSignatureS;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       ECDSA-SHA256 verifies [pMessage, pMessage+msgLen) against (pR,rLen)/(pS,sLen)
	 *              using verifyKeyHandle's public component - either the original key pair handle or
	 *              a standalone public key imported via HSE_Ecc_ImportPublicKey_Ram().
	 *
	 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK on match, HSE_SRV_RSP_VERIFY_FAILED otherwise.
	 */
	hseSrvResponse_t HSE_Ecc_VerifyMessage(hseKeyHandle_t verifyKeyHandle, const uint8_t *pMessage, uint32_t msgLen,
										   const uint8_t *pR, uint32_t rLen, const uint8_t *pS, uint32_t sLen)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseSignSrv_t *pSignReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		static uint32_t VerifyRLen;
		static uint32_t VerifySLen;

		VerifyRLen = rLen;
		VerifySLen = sLen;

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pSignReq = &(pHseSrvDescriptor->hseSrv.signReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_SIGN;
		pSignReq->accessMode = HSE_ACCESS_MODE_ONE_PASS;
		pSignReq->streamId = 0U;
		pSignReq->authDir = HSE_AUTH_DIR_VERIFY;
		pSignReq->bInputIsHashed = FALSE;
		pSignReq->signScheme.signSch = HSE_SIGN_ECDSA;
		pSignReq->signScheme.sch.ecdsa.hashAlgo = HSE_HASH_ALGO_SHA2_256;
		pSignReq->keyHandle = verifyKeyHandle;
		pSignReq->sgtOption = HSE_SGT_OPTION_NONE;
		pSignReq->inputLength = msgLen;
		pSignReq->pInput = (HOST_ADDR)pMessage;
		pSignReq->pSignatureLength[0] = (HOST_ADDR)&VerifyRLen;
		pSignReq->pSignatureLength[1] = (HOST_ADDR)&VerifySLen;
		pSignReq->pSignature[0] = (HOST_ADDR)pR;
		pSignReq->pSignature[1] = (HOST_ADDR)pS;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Imports a 64-byte raw (X || Y) public key into ECC_PUB_RAM_KEY_HANDLE, verify-only.
	 *              Used to reconstruct a usable public key purely from bytes read back out of Data
	 *              Flash, independent of the ECC_PAIR_RAM_KEY_HANDLE slot that generated it.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_Ecc_ImportPublicKey_Ram(const uint8_t *pPubKeyXY)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;
		hseImportKeySrv_t *pImportKeyReq;
		hseSrvResponse_t RetVal = HSE_SRV_RSP_GENERAL_ERROR;
		uint8_t u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		static hseKeyInfo_t KeyInfo;

		memset(&KeyInfo, 0, sizeof(KeyInfo));
		KeyInfo.keyFlags = HSE_KF_USAGE_VERIFY;
		KeyInfo.keyBitLen = HSE_KEY256_BITS;
		KeyInfo.keyType = HSE_KEY_TYPE_ECC_PUB;
		KeyInfo.specific.eccCurveId = HSE_EC_SEC_SECP256R1;

		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

		pHseSrvDescriptor->srvId = HSE_SRV_ID_IMPORT_KEY;
		pImportKeyReq->targetKeyHandle = ECC_PUB_RAM_KEY_HANDLE;
		pImportKeyReq->pKeyInfo = (HOST_ADDR)&KeyInfo;
		pImportKeyReq->pKey[0] = (HOST_ADDR)pPubKeyXY;
		pImportKeyReq->pKey[1] = 0U;
		pImportKeyReq->pKey[2] = 0U;
		pImportKeyReq->keyLen[0] = ECC_PUBKEY_SIZE;
		pImportKeyReq->keyLen[1] = 0U;
		pImportKeyReq->keyLen[2] = 0U;
		pImportKeyReq->cipher.cipherKeyHandle = HSE_INVALID_KEY_HANDLE;
		pImportKeyReq->keyContainer.authKeyHandle = HSE_INVALID_KEY_HANDLE;
		pImportKeyReq->keyFormat.eccKeyFormat = HSE_KEY_FORMAT_ECC_PUB_RAW;

		HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Full ECC example: generate a key pair, sign a sample message, verify it fresh,
	 *              then round-trip [message][public key][r][s] through a dedicated Data Flash
	 *              sector, re-import the flash-read public key into a separate verify-only slot, and
	 *              verify the flash-read signature against the flash-read message using only that
	 *              flash-recovered key.
	 */
	void HSE_Ecc_StorageCheckDemo(void)
	{
		uint32_t i;

		for (i = 0U; i < ECC_MESSAGE_SIZE; i++)
		{
			HSE_Ecc_TestMessage[i] = (uint8_t)('a' + (i % 26U));
		}

		HSE_Ecc_GenerateKeyPairResponse = HSE_Ecc_GenerateKeyPair();
		HSE_Ecc_SignResponse = HSE_Ecc_SignMessage(HSE_Ecc_TestMessage, ECC_MESSAGE_SIZE);
		HSE_Ecc_VerifyResponse = HSE_Ecc_VerifyMessage(ECC_PAIR_RAM_KEY_HANDLE, HSE_Ecc_TestMessage, ECC_MESSAGE_SIZE,
													   HSE_EccSignatureR, HSE_EccSignatureRLength,
													   HSE_EccSignatureS, HSE_EccSignatureSLength);

		/* Build the flash blob: [message][public key][r][s] */
		memcpy(&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_MESSAGE], HSE_Ecc_TestMessage, ECC_MESSAGE_SIZE);
		memcpy(&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_PUBKEY], HSE_EccPublicKey, ECC_PUBKEY_SIZE);
		memcpy(&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_R], HSE_EccSignatureR, ECC_CURVE_BYTE_LEN);
		memcpy(&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_S], HSE_EccSignatureS, ECC_CURVE_BYTE_LEN);

		HSE_Ecc_FlashWriteStatus = HSE_MacEcc_FlashWriteBlob(ECC_FLASH_ADDR, ECC_FLASH_VIRTUAL_SECTOR, HSE_Ecc_FlashBlob, ECC_FLASH_BLOB_SIZE);
		HSE_Ecc_FlashReadStatus = HSE_MacEcc_FlashRead(ECC_FLASH_ADDR, HSE_Ecc_FlashBlob, ECC_FLASH_BLOB_SIZE);

		HSE_Ecc_ImportPubKeyFromFlashResponse = HSE_Ecc_ImportPublicKey_Ram(&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_PUBKEY]);

		HSE_Ecc_VerifyFromFlashResponse = HSE_Ecc_VerifyMessage(ECC_PUB_RAM_KEY_HANDLE,
																&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_MESSAGE], ECC_MESSAGE_SIZE,
																&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_R], ECC_CURVE_BYTE_LEN,
																&HSE_Ecc_FlashBlob[ECC_BLOB_OFFSET_S], ECC_CURVE_BYTE_LEN);

		HSE_Ecc_FlashRoundTripVerified = (C40_IP_STATUS_SUCCESS == HSE_Ecc_FlashWriteStatus) &&
										 (C40_IP_STATUS_SUCCESS == HSE_Ecc_FlashReadStatus) &&
										 (HSE_SRV_RSP_OK == HSE_Ecc_ImportPubKeyFromFlashResponse) &&
										 (HSE_SRV_RSP_OK == HSE_Ecc_VerifyFromFlashResponse);
	}

	/*!
	 * @brief       Runs both storage-check demos (MAC, then ECC) in sequence.
	 */
	void HSE_Mac_Ecc_Example_Demo(void)
	{
		HSE_Mac_StorageCheckDemo();
		// HSE_Ecc_StorageCheckDemo();
	}

#ifdef __cplusplus
}
#endif

/** @} */
