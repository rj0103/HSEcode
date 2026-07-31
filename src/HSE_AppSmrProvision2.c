/**
 ******************************************************************************
 * @file     HSE_AppSmrProvision2.c
 * @brief    Provisioning app logic - see this module's header for context.
 *           Whole file compiles to nothing unless PROVISION_APP_BUILD is
 *           defined (Flash_Load_KEY build config only).
 * @location /test/src/HSE_AppSmrProvision2.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifdef __cplusplus
extern "C"
{
#endif

#include "HSE_AppSmrProvision2.h"

#ifdef PROVISION_APP_BUILD

#include "string.h"
#include "app_smr_2_provision_data.h"

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
#define MU0_INSTANCE_U8_LOCAL  ((uint8)0U)

/* NVM catalog group 1, slot 0 (HSE_Main.c's Hse_aNvmKeyCatalog) - the real, persistent Main app
   verify key. Must match the handle Bootlaoder's HSE_BootVerify.c checks against. */
#define ECC_APP_VERIFY_NVM_KEY_HANDLE  GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_NVM, 1, 0)

/* Must match APP_SMR_ENTRY_INDEX conceptually used by Bootlaoder's HSE_BootVerify.c. */
#define APP_SMR_ENTRY_INDEX  (2U)

#define ECC_CURVE_BYTE_LEN  (32U)  /* secp256r1: 256-bit curve, 32-byte r/s/coordinates */

/* Fixed region, per the current memory map - not measured/recalculated per build (see this
   module's header comment). MUST match what extract_app_slice.bat actually signs, not the raw
   flash layout:
     - START = __text_start, not ORIGIN(int_pflash) (0x00482000) - linker_flash_s32k312_Release.ld
       places .boot_header at ORIGIN then rounds up to the next 8KB sector before __text_start, so
       __text_start lands at 0x00484000. Signing (and this install) must both start there - the
       boot_header bytes themselves are never part of the measured/authenticated range.
     - SIZE = 0x0001E000 (120KB), NOT int_pflash's full 0x00020000 (128KB) LENGTH - int_pflash is
       declared as ORIGIN 0x00482000 + LENGTH 0x00020000 = ends at 0x004A2000, and __text_start
       already ate 0x2000 of that for .boot_header, so only 0x1E000 remains inside int_pflash's
       own declared bounds. linker_flash_s32k312_Release.ld's .pflash section is explicitly
       zero-padded out to exactly ORIGIN(int_pflash)+LENGTH(int_pflash) - matching this SIZE
       guarantees the signed region is exactly the region the linker guarantees is real,
       deterministic content; going past it (e.g. the old 0x00020000) would authenticate bytes
       past int_pflash's own boundary that nothing guarantees are zero. Must match the --pad-to
       value extract_app_slice.bat passes to sign_tool.py slice. */
#define APP_SMR_REGION_START  (0x00484000UL)
#define APP_SMR_REGION_SIZE   (0x0001E000UL)

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/
hseSrvResponse_t HSE_AppSmr2_HseInitResponse           = HSE_SRV_RSP_GENERAL_ERROR;
hseKeyInfo_t     HSE_AppSmr2_VerifyKeyInfo;
hseSrvResponse_t HSE_AppSmr2_GetVerifyKeyInfoResponse  = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_AppSmr2_ImportVerifyKeyResponse   = HSE_SRV_RSP_GENERAL_ERROR;
hseSrvResponse_t HSE_AppSmr2_InstallEntryResponse      = HSE_SRV_RSP_GENERAL_ERROR;

/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/
/* This translation unit obtains its own MU channel(s) via Hse_Ip_GetFreeChannel(), so it needs its
   own host-side request/descriptor storage, same reasoning as every other HSE_*.c file in this
   codebase - never shared across translation units. */
static Hse_Ip_ReqType     HseIp_aRequest[HSE_IP_NUM_OF_CHANNELS_PER_MU];
static hseSrvDescriptor_t Hse_aSrvDescriptor[HSE_IP_NUM_OF_CHANNELS_PER_MU];
static Hse_Ip_MuStateType HseIp_MuState;

