/**
 ******************************************************************************
 * @file     HSE_AppSmrProvision.h
 * @brief    One-time provisioning: import GSLU_APP's real ECDSA (secp256r1)
 *           verify key into HSE's persistent NVM catalog, and install GSLU_APP's
 *           own SMR entry (covering __text_start..__text_end) so the Bootlaoder
 *           project can verify it before jumping in.
 *           See BOOTLOADER_SECURE_BOOT_PLAN.md, Stage 2.
 * @details  Signing this image and then embedding that same signature into it
 *           would be circular - embedding the real signature changes the very
 *           bytes the signature is supposed to describe. Fixed by keeping the
 *           key/signature *data* out of the build that actually gets signed:
 *           - RUN_APP_SMR_PROVISIONING OFF (the normal, default state): the
 *             key/signature arrays are all-zero. THIS is the build you sign
 *             with tools/sign_tool.py, and the one that gets shipped/flashed
 *             permanently - the one Bootlaoder will check forever after.
 *           - RUN_APP_SMR_PROVISIONING ON: the arrays hold the real values
 *             from app_smr_provision_data.h. Flash this ONCE so the import +
 *             install calls run with the real data, then flip back OFF and
 *             reflash the original all-zero build (same exact bytes you
 *             signed) permanently.
 *           Toggling this flag does NOT change __text_start/__text_end -
 *           the arrays are the same fixed size (64/32/32 bytes) either way,
 *           only their contents differ - so both builds share the same
 *           address layout, and the code doesn't need to know or care which
 *           one is currently running.
 *           CORRECTION (confirmed by testing): HSE_SRV_ID_SMR_ENTRY_INSTALL with
 *           HSE_SMR_CFG_FLAG_INSTALL_AUTH DOES verify pSmrData against pAuthTag
 *           immediately at install time - it is not just recording a future
 *           reference. So installing from the provisioning build itself (whose
 *           own region content differs from the signed all-zero build's) is
 *           expected to report HSE_SRV_RSP_VERIFY_FAILED. What matters is
 *           whether the SMR entry structure is still recorded despite that -
 *           confirm by reflashing the all-zero build and checking the
 *           bootloader's on-demand verify (HSE_BootVerify_VerifyAppSmr()).
 *           If a re-install is ever needed after the app changes (new code,
 *           same key), HSE_AppSmr_UpdateSignature() re-runs just the install
 *           step with whatever (r,s) is currently compiled in, without
 *           touching the already-imported key - re-run tools/sign_tool.py's
 *           `sign` + `header` commands (same key pair) to regenerate
 *           app_smr_provision_data.h first.
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
*                                      DEFINES AND MACROS
==================================================================================================*/
/* Single authoritative toggle - both HSE_AppSmrProvision.c and APP_Main.c see this same
   definition (both include this header). Leave commented out except for the one-time
   provisioning flash described above. */
//#define RUN_APP_SMR_PROVISIONING

/* Separate toggle for re-entering just a new signature (HSE_AppSmr_UpdateSignature()) after the
   app was rebuilt/re-signed but the key itself hasn't changed - the common case going forward,
   since the key only ever needs importing once. Independent from RUN_APP_SMR_PROVISIONING so
   either can be exercised without the other. */
//#define RUN_APP_SMR_SIGNATURE_UPDATE

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
extern hseKeyInfo_t     HSE_AppSmrVerifyKeyInfo;
extern hseSrvResponse_t HSE_AppSmr_GetVerifyKeyInfoResponse;
extern hseSrvResponse_t HSE_AppSmr_ImportVerifyKeyResponse;
extern hseSrvResponse_t HSE_AppSmr_InstallEntryResponse;
extern hseSrvResponse_t HSE_AppSmr_VerifyEntryResponse;
extern hseSrvResponse_t HSE_AppSmr_UpdateSignatureResponse;

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
 * @brief   Imports the app's public key (real bytes if RUN_APP_SMR_PROVISIONING is ON, all-zero
 *          otherwise - see file @details) as a verify-only secp256r1 public key into HSE's
 *          persistent NVM catalog. Skips the import if already provisioned.
 */
hseSrvResponse_t HSE_AppSmr_ImportVerifyKey_Nvm(void);

/*!
 * @brief   Installs GSLU_APP's own SMR entry, covering [__text_start, __text_end) - the real
 *          flashed vector table + code + rodata of this running image - authenticated via
 *          ECDSA/SHA-256 using the NVM verify key and the (r,s) signature (real if
 *          RUN_APP_SMR_PROVISIONING is ON, all-zero otherwise - see file @details).
 */
hseSrvResponse_t HSE_AppSmr_InstallEntry(void);

/*!
 * @brief   Triggers on-demand verification of GSLU_APP's own SMR entry. Only meaningful once the
 *          all-zero (RUN_APP_SMR_PROVISIONING OFF) build - the one actually signed - is what's
 *          flashed; calling this from the provisioning build itself will correctly report
 *          HSE_SRV_RSP_VERIFY_FAILED, since the provisioning build's own (real-data-containing)
 *          bytes, not the signed all-zero build's, currently occupy the region.
 */
hseSrvResponse_t HSE_AppSmr_VerifyEntryOnDemand(void);

/*!
 * @brief   Runs the one-time provisioning sequence: import the NVM verify key, then install the
 *          SMR entry. Deliberately does not self-verify afterward - see
 *          HSE_AppSmr_VerifyEntryOnDemand()'s @brief for why that would be misleading here.
 *          Inspect HSE_AppSmr_ImportVerifyKeyResponse / HSE_AppSmr_InstallEntryResponse via
 *          debugger instead.
 */
void HSE_AppSmr_Provision_Demo(void);

/*!
 * @brief   Re-enters a NEW signature for the app WITHOUT touching the already-imported verify
 *          key - just re-runs HSE_AppSmr_InstallEntry() with whatever (r,s) is currently compiled
 *          in (app_smr_provision_data.h). Use this (instead of HSE_AppSmr_Provision_Demo()) when
 *          the app was rebuilt/re-signed but the key itself hasn't changed - the common case,
 *          since the key only needs to be imported once, ever.
 *
 * @return  hseSrvResponse_t - same caveat as HSE_AppSmr_InstallEntry() applies: calling this from
 *          the provisioning build itself (RUN_APP_SMR_PROVISIONING ON) is expected to report
 *          HSE_SRV_RSP_VERIFY_FAILED, since install verifies against the CURRENT flash content,
 *          which differs from the all-zero build that was actually signed.
 */
hseSrvResponse_t HSE_AppSmr_UpdateSignature(void);

#ifdef __cplusplus
}
#endif

#endif /* __HSE_APP_SMR_PROVISION_H__ */

/** @} */
