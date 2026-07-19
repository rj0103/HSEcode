# Plan: HSE-Based Secure Boot for GSLU_APP (S32K312 / HSE_B)

## Process Overview (read this first)

Secure boot means: before the application is allowed to run, HSE checks a cryptographic proof that the flashed code hasn't been tampered with, and only releases the CPU core if that check passes. Getting the enforcement step wrong can make the board refuse to boot at all, so this is done as two separate stages, with a hard stop and a safety check in between.

**Stage A — Prove the mechanism works, with no ability to break booting (this is what gets built first):**

1. **Provision a signing key, temporarily.** Import a symmetric AES key into a scratch RAM slot, flagged so it can both sign and verify.
2. **Compute a "proof tag" over a piece of the firmware.** Ask HSE to generate a CMAC (a keyed checksum) over a chosen block of the flashed code, using that temporary key. This tag is what will later prove the code hasn't changed.
3. **Provision the real verification key, permanently.** Import the same key value into a persistent NVM slot — but flagged verify-only this time, never sign-capable, so it can check tags but never mint new ones.
4. **Register the protected region with HSE.** Tell HSE: "this address range, this size, this expected tag, checked with this verify-only key" — this is the SMR (Secure Memory Region) install step. At this point HSE knows how to check the region, but nothing is enforced yet.
5. **Delete the temporary signing key.** It already did its one job (step 2) and should not remain on the device.
6. **Ask HSE to check it, on demand.** Manually trigger verification and read back pass/fail. Nothing about the boot process changes because of this — it's purely "does the check work," run like any other function call.
7. **Prove the check isn't a rubber stamp.** Deliberately corrupt the test data and re-run step 6 — confirm it now reports failure. If it doesn't, the whole mechanism is untrustworthy and nothing further should proceed.

At the end of Stage A, you have hard evidence the sign → install → verify pipeline is cryptographically sound, and zero risk has been taken with the boot process — the board still boots exactly as it does today, unconditionally.

**— Hard stop here. Get the board's recovery path solid before going further —**

8. **Fix and prove the existing "erase HSE" recovery function actually works.** There's already a stub for this in the codebase, but it's built incorrectly and doesn't function. Before touching anything that can block booting, this needs to be corrected and tested, so there's a known-working way to recover the board if Stage B goes wrong.

**Stage B — Turn on real enforcement (separate go-ahead required before starting this; this is the part that can brick the board if done wrong):**

9. **Link the verified region to the actual core-release decision.** Configure HSE's "Core Reset" table so it will hold the CPU in reset if the region from Stage A fails its check — this is the step that actually starts to matter for boot.
10. **Flip the "secure boot enabled" flag in the flashed boot header.** Currently the project's boot header is an unconfigured placeholder; this needs proper research into the exact bit layout HSE expects (flagged in the technical plan below as something to confirm, not guess) before it's touched.
11. **Reflash and test — with the recovery path from step 8 ready, and ideally on a spare board first**, not the only board you have.

Everything from step 9 onward is the genuinely risky part and should only start once Stage A has been demonstrated working and step 8's recovery path is proven, and only with an explicit decision to proceed at that point — not folded into the same work session as Stage A.

---

## Technical Reference (concrete APIs, struct fields, function names for whoever implements Stage A)

The rest of this document is the detailed technical grounding for the process above — exact struct fields, service IDs, and function signatures, all verified against this project's actual vendor headers rather than assumed.

## Context