/*!
 * @brief       Brings up the Hse_Ip driver on MU0. Mirrors Bootlaoder's HSE_BootVerify_Init():
 *              bounded wait for HSE firmware's own HSE_STATUS_INIT_OK hardware status bit, then
 *              Hse_Ip_Init().
 *
 * @return      true if HSE came up and the driver initialized successfully; false otherwise.
 */
bool HSE_AppSmr2_Init(void)
{
	volatile hseStatus_t HseStatus;
	volatile uint32_t    u32Timeout = TIMEOUT_TICKS_U32;
	Hse_Ip_StatusType    HseIpStatus;

	while (u32Timeout != 0U)
	{
		HseStatus = Hse_Ip_GetHseStatus(MU0_INSTANCE_U8_LOCAL);
		if (0U != (HseStatus & HSE_STATUS_INIT_OK))
		{
			break;
		}
		u32Timeout--;
	}

	if (0U == u32Timeout)
	{
		return false; /* HSE firmware never reported ready within the timeout budget */
	}

	HseIpStatus = Hse_Ip_Init(MU0_INSTANCE_U8_LOCAL, &HseIp_MuState);

	return (HSE_IP_STATUS_SUCCESS == HseIpStatus);
}

/*!
 * @brief       Reads back the stored properties of ECC_APP_VERIFY_NVM_KEY_HANDLE (flags, key
 *              type, bit length) without exposing the raw key value. HSE_SRV_RSP_KEY_EMPTY means
 *              the slot has never been provisioned yet.
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_AppSmr2_GetVerifyKeyInfo(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseGetKeyInfoSrv_t* pGetKeyInfoReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8_LOCAL);

	memset(&HSE_AppSmr2_VerifyKeyInfo, 0, sizeof(HSE_AppSmr2_VerifyKeyInfo));

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pGetKeyInfoReq = &(pHseSrvDescriptor->hseSrv.getKeyInfoReq);

	pHseSrvDescriptor->srvId  = HSE_SRV_ID_GET_KEY_INFO;
	pGetKeyInfoReq->keyHandle = ECC_APP_VERIFY_NVM_KEY_HANDLE;
	pGetKeyInfoReq->pKeyInfo  = (HOST_ADDR)&HSE_AppSmr2_VerifyKeyInfo;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8_LOCAL, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

/*!
 * @brief       Imports AppSmr_PublicKeyXY as a verify-only secp256r1 public key into
 *              ECC_APP_VERIFY_NVM_KEY_HANDLE (persistent NVM). Skips the import if the slot is
 *              already provisioned from an earlier run.
 *
 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK also covers the "already provisioned" case.
 */
hseSrvResponse_t HSE_AppSmr2_ImportVerifyKey_Nvm(void)
{
	hseSrvDescriptor_t* pHseSrvDescriptor;
	hseImportKeySrv_t*  pImportKeyReq;
	hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t             u8MuChannel;
	static hseKeyInfo_t KeyInfo;

	HSE_AppSmr2_GetVerifyKeyInfoResponse = HSE_AppSmr2_GetVerifyKeyInfo();
	if (HSE_SRV_RSP_OK == HSE_AppSmr2_GetVerifyKeyInfoResponse)
	{
		/* Already provisioned by an earlier run - nothing to do. */
		return HSE_SRV_RSP_OK;
	}

	u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8_LOCAL);

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

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8_LOCAL, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	/* Refresh the cached key info now that the key has (hopefully) been written */
	HSE_AppSmr2_GetVerifyKeyInfoResponse = HSE_AppSmr2_GetVerifyKeyInfo();

	return RetVal;
}

