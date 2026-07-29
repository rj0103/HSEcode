/**
 ******************************************************************************
 * @file     HSE_AppSmrProvision.h
 * @brief    One-time provisioning: import GSLU_APP's real ECDSA (secp256r1)
 *           verify key into HSE's persistent NVM catalog, and install GSLU_APP's
 *           own SMR entry (covering the FINAL app's fixed [text_start, text_end)
 *           range) so the Bootlaoder project can verify it before jumping in.
 *           See BOOTLOADER_SECURE_BOOT_PLAN.md, Stage 2.
 * @details  IMPORTANT - two-build model, not "flash once and it just works":
 *           1. Build GSLU_APP with RUN_APP_SMR_PROVISIONING left OFF (undefined,
 *              below). This file's ENTIRE body compiles to nothing in that build -
 *              no key data, no provisioning code at all. THIS is the "final app" -
 *              the one that gets signed (tools/sign_tool.py) and the one the
 *              bootloader will check forever after. Note its __text_start/
 *              __text_end from its own .map file.
 *           2. Hardcode those exact addresses into HSE_AppSmrProvision.c's
 *              APP_PROTECTED_TEXT_START/END (NOT the extern __text_start/
 *              __text_end linker symbols - those would refer to THIS
 *              (provisioning) build's own, larger, different layout, not the
 *              final app's).
 *           3. Build GSLU_APP AGAIN with RUN_APP_SMR_PROVISIONING ON (this time
 *              this file's real code + the real key/signature from
 *              app_smr_provision_data.h ARE compiled in). Flash this build ONCE -
 *              it imports the key and installs the SMR pointing at the addresses
 *              from step 2, then plays no further part.
 *           4. Reflash the ORIGINAL "final app" build from step 1 (unchanged,
 *              same exact bytes that were signed) - this runs permanently, and
 *              is what Bootlaoder verifies before jumping in.
 *
 *           WHY the two builds: this file's own compiled key/signature data
 *           would otherwise sit inside the very byte range the signature is
 *           supposed to describe - a circular "signing an image that contains
 *           its own signature" problem. Keeping this file's content entirely
 *           absent from the final, protected build sidesteps that: nothing
 *           about the final app depends on what's compiled here.
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
/* Single authoritative toggle for this whole module - both HSE_AppSmrProvision.c and APP_Main.c
   see this same definition (both include this header). Leave commented out for every build
   except the one-time "provisioning" flash described above. */
//#define RUN_APP_SMR_PROVISIONING

#ifdef RUN_APP_SMR_PROVISIONING

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
 * @brief   Installs GSLU_APP's own SMR entry, covering the FINAL app's fixed, hardcoded
 *          [APP_PROTECTED_TEXT_START, APP_PROTECTED_TEXT_END) range (see file @details - NOT
 *          this build's own __text_start/__text_end), authenticated via ECDSA/SHA-256 using the
 *          NVM verify key and the (r,s) signature from app_smr_provision_data.h.
 */
hseSrvResponse_t HSE_AppSmr_InstallEntry(void);

/*!
 * @brief   Triggers on-demand verification of GSLU_APP's own SMR entry. Only meaningful once the
 *          final app (not this provisioning build) is actually the thing flashed at the protected
 *          address range - calling this during the provisioning build itself will correctly
 *          report HSE_SRV_RSP_VERIFY_FAILED, since this provisioning build's own bytes (not the
 *          final app's) currently occupy that range. Provided for manual use after reflashing the
 *          final app, if you want to double-check before relying on the bootloader's own check.
 */
hseSrvResponse_t HSE_AppSmr_VerifyEntryOnDemand(void);

/*!
 * @brief   Runs the one-time provisioning sequence: import the NVM verify key, then install the
 *          SMR entry. Does NOT self-verify (see HSE_AppSmr_VerifyEntryOnDemand()'s @brief for why
 *          that would be misleading here) - inspect HSE_AppSmr_ImportVerifyKeyResponse /
 *          HSE_AppSmr_InstallEntryResponse via debugger instead.
 */
void HSE_AppSmr_Provision_Demo(void);

#endif /* RUN_APP_SMR_PROVISIONING */

#ifdef __cplusplus
}
#endif

#endif /* __HSE_APP_SMR_PROVISION_H__ */

/** @} */
