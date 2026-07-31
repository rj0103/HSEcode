/**
 ******************************************************************************
 * @file     HSE_AppSmrProvision2.h
 * @brief    Provisioning app logic (build config "Flash_Load_KEY", flashed at
 *           0x004C2000, factory/EOL only - never shipped). Imports Main app's
 *           real, offline-signed verify-only ECC public key into HSE's
 *           persistent NVM catalog, then installs Main app's SMR entry
 *           (index 2) covering its fixed region [0x00482000, 0x004C2000) -
 *           the reserved gap between Main app and this Provisioning app.
 *           Whole file compiles to nothing unless PROVISION_APP_BUILD is
 *           defined (set only in the Flash_Load_KEY build config's own
 *           compiler settings - never hand-toggled, never present in
 *           Release_FLASH/Debug_FLASH).
 * @details  Region bounds are fixed compile-time constants, not measured -
 *           see app_smr_2_provision_data.h's own header comment for why
 *           (tools/sign_tool.py now zero-pads Main app's signed slice out to
 *           this exact fixed size, so it never needs to change build to
 *           build - only the (r,s) signature does, when Main app changes).
 * @location /test/src/HSE_AppSmrProvision2.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifndef __HSE_APP_SMR_PROVISION2_H__
#define __HSE_APP_SMR_PROVISION2_H__

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
==================================================================================================*/
#include "HSE_Main.h"

/* Set only in the Flash_Load_KEY build config's own preprocessor defines - never uncomment this
   by hand. Left here, commented out, purely so this header is self-documenting about what
   controls it; Release_FLASH/Debug_FLASH never define it, so this whole module compiles to
   nothing in those builds. */
//#define PROVISION_APP_BUILD

#ifdef PROVISION_APP_BUILD

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
extern hseSrvResponse_t HSE_AppSmr2_HseInitResponse;
extern hseKeyInfo_t     HSE_AppSmr2_VerifyKeyInfo;
extern hseSrvResponse_t HSE_AppSmr2_GetVerifyKeyInfoResponse;
extern hseSrvResponse_t HSE_AppSmr2_ImportVerifyKeyResponse;
extern hseSrvResponse_t HSE_AppSmr2_InstallEntryResponse;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*!
 * @brief   Brings up the Hse_Ip driver on MU0 - bounded wait for HSE_STATUS_INIT_OK, then
 *          Hse_Ip_Init(). Same bring-up shape as Bootlaoder's HSE_BootVerify_Init() - this app
 *          doesn't need GSLU_APP's full HSE_Init() catalog-formatting logic, just enough to issue
 *          import/install calls.
 *
 * @return  true if HSE came up and the driver initialized successfully; false otherwise.
 */
bool HSE_AppSmr2_Init(void);

/*!
 * @brief   Reads back the stored properties of the Main app verify key (NVM), without exposing
 *          the raw key value. Also used to detect "already provisioned by an earlier run" so
 *          HSE_AppSmr2_ImportVerifyKey_Nvm() doesn't attempt to re-import over it.
 */
hseSrvResponse_t HSE_AppSmr2_GetVerifyKeyInfo(void);

/*!
 * @brief   Imports Main app's real, offline-signed public key (app_smr_2_provision_data.h) as a
 *          verify-only secp256r1 public key into HSE's persistent NVM catalog (group 1, slot 0).
 *          Skips the import if HSE_AppSmr2_GetVerifyKeyInfo() shows the slot is already
 *          provisioned.
 */
hseSrvResponse_t HSE_AppSmr2_ImportVerifyKey_Nvm(void);

/*!
 * @brief   Installs Main app's SMR entry (index 2), covering the fixed region
 *          [APP_SMR_REGION_START, APP_SMR_REGION_START + APP_SMR_REGION_SIZE) - Main app's real,
 *          already-flashed bytes at this point, since all three images are flashed together at
 *          EOL - authenticated via ECDSA/SHA-256 using the NVM verify key and the real (r,s) from
 *          app_smr_2_provision_data.h.
 */
hseSrvResponse_t HSE_AppSmr2_InstallEntry(void);

/*!
 * @brief   Runs the full provisioning sequence: bring up HSE, import the verify key (skip if
 *          already provisioned), install the SMR entry. Does not verify afterward and does not
 *          jump into Main app - APP_Main.c halts and reports via UART/LED; the operator resets
 *          and uses SW1 to actually boot Main app for real.
 */
void HSE_AppSmr2_Provision(void);

#endif /* PROVISION_APP_BUILD */

#ifdef __cplusplus
}
#endif

#endif /* __HSE_APP_SMR_PROVISION2_H__ */

/** @} */