/*!
 * @brief       Installs SMR entry #APP_SMR_ENTRY_INDEX covering [APP_SMR_REGION_START,
 *              APP_SMR_REGION_START + APP_SMR_REGION_SIZE) - Main app's real, already-flashed
 *              bytes, since all three images are flashed together at EOL - authenticated via
 *              ECDSA/SHA-256 using ECC_APP_VERIFY_NVM_KEY_HANDLE and the real (r,s) from
 *              app_smr_2_provision_data.h. In-place auth (pSmrDest = 0, no copy), on-demand only
 *              (checkPeriod = 0).
 *
 * @return      hseSrvResponse_t
 */
hseSrvResponse_t HSE_AppSmr2_InstallEntry(void)
{
	hseSrvDescriptor_t*      pHseSrvDescriptor;
	hseSmrEntryInstallSrv_t* pSmrInstallReq;
	hseSrvResponse_t         RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	uint8_t                  u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8_LOCAL);
	static hseSmrEntry_t     SmrEntry;

	memset(&SmrEntry, 0, sizeof(SmrEntry));
	SmrEntry.pSmrSrc                                 = (uint32_t)APP_SMR_REGION_START;
	SmrEntry.smrSize                                 = (uint32_t)APP_SMR_REGION_SIZE;
	SmrEntry.pSmrDest                                = 0U;
	SmrEntry.configFlags                             = HSE_SMR_CFG_FLAG_INSTALL_AUTH;
	SmrEntry.checkPeriod                             = 0U;
	SmrEntry.authKeyHandle                           = ECC_APP_VERIFY_NVM_KEY_HANDLE;
	SmrEntry.authScheme.sigScheme.signSch            = HSE_SIGN_ECDSA;
	SmrEntry.authScheme.sigScheme.sch.ecdsa.hashAlgo = HSE_HASH_ALGO_SHA2_256;
	SmrEntry.pInstAuthTag[0]                         = (uint32_t)AppSmr_SignatureR;
	SmrEntry.pInstAuthTag[1]                         = (uint32_t)AppSmr_SignatureS;
	SmrEntry.versionOffset                           = HSE_SMR_VERSION_NOT_USED;

	pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	pSmrInstallReq = &(pHseSrvDescriptor->hseSrv.smrEntryInstallReq);

	pHseSrvDescriptor->srvId         = HSE_SRV_ID_SMR_ENTRY_INSTALL;
	pSmrInstallReq->accessMode       = HSE_ACCESS_MODE_ONE_PASS;
	pSmrInstallReq->entryIndex       = APP_SMR_ENTRY_INDEX;
	pSmrInstallReq->pSmrEntry        = (HOST_ADDR)&SmrEntry;
	pSmrInstallReq->pSmrData         = (HOST_ADDR)APP_SMR_REGION_START;
	pSmrInstallReq->smrDataLength    = (uint32_t)APP_SMR_REGION_SIZE;
	pSmrInstallReq->pAuthTag[0]      = (HOST_ADDR)AppSmr_SignatureR;
	pSmrInstallReq->pAuthTag[1]      = (HOST_ADDR)AppSmr_SignatureS;
	pSmrInstallReq->authTagLength[0] = (uint16_t)ECC_CURVE_BYTE_LEN;
	pSmrInstallReq->authTagLength[1] = (uint16_t)ECC_CURVE_BYTE_LEN;

	HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8_LOCAL, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	return RetVal;
}

void HSE_AppSmr2_Provision(void)
{
	bool hseUp = HSE_AppSmr2_Init();

	HSE_AppSmr2_HseInitResponse = hseUp ? HSE_SRV_RSP_OK : HSE_SRV_RSP_GENERAL_ERROR;
	if (!hseUp)
	{
		return;
	}

	HSE_AppSmr2_ImportVerifyKeyResponse = HSE_AppSmr2_ImportVerifyKey_Nvm();
	if (HSE_SRV_RSP_OK == HSE_AppSmr2_ImportVerifyKeyResponse)
	{
		HSE_AppSmr2_InstallEntryResponse = HSE_AppSmr2_InstallEntry();
	}
}

#endif /* PROVISION_APP_BUILD */

#ifdef __cplusplus
}
#endif

/** @} */