This project currently uses HSE purely for key management and crypto demos (`HSE_Main.c`, `HSE_FlashStorage_Example.c`): import/generate AES-128 keys, encrypt/decrypt, store ciphertext in Data Flash. The app boots standalone under a P&E debug probe with **no enforced boot chain at all** — `Reset_Handler` → `main()` → `APP_Main()` runs regardless of what firmware is flashed. Moving toward genuine secure boot (HSE verifying the app image before it's allowed to run) is a much bigger, higher-stakes step than anything done so far: getting it wrong can prevent the M7 core from ever leaving reset.

This plan is **staged into two milestones**. Phase 1 proves the SMR (Secure Memory Region) install/verify mechanism works end-to-end with **zero risk to boot** — nothing about whether the core resets or runs changes. Phase 2 (separate, later, requires its own explicit go-ahead) is where actual enforcement gets turned on, and that's the part that can brick the board if wrong.

Everything below is grounded in this project's actual vendor headers (exact file:line citations, verified directly, not guessed):
- `src/interface/inc_services/hse_srv_smr_install.h` — `hseSmrEntry_t`, `hseSmrEntryInstallSrv_t`, `hseSmrVerifySrv_t`, `hseCrEntry_t`, `hseCrEntryInstallSrv_t`
- `src/interface/inc_services/hse_srv_mac.h` — `hseMacSrv_t`
- `src/interface/inc_common/hse_common_types.h` — `hseMacScheme_t`, `hseCmacScheme_t`, `hseAuthScheme_t`
- `src/interface/hse_interface.h` — service IDs
- `src/interface/config/hse_b_config.h` — confirms `HSE_SPT_SMR_CR`, `HSE_NUM_OF_SMR_ENTRIES (8U)` are compiled into this build
- `Project_Settings/Linker_Files/linker_flash_s32k312.ld` — confirms `__text_start` (line 66) / `__text_end` (line 113) symbols exist, bounding the flashed image
- `Project_Settings/Startup_Code/startup_cm7.s` — the current (demo-only, non-HSE-format) `.boot_header` IVT

## New files

Follow the existing paired convention (`HSE_Main.c/.h`, `HSE_FlashStorage_Example.c/.h`):

**`src/HSE_SecureBoot.c`** / **`src/HSE_SecureBoot.h`** — entirely new, isolated translation unit. Does not modify `HSE_Main.c`/`.h` at all (so the already-working key-management demo can't be disturbed). Gated behind `#ifdef RUN_SECURE_BOOT_PHASE1_DEMO` (undefined by default), matching the existing `RUN_FORMAT_KEY_CATALOGS_IN_INIT` / `ERASE_NVM_KEYS` idiom already used in this codebase.

Every function follows the exact low-level pattern already established throughout `HSE_Main.c`: obtain a channel via `Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8)`, `memset` the `Hse_aSrvDescriptor[u8MuChannel]`, fill the relevant `hseSrv.<x>Req` union member, set `HseIp_aRequest[u8MuChannel].eReqType = HSE_IP_REQTYPE_SYNC` / `.u32Timeout = TIMEOUT_TICKS_U32`, call `Hse_Ip_ServiceRequest(...)`, and store the raw `hseSrvResponse_t` in an inspectable global — same as `HSE_ImportKeyResponse`, `HSE_AesEncryptResponse`, etc.

## Key handles and catalog slots (no re-format needed)

Both catalogs already have spare slot capacity — `Hse_aRamKeyCatalog`/`Hse_aNvmKeyCatalog` group 0 each have `4U` slots, only slots 0 and 1 (RAM) / 0 (NVM) are used today. New handles, no `HSE_FormatHseKeyCatalogs()` re-run required, existing keys untouched:
- `AES_SMR_SIGN_RAM_KEY_HANDLE` = `GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM, 0, 2)` — transient, `HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY`, used only to generate the install tag, then erased.
- `AES_SMR_VERIFY_NVM_KEY_HANDLE` = `GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_NVM, 0, 1)` — persistent, `HSE_KF_USAGE_VERIFY` only. This is the SMR's actual `authKeyHandle`.

## Auth scheme: CMAC (AES-128), not ECDSA — for Phase 1

**Recommendation: `HSE_MAC_ALGO_CMAC`.** This project has zero asymmetric-key infrastructure (no RSA/ECDSA key-pair generation, no public-key import, no offline signing tool) — building that is a separate, larger effort. CMAC reuses the exact AES-128 import pattern already proven working (`HSE_ImportAESKey`/`HSE_ImportNvmAESKey`), and `hseSmrEntry_t.authScheme` (a `hseAuthScheme_t` union of `.macScheme`/`.sigScheme`) treats CMAC as a first-class, fully-supported SMR scheme, not a workaround.

**Tradeoff to document in `HSE_SecureBoot.h`, in plain comments**: CMAC's sign and verify operations use the *same* secret key. Even keeping the SMR's `authKeyHandle` VERIFY-only, the raw key bytes still originated on a host machine and got imported onto the device — a real production secure-boot design wants asymmetric (ECDSA), where only a *public* key ever touches the device and the private signing key lives on a build server that never sees the board. That's an explicit, deliberate simplification for this single-dev-board/learning-stage milestone, not a production posture — call it out again when Phase 2 is discussed.

## What gets covered by the SMR, and how the tag is produced

**Region**: `extern uint32_t __text_start[];` / `extern uint32_t __text_end[];` (both confirmed to exist in the linker script). `pSmrSrc = (uint32_t)__text_start`, `smrSize = (uint32_t)__text_end - (uint32_t)__text_start` — this is exactly the already-flashed vector table + code + rodata of the running image. No new build step, no extra flashing.

**Staged validation within Phase 1** (extra safety margin, catches a plumbing mistake before trusting it against the whole image):
- **1a**: first validate against a small `static const uint8_t` test buffer defined inside `HSE_SecureBoot.c` (e.g. 64 bytes — see open item below on alignment) — a trivial, reproducible expected result.
- **1b**: once 1a passes, repeat against the real `__text_start`/`__text_end` span.

**Tag production**: on-device, not an offline signer — appropriate for a single dev board. Sequence:
1. Import the transient RAM key (`AES_SMR_SIGN_RAM_KEY_HANDLE`).
2. Generate the CMAC tag via `HSE_SRV_ID_MAC` / `hseMacSrv_t`: `accessMode = HSE_ACCESS_MODE_ONE_PASS`, `authDir = HSE_AUTH_DIR_GENERATE`, `macScheme.macAlgo = HSE_MAC_ALGO_CMAC`, `macScheme.sch.cmac.cipherAlgo = HSE_CIPHER_ALGO_AES`, `keyHandle = AES_SMR_SIGN_RAM_KEY_HANDLE`, `pInput`/`inputLength` = the region, `pTag` = output buffer, **`pTagLength` is itself a pointer to a `uint32_t` holding the buffer size as input / actual tag length as output** (same "pointer-to-length" pattern `HSE_genreateSHA()` already uses via its `p_length` global — mirror that exactly, don't pass a plain integer).
3. Import the persistent NVM key (`AES_SMR_VERIFY_NVM_KEY_HANDLE`), VERIFY-only.
4. Install the SMR (`HSE_SRV_ID_SMR_ENTRY_INSTALL` / `hseSmrEntryInstallSrv_t`): `accessMode = HSE_ACCESS_MODE_ONE_PASS`, `entryIndex` (use **1**, not 0 — SMR#0 is special-cased for SHE-boot/BOOT_MAC_KEY), `pSmrEntry` → `hseSmrEntry_t` with `pSmrSrc`/`smrSize` = the region, `pSmrDest = 0` (in-place auth, no copy), `configFlags = HSE_SMR_CFG_FLAG_INSTALL_AUTH`, `checkPeriod = 0` (on-demand only, no background periodic re-check yet), `authKeyHandle = AES_SMR_VERIFY_NVM_KEY_HANDLE`, `authScheme.macScheme` set the same way as step 2, `pInstAuthTag[0]` = address of the tag from step 2, `versionOffset = HSE_SMR_VERSION_NOT_USED`; also `pSmrData` = the same region address, `smrDataLength` = region size, `pAuthTag[0]`/`authTagLength[0]` = the tag/length from step 2.
5. Erase the transient RAM key (`HSE_SRV_ID_ERASE_KEY`, same shape as `HSE_EraseNvmAesKey()`) — no long-lived key on the device ever carries `HSE_KF_USAGE_SIGN`.
6. Trigger on-demand verification: `HSE_SRV_ID_SMR_VERIFY` / `hseSmrVerifySrv_t` (`entryIndex = 1`, `reserved = 0`, `options = HSE_SMR_VERIFICATION_OPTION_NONE`).

**Open item to verify before writing code** (do not guess past this): `hseSmrEntryInstallSrv_t.smrDataLength`'s doc block states CMAC requires the length to be "a multiple of 64 bytes" for the STREAMING start/update steps, but is ambiguous whether this alignment also applies in ONE-PASS mode (the surrounding table marks `smrDataLength` as used in all modes, but the specific "multiple of 64" language sits under a note that reads as streaming-specific). Confirm this against the actual HSE Reference Manual (referenced repeatedly in these headers as the authority for anything the RTD headers don't fully specify) or by direct experiment with a 64-byte-aligned test buffer in 1a, before assuming an unaligned `__text_end - __text_start` length will be accepted as-is in 1b.

## Functions to add (all in `HSE_SecureBoot.c`/`.h`)

- `hseSrvResponse_t HSE_ImportSmrSignKey_Ram(void)`
- `hseSrvResponse_t HSE_ImportSmrVerifyKey_Nvm(void)` — same "already provisioned?" guard pattern as `HSE_ImportNvmAESKey()` (check via a `HSE_GetSmrVerifyKeyInfo()` call first, skip if already present)
- `hseSrvResponse_t HSE_GetSmrVerifyKeyInfo(void)` — mirrors `HSE_GetNvmAesKeyInfo()`
- `hseSrvResponse_t HSE_ComputeSmrInstallTag(const uint8_t *pRegionStart, uint32_t regionLen)` — writes into module-level `HSE_SmrInstallTag[16]` / `HSE_SmrInstallTagLength`
- `hseSrvResponse_t HSE_EraseSmrSignKey_Ram(void)`
- `hseSrvResponse_t HSE_InstallSmrEntry(uint8_t entryIndex, const uint8_t *pRegionStart, uint32_t regionLen)`
- `hseSrvResponse_t HSE_VerifySmrEntryOnDemand(uint8_t entryIndex)`
- `void HSE_SecureBoot_Phase1_Demo(void)` — single orchestrating function, called once from `APP_Main.c` (mirrors `HSE_Example_StoreEncryptedDataDemo()`), running 1a then 1b in sequence.

Inspectable globals (`extern` in the `.h`): `HSE_ImportSmrSignKeyRamResponse`, `HSE_ImportSmrVerifyKeyNvmResponse`, `HSE_SmrVerifyKeyInfo` (`hseKeyInfo_t`), `HSE_GetSmrVerifyKeyInfoResponse`, `HSE_ComputeSmrInstallTagResponse`, `HSE_SmrInstallTag[16]`, `HSE_SmrInstallTagLength`, `HSE_EraseSmrSignKeyRamResponse`, `HSE_InstallSmrEntryResponse` (1a and 1b variants, or reuse with a suffix per stage), `HSE_VerifySmrEntryResponse` (1a and 1b variants).

## Phase 2 (NOT part of this implementation pass — separate go-ahead required)

Only after Phase 1 is verified working on real hardware, and only with explicit separate approval:
1. Install a Core Reset entry: `HSE_SRV_ID_CORE_RESET_ENTRY_INSTALL` / `hseCrEntryInstallSrv_t` → `hseCrEntry_t` with `preBootSmrMap = (1UL << 1)` (linking SMR#1 from Phase 1), `pPassReset` = the real M7 vector table address (already computed in `startup_cm7.s` as `CM7_0_VTOR_ADDR`), `startOption = HSE_CR_AUTO_START`, `crSanction` — start with `HSE_CR_SANCTION_KEEP_CORE_IN_RESET` rather than `HSE_CR_SANCTION_RESET_SOC`, since a kept-in-reset core is the more directly observable, less confusing failure mode than a reset loop.
2. Only then modify `startup_cm7.s`'s `.boot_header` to set `BOOT_SEQ = 1` in the real IVT — **the exact bit/field for this in HSE's actual IVT format is not yet identified in this codebase**; the current `.boot_header` is explicitly commented as a demo/non-HSE-format placeholder ("need to be defined and customized manually by user. RTD does not provide a tool to configure it"). This needs its own research pass against the HSE Reference Manual before any code is written — flagged, not guessed.
3. Reflash and test with the debug probe attached and a recovery plan ready (see below).

## Prerequisite before Phase 2: fix `HSE_EraseHSE()`

`HSE_EraseHSE()` (`HSE_Main.c`, `#ifdef ERASE_HSE`) is currently broken — it builds a `getAttrReq`-shaped descriptor (`attrId`/`attrLen`/`pAttr`) but sets `srvId = HSE_SRV_ID_ERASE_FW`, which needs its own distinct request structure. Before Phase 2 is ever attempted on real hardware: locate the correct erase-FW request structure in the HSE headers, rewrite `HSE_EraseHSE()` to build it correctly, and bench-test that it actually recovers a board via the debug probe. This is the safety net if Phase 2's `preBootSmrMap` verification fails in a way that keeps the core in reset.

## Verification

**After Phase 1**, check via debugger (same style as existing `HSE_AesRoundTripMatch` inspection):
- `HSE_ImportSmrSignKeyRamResponse`, `HSE_ImportSmrVerifyKeyNvmResponse` (or its already-provisioned-skip path), `HSE_ComputeSmrInstallTagResponse` (`HSE_SmrInstallTagLength == 16`), `HSE_InstallSmrEntryResponse`, `HSE_EraseSmrSignKeyRamResponse`, `HSE_VerifySmrEntryResponse` — all should read `HSE_SRV_RSP_OK`.
- Negative control: deliberately corrupt one byte of the Phase-1a test buffer (or shift the length) and re-run — `HSE_VerifySmrEntryResponse` should become `HSE_SRV_RSP_VERIFY_FAILED`, proving the check isn't a no-op.
- `Hse_Ip_GetHseStatus()`'s `HSE_STATUS_BOOT_OK` bit should **not** appear yet — it's only meaningful once `BOOT_SEQ = 1`, which Phase 1 never touches.

**Before Phase 2** (once separately approved): confirm the debug probe can still fully reflash the board after a deliberately-forced failure; prefer an expendable/spare board for the first real attempt if one exists; re-confirm `HSE_STATUS_CUST_SUPER_USER` is set immediately before the CR install call (`HSE_SYS_AUTH_NVM_CONFIG` SU rights are required for CR install; if not already granted by default CUST_DEL life-cycle, an explicit `HSE_SYS_Authorization_Req`/`Resp` exchange — not yet built anywhere in this codebase — becomes a prerequisite); have the fixed/verified `HSE_EraseHSE()` ready and tested, not theoretical.
