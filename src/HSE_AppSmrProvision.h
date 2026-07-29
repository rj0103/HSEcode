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
 *           Not exercised in the same boot the values matter for: the
 *           install call only records the provided signature as HSE's
 *           reference for future checks (it does not need to already match
 *           current flash content at install time) - so it's fine that the
 *           provisioning build's own flash content differs from the
 *           all-zero build's.
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
#define RUN_APP_SMR_PROVISIONING

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

#ifdef __cplusplus
}
#endif

#endif /* __HSE_APP_SMR_PROVISION_H__ */

/** @} */
