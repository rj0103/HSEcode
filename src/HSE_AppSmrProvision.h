/**
 ******************************************************************************
 * @file     HSE_AppSmrProvision.h
 * @brief    One-time provisioning: import GSLU_APP's real ECDSA (secp256r1)
 *           verify key into HSE's persistent NVM catalog, and install GSLU_APP's
 *           own SMR entry (covering __text_start..__text_end) so the Bootlaoder
 *           project can verify it before jumping in.
 *           See BOOTLOADER_SECURE_BOOT_PLAN.md, Stage 2.
 * @details  This is meant to be built into GSLU_APP, flashed ONCE (so the key
 *           lands in NVM and the SMR gets installed), then left out of normal
 *           builds afterward - guarded behind RUN_APP_SMR_PROVISIONING, same
 *           idiom as RUN_SECURE_BOOT_PHASE1_DEMO / RUN_MAC_ECC_EXAMPLE. Once
 *           provisioned, GSLU_APP no longer needs to carry this code: the key
 *           and the SMR entry both persist in HSE across reset.
 * @location /test/src/HSE_AppSmrProvision.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifndef __HSE_APP_SMR_PROVISION_H__
#define __HSE_APP_SMR_PROVISION_H__

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
==================================================================================================*/
#include "HSE_Main.h"

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
extern hseKeyInfo_t     HSE_AppSmrVerifyKeyInfo;
extern hseSrvResponse_t HSE_AppSmr_GetVerifyKeyInfoResponse;
extern hseSrvResponse_t HSE_AppSmr_ImportVerifyKeyResponse;
extern hseSrvResponse_t HSE_AppSmr_InstallEntryResponse;
extern hseSrvResponse_t HSE_AppSmr_VerifyEntryResponse;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*!
 * @brief   Reads back the stored properties of the app verify key (NVM), without exposing the
 *          raw key value. Also used to detect "already provisioned by an earlier boot" so
 *          HSE_AppSmr_ImportVerifyKey_Nvm() doesn't attempt to re-import over it.
 */
hseSrvResponse_t HSE_AppSmr_GetVerifyKeyInfo(void);

/*!
 * @brief   Imports AppSmr_PublicKeyXY (from app_smr_provision_data.h - see that file for how to
 *          regenerate it for real, via tools/sign_tool.py) as a verify-only secp256r1 public key
 *          into HSE's persistent NVM catalog. Skips the import if already provisioned.
 */
hseSrvResponse_t HSE_AppSmr_ImportVerifyKey_Nvm(void);

/*!
 * @brief   Installs GSLU_APP's own SMR entry, covering [__text_start, __text_end) - the real
 *          flashed vector table + code + rodata of this running image - authenticated via
 *          ECDSA/SHA-256 using the NVM verify key and the (r,s) signature from
 *          app_smr_provision_data.h.
 */
hseSrvResponse_t HSE_AppSmr_InstallEntry(void);

/*!
 * @brief   Triggers on-demand verification of GSLU_APP's own SMR entry - a sanity check that the
 *          just-installed entry actually verifies before handing control over to the bootloader's
 *          own check. Not itself part of the boot-time enforcement path.
 */
hseSrvResponse_t HSE_AppSmr_VerifyEntryOnDemand(void);

/*!
 * @brief   Runs the whole one-time provisioning sequence: import the NVM verify key, install the
 *          SMR entry, then verify it once as a sanity check. Intended to run exactly once (or be
 *          skipped on subsequent boots once already provisioned) - see file-level @details.
 */
void HSE_AppSmr_Provision_Demo(void);

#ifdef __cplusplus
}
#endif

#endif /* __HSE_APP_SMR_PROVISION_H__ */

/** @} */
