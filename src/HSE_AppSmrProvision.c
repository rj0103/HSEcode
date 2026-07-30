/**
 ******************************************************************************
 * @file     HSE_AppSmrProvision.c
 * @brief    One-time provisioning: import GSLU_APP's real ECDSA (secp256r1)
 *           verify key into HSE's persistent NVM catalog, and install GSLU_APP's
 *           own SMR entry (covering __text_start..__text_end) so the Bootlaoder
 *           project can verify it before jumping in.
 *           See BOOTLOADER_SECURE_BOOT_PLAN.md, Stage 2, and this module's
 *           header for why the key/signature data (not the code) is what
 *           toggles behind RUN_APP_SMR_PROVISIONING.
 * @location /test/src/HSE_AppSmrProvision.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifdef __cplusplus
extern "C"
{
#endif

#include "HSE_AppSmrProvision.h"
#include "string.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
/* NVM catalog group 1 (Hse_aNvmKeyCatalog in HSE_Main.c) - dedicated to this key alone. */
#define ECC_APP_VERIFY_NVM_KEY_HANDLE  GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_NVM, 1, 0)

/* SMR#0 is special-cased for SHE-boot/BOOT_MAC_KEY, #1 is HSE_SecureBoot.c's Stage 1a test entry -
   use #2 for GSLU_APP's real, bootloader-facing entry. */
#define APP_SMR_ENTRY_INDEX  (2U)

#define ECC_CURVE_BYTE_LEN  (32U)  /* secp256r1: 256-bit curve, 32-byte r/s/coordinates */

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/
hseKeyInfo_t     HSE_AppSmrVerifyKeyInfo;
hseSrvResponse_t HSE_AppSmr_GetVerifyKeyInfoResponse = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_AppSmr_ImportVerifyKeyResponse  = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_AppSmr_InstallEntryResponse     = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_AppSmr_VerifyEntryResponse      = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_AppSmr_UpdateSignatureResponse  = HSE_SRV_RSP_GENERAL_ERROR;

/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/
/* Key/signature data: real values (from tools/sign_tool.py, via app_smr_provision_data.h) only
   when RUN_APP_SMR_PROVISIONING is ON. OFF (the default, and the state the shipped/signed build
   uses) keeps these all-zero. Either way the arrays are the same fixed size, so this choice never
   changes __text_start/__text_end - see this file's header for why that matters. */
#ifdef RUN_APP_SMR_PROVISIONING
#include "app_smr_provision_data.h"
#else
static const uint8_t AppSmr_PublicKeyXY[64] = {0};
static const uint8_t AppSmr_SignatureR[32]  = {0};
static const uint8_t AppSmr_SignatureS[32]  = {0};
#endif

/* The real flashed image bounds - vector table + code + rodata of this running image, exactly
   like SECURE_BOOT_PLAN.md's Region 1b. Confirmed to exist in the linker script (see that file).
   Identical address range regardless of RUN_APP_SMR_PROVISIONING, since the arrays above are the
   same size either way. */
extern uint32_t __text_start[];
extern uint32_t __text_end[];

/* This translation unit obtains its own MU channels via Hse_Ip_GetFreeChannel(), so it needs its
   own host-side request/descriptor storage - same reasoning as HSE_SecureBoot.c / HSE_Mac_Ecc_Example.c. */
static Hse_Ip_ReqType     HseIp_aRequest[HSE_IP_NUM_OF_CHANNELS_PER_MU];
static hseSrvDescriptor_t Hse_aSrvDescriptor[HSE_IP_NUM_OF_CHANNELS_PER_MU];

/*!
 * @brief       Reads back the stored properties of ECC_APP_VERIFY_NVM_KEY_HANDLE (flags, key
 *              type, bit length) without exposing the raw key value. HSE_SRV_RSP_KEY_EMPTY means
 *              the slot has never been provisioned yet. Result left in HSE_AppSmrVerifyKeyInfo.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_AppSmr_GetVerifyKeyInfo(void)
{
	hseSrvDescriptor_t*   pHseSrvDescriptor;
	hseGetKeyInfoSrv_t*   pGetKeyInfoReq;
	hseSrvResponse_t      RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t               u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	memset(&HSE_AppSmrVerifyKeyInfo, 0, sizeof(HSE_AppSmrVerifyKeyInfo));

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pGetKeyInfoReq = &(pHseSrvDescriptor->hseSrv.getKeyInfoReq);

	pHseSrvDescriptor->srvId  = HSE_SRV_ID_GET_KEY_INFO;
	pGetKeyInfoReq->keyHandle = ECC_APP_VERIFY_NVM_KEY_HANDLE;
	pGetKeyInfoReq->pKeyInfo  = (HOST_ADDR)&HSE_AppSmrVerifyKeyInfo;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Imports AppSmr_PublicKeyXY as a verify-only secp256r1 public key into
 *              ECC_APP_VERIFY_NVM_KEY_HANDLE (persistent NVM). Guarded the same way as
 *              HSE_ImportNvmAESKey(): checks HSE_AppSmr_GetVerifyKeyInfo() first and skips the
 *              import if the slot is already provisioned from an earlier boot.
 *
 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK also covers the "already provisioned" case.
 */
