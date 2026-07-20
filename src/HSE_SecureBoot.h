/**
 ******************************************************************************
 * @file     HSE_SecureBoot.h
 * @brief    Secure boot (SMR install/verify) proof-of-concept, staged per
 *           SECURE_BOOT_PLAN.md. Stage 1a: prepare a throwaway 20KB test
 *           region in Data Flash to exercise the SMR pipeline against,
 *           before it's ever pointed at the real application image.
 * @location /test/src/HSE_SecureBoot.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifndef __HSE_SECUREBOOT_H__
#define __HSE_SECUREBOOT_H__

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
extern C40_Ip_StatusType HSE_SecureBootTestDataWriteStatus;
extern bool              HSE_SecureBootTestDataVerified;

/* Stage 1a pipeline results, all left in these globals purely for debugger inspection - mirrors
   the HSE_ImportKeyResponse / HSE_AesEncryptResponse etc. pattern already used in HSE_Main.c. */
extern hseSrvResponse_t HSE_ImportSmrSignKeyRamResponse;
extern hseSrvResponse_t HSE_ImportSmrVerifyKeyNvmResponse;
extern hseKeyInfo_t     HSE_SmrVerifyKeyInfo;
extern hseSrvResponse_t HSE_GetSmrVerifyKeyInfoResponse;
extern hseSrvResponse_t HSE_ComputeSmrInstallTagResponse;
extern uint8_t          HSE_SmrInstallTag[16];
extern uint32_t         HSE_SmrInstallTagLength;
extern hseSrvResponse_t HSE_EraseSmrSignKeyRamResponse;
extern hseSrvResponse_t HSE_InstallSmrEntryResponse;
extern hseSrvResponse_t HSE_VerifySmrEntryResponse;

/* Negative-control (plan step 7) results - proves the verify isn't a rubber stamp */
extern C40_Ip_StatusType HSE_SecureBootCorruptTestDataStatus;
extern hseSrvResponse_t  HSE_VerifySmrEntryAfterCorruptionResponse;
extern bool              HSE_SecureBootNegativeControlPassed;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*!
 * @brief   Erases sectors 3-5 of Data Flash (0x10006000, 20KB span) and fills them with a
 *          repeating 0x00-0xFF byte-counter pattern, standing in for "application data" to be
 *          protected. Then reads it back and confirms it matches. This is throwaway test data,
 *          deliberately kept out of the real application image (see SECURE_BOOT_PLAN.md).
 */
void HSE_SecureBoot_PrepareTestData(void);

/*!
 * @brief   Imports a transient AES-128 key (sign+verify capable) into the RAM key catalog,
 *          used only to generate the SMR installation tag before being erased again.
 */
hseSrvResponse_t HSE_ImportSmrSignKey_Ram(void);

/*!
 * @brief   Reads back the stored properties of the persistent SMR verify key (NVM), without
 *          exposing the raw key value. Also used to detect "already provisioned by an earlier
 *          boot" so HSE_ImportSmrVerifyKey_Nvm() doesn't attempt to re-import over it.
 */
hseSrvResponse_t HSE_GetSmrVerifyKeyInfo(void);

/*!
 * @brief   Imports the persistent, verify-only AES-128 key into the NVM key catalog. Skips the
 *          import if HSE_GetSmrVerifyKeyInfo() shows the slot is already provisioned.
 */
hseSrvResponse_t HSE_ImportSmrVerifyKey_Nvm(void);

/*!
 * @brief   Computes a CMAC (AES-128) tag over [pRegionStart, pRegionStart + regionLen) using the
 *          transient RAM sign key, storing the result in HSE_SmrInstallTag / HSE_SmrInstallTagLength.
 */
hseSrvResponse_t HSE_ComputeSmrInstallTag(const uint8_t *pRegionStart, uint32_t regionLen);

/*!
 * @brief   Erases the transient RAM sign key - it has done its one job (producing the install
 *          tag) and must not remain on the device with SIGN usage.
 */
hseSrvResponse_t HSE_EraseSmrSignKey_Ram(void);

/*!
 * @brief   Installs an SMR entry at the given table index, covering [pRegionStart, pRegionStart +
 *          regionLen), authenticated via the persistent NVM verify key and the tag already
 *          computed by HSE_ComputeSmrInstallTag(). On-demand only (checkPeriod = 0); this alone
 *          does not affect boot until a Core Reset entry references it (Phase 2, not done here).
 */
hseSrvResponse_t HSE_InstallSmrEntry(uint8_t entryIndex, const uint8_t *pRegionStart, uint32_t regionLen);

/*!
 * @brief   Triggers on-demand verification of the given SMR entry and returns the raw response
 *          (HSE_SRV_RSP_OK on match, HSE_SRV_RSP_VERIFY_FAILED if the region no longer matches
 *          the installed tag).
 */
hseSrvResponse_t HSE_VerifySmrEntryOnDemand(uint8_t entryIndex);

/*!
 * @brief   Negative control (plan step 7): deliberately corrupts the first chunk of the Stage 1a
 *          test region (already verified as installed) and re-runs on-demand verification.
 *          Confirms the check reports HSE_SRV_RSP_VERIFY_FAILED - i.e. that it isn't a no-op.
 *          Leaves results in HSE_SecureBootCorruptTestDataStatus / HSE_VerifySmrEntryAfterCorruptionResponse
 *          / HSE_SecureBootNegativeControlPassed for inspection.
 */
void HSE_SecureBoot_NegativeControlTest(uint8_t entryIndex);

/*!
 * @brief   Single orchestrating entry point for the whole Stage 1a demo (plan steps 1-7): prepares
 *          the throwaway test region, imports both keys, computes and installs the SMR entry,
 *          erases the transient sign key, verifies on-demand (expect PASS), then runs the
 *          negative-control corruption test (expect FAIL). Nothing here changes boot behavior -
 *          the SMR is not yet linked to a Core Reset entry (see SECURE_BOOT_PLAN.md Phase 2).
 */
void HSE_SecureBoot_Phase1_Demo(void);

#ifdef __cplusplus
}
#endif

#endif /* __HSE_SECUREBOOT_H__ */

/** @} */