hseSrvResponse_t HSE_AppSmr_ImportVerifyKey_Nvm(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseImportKeySrv_t*  pImportKeyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel;
	static hseKeyInfo_t KeyInfo;

	HSE_AppSmr_GetVerifyKeyInfoResponse = HSE_AppSmr_GetVerifyKeyInfo();
	if (HSE_SRV_RSP_OK == HSE_AppSmr_GetVerifyKeyInfoResponse)
	{
		/* Already provisioned by an earlier boot - nothing to do. */
		return HSE_SRV_RSP_OK;
	}

	u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	KeyInfo.keyFlags            = HSE_KF_USAGE_VERIFY;
	KeyInfo.keyBitLen           = HSE_KEY256_BITS;
	KeyInfo.keyType             = HSE_KEY_TYPE_ECC_PUB;
	KeyInfo.specific.eccCurveId = HSE_EC_SEC_SECP256R1;

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

	pHseSrvDescriptor->srvId                  = HSE_SRV_ID_IMPORT_KEY;
	pImportKeyReq->targetKeyHandle            = ECC_APP_VERIFY_NVM_KEY_HANDLE;
	pImportKeyReq->pKeyInfo                   = (HOST_ADDR)&KeyInfo;
	pImportKeyReq->pKey[0]                    = (HOST_ADDR)AppSmr_PublicKeyXY;
	pImportKeyReq->pKey[1]                    = 0U;
	pImportKeyReq->pKey[2]                    = 0U;
	pImportKeyReq->keyLen[0]                  = sizeof(AppSmr_PublicKeyXY);
	pImportKeyReq->keyLen[1]                  = 0U;
	pImportKeyReq->keyLen[2]                  = 0U;
	pImportKeyReq->cipher.cipherKeyHandle      = HSE_INVALID_KEY_HANDLE;
	pImportKeyReq->keyContainer.authKeyHandle  = HSE_INVALID_KEY_HANDLE;
	pImportKeyReq->keyFormat.eccKeyFormat      = HSE_KEY_FORMAT_ECC_PUB_RAW;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	/* Refresh the cached key info now that the key has (hopefully) been written */
	HSE_AppSmr_GetVerifyKeyInfoResponse = HSE_AppSmr_GetVerifyKeyInfo();

	return RetVal;
}

/*!
 * @brief       Installs SMR entry #APP_SMR_ENTRY_INDEX covering [__text_start, __text_end) -
 *              GSLU_APP's own real flashed image - authenticated via ECDSA/SHA-256 using
 *              ECC_APP_VERIFY_NVM_KEY_HANDLE and the (r,s) signature (real or all-zero - see
 *              this file's @details). In-place auth (pSmrDest = 0, no copy), on-demand only
 *              (checkPeriod = 0).
 * @details     Installing this SMR does not, by itself, change boot behavior - it teaches HSE how
 *              to check the region on request. The Bootlaoder project is what actually calls that
 *              check before jumping in (BOOTLOADER_SECURE_BOOT_PLAN.md Stage 3); linking it to
 *              real core-release enforcement is that plan's Stage 4/Phase 2, not done here.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_AppSmr_InstallEntry(void)
{
	hseSrvDescriptor_t*      pHseSrvDescriptor;
	hseSmrEntryInstallSrv_t* pSmrInstallReq;
	hseSrvResponse_t         RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t                  u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
	static hseSmrEntry_t     SmrEntry;
	uint32_t                 regionLen = (uint32_t)__text_end - (uint32_t)__text_start;

	memset(&SmrEntry, 0, sizeof(SmrEntry));
	SmrEntry.pSmrSrc                          = (uint32_t)__text_start;
	SmrEntry.smrSize                          = regionLen;
	SmrEntry.pSmrDest                         = 0U;
	SmrEntry.configFlags                      = HSE_SMR_CFG_FLAG_INSTALL_AUTH;
	SmrEntry.checkPeriod                      = 0U;
	SmrEntry.authKeyHandle                    = ECC_APP_VERIFY_NVM_KEY_HANDLE;
	SmrEntry.authScheme.sigScheme.signSch           = HSE_SIGN_ECDSA;
	SmrEntry.authScheme.sigScheme.sch.ecdsa.hashAlgo = HSE_HASH_ALGO_SHA2_256;
	SmrEntry.pInstAuthTag[0]                  = (uint32_t)AppSmr_SignatureR;
	SmrEntry.pInstAuthTag[1]                  = (uint32_t)AppSmr_SignatureS;
	SmrEntry.versionOffset                    = HSE_SMR_VERSION_NOT_USED;

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pSmrInstallReq = &(pHseSrvDescriptor->hseSrv.smrEntryInstallReq);

	pHseSrvDescriptor->srvId         = HSE_SRV_ID_SMR_ENTRY_INSTALL;
	pSmrInstallReq->accessMode       = HSE_ACCESS_MODE_ONE_PASS;
	pSmrInstallReq->entryIndex       = APP_SMR_ENTRY_INDEX;
	pSmrInstallReq->pSmrEntry        = (HOST_ADDR)&SmrEntry;
	pSmrInstallReq->pSmrData         = (HOST_ADDR)__text_start;
	pSmrInstallReq->smrDataLength    = regionLen;
	pSmrInstallReq->pAuthTag[0]      = (HOST_ADDR)AppSmr_SignatureR;
	pSmrInstallReq->pAuthTag[1]      = (HOST_ADDR)AppSmr_SignatureS;
	pSmrInstallReq->authTagLength[0] = (uint16_t)ECC_CURVE_BYTE_LEN;
	pSmrInstallReq->authTagLength[1] = (uint16_t)ECC_CURVE_BYTE_LEN;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Triggers on-demand verification of SMR entry #APP_SMR_ENTRY_INDEX.
 * @details     Only meaningful once the all-zero (RUN_APP_SMR_PROVISIONING OFF) build - the one
 *              actually signed - is what's flashed. Calling this from the provisioning build
 *              itself will correctly report HSE_SRV_RSP_VERIFY_FAILED, since the provisioning
 *              build's own (real-data) bytes, not the signed all-zero build's, currently occupy
 *              the region. That's why HSE_AppSmr_Provision_Demo() below does not call this
 *              automatically - use it manually (e.g. via debugger) after reflashing the all-zero
 *              build, if you want to double-check before relying on the bootloader's own check.
 *
 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK on match, HSE_SRV_RSP_VERIFY_FAILED otherwise.
 */
hseSrvResponse_t HSE_AppSmr_VerifyEntryOnDemand(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseSmrVerifySrv_t*  pSmrVerifyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pSmrVerifyReq = &(pHseSrvDescriptor->hseSrv.smrVerifyReq);

	pHseSrvDescriptor->srvId  = HSE_SRV_ID_SMR_VERIFY;
	pSmrVerifyReq->entryIndex = APP_SMR_ENTRY_INDEX;
	pSmrVerifyReq->reserved   = 0U;
	pSmrVerifyReq->options    = HSE_SMR_VERIFICATION_OPTION_NONE;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Runs the one-time provisioning sequence: import the NVM verify key, then install
 *              the SMR entry. Deliberately does NOT self-verify afterward - see
 *              HSE_AppSmr_VerifyEntryOnDemand()'s @details for why that would be misleading here.
 *              Inspect HSE_AppSmr_ImportVerifyKeyResponse / HSE_AppSmr_InstallEntryResponse via
 *              debugger to confirm this ran cleanly (both should read HSE_SRV_RSP_OK).
 */
void HSE_AppSmr_Provision_Demo(void)
{
	HSE_AppSmr_ImportVerifyKeyResponse = HSE_AppSmr_ImportVerifyKey_Nvm();
	HSE_AppSmr_InstallEntryResponse    = HSE_AppSmr_InstallEntry();
}

/*!
 * @brief       Re-enters a new signature without touching the already-imported verify key - see
 *              this function's header comment for when to use it instead of
 *              HSE_AppSmr_Provision_Demo().
 */
hseSrvResponse_t HSE_AppSmr_UpdateSignature(void)
{
	HSE_AppSmr_UpdateSignatureResponse = HSE_AppSmr_InstallEntry();
	return HSE_AppSmr_UpdateSignatureResponse;
}

#ifdef __cplusplus
}
#endif

/** @} */
