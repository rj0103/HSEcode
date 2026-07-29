# Plan: Secure Boot Chain — Bootlaoder → GSLU_APP (S32K312 / HSE_B)

## Why this plan exists, and how it differs from SECURE_BOOT_PLAN.md

`SECURE_BOOT_PLAN.md` (Stage 1a, already implemented in `src/HSE_SecureBoot.c`) was written assuming GSLU_APP was the *only* thing that runs after reset — HSE verifies GSLU_APP directly, full stop. That assumption is now wrong: a second project, `Bootlaoder` (a sibling folder, `G:\cw_HSE\workspace\GSLU_APP\Bootlaoder`, its own git repo), already runs first and jumps into GSLU_APP.

That changes the shape of the problem from "HSE verifies one image" to a **two-link chain of trust**:

```
Reset → HSE verifies+releases → Bootlaoder → Bootlaoder verifies+jumps → GSLU_APP
        (hardware check, Phase 2)             (software check, this plan)
```

HSE's hardware boot-time check can only ever gate the *first* thing that runs — that's the bootloader now, not GSLU_APP. For GSLU_APP to be protected at all, the **bootloader itself** must, after HSE has verified and released it, call HSE's on-demand SMR verify service against GSLU_APP's image *before* jumping into it — the exact same primitive already proven in `HSE_VerifySmrEntryOnDemand()`, just invoked from the other side of the chain. Nothing here changes what's already proven in `HSE_SecureBoot.c`; it adds a second, symmetrical check on the bootloader's end.

---

## Blocking issue found first — the two projects currently overlap in flash (RESOLVED)

Before any of the trust-chain work below means anything, this had to be fixed, or flashing both images would corrupt each other. **Status: fixed** via a new `Project_Settings/Linker_Files/linker_flash_s32k312_Release.ld`, used by the project's existing `Release_FLASH` build config (`.cproject:283`).

- Bootlaoder's linker script (`Bootlaoder/Project_Settings/Linker_Files/linker_flash_s32k312.ld:46`) reserves `int_pflash : ORIGIN = 0x00400000, LENGTH = 0x00080000` — comment says "512kb Bootloader space".
- `Bootlaoder/src/main.c:43` hardcodes `#define ADDR_APP 0x00482000UL`. This is **not** `__text_start` — it's read as `*(uint32_t*)(ADDR_APP + 0xC)` (`main.c:97`), which is the `.boot_header`'s "Offset 0x0C: CM7_0 Start address" field (`Bootlaoder/Project_Settings/Startup_Code/startup_cm7.s:201`). Since `.boot_header` is `KEEP()`'d as the very first thing placed in `.pflash` (`linker_*.ld:64`, before `__text_start` at line 66), `ADDR_APP` must equal the app's **`ORIGIN(int_pflash)`**, not its `__text_start`.
- Fix: `linker_flash_s32k312_Release.ld:46` now sets `int_pflash : ORIGIN = 0x00482000, LENGTH = 0x00080000` — `ORIGIN` matches `ADDR_APP` exactly. `__text_start` itself lands 8KB later (after `.boot_header` + the `ALIGN(8192)` at line 65), which is fine — nothing needs `__text_start` at a specific address, only `.boot_header`'s location matters for the bootloader's jump logic. `int_dflash` is unchanged (`0x00016000` / 88KB in both the old and new file — Bootlaoder doesn't touch Data Flash at all yet, so no overlap risk there currently).
- The original `linker_flash_s32k312.ld` (still `ORIGIN = 0x00400000`, still colliding with the bootloader) remains in place for the `Debug_FLASH` config — confirmed intentional, not an oversight.

---

## Stage-by-stage process

**Stage 0 (blocking, see above) — DONE**: GSLU_APP's `Release_FLASH` config now links via `linker_flash_s32k312_Release.ld`, `ORIGIN(int_pflash) = 0x00482000` matching `Bootlaoder/src/main.c`'s `ADDR_APP`. Worth a rebuild + `.map`-file check to confirm `.boot_header` actually lands at `0x00482000` before moving on, but the linker math is correct.

**Stage 1 — Give the Bootloader project HSE awareness (currently has none)**

Confirmed: `Bootlaoder` has **zero** HSE-related code anywhere — no `Hse_Ip` driver, no MU component, no `src/HSE_*.c` files (searched all of `RTD/`, `src/`, `generate/`; the only "HSE" string hits are an unrelated clock-tree enum `HSE_CLK` and comments). It needs, at minimum:
- The `Hse_Ip` driver component (init + `Hse_Ip_GetFreeChannel`/`Hse_Ip_ServiceRequest`), ported over the same way GSLU_APP already uses it.
- Just enough of the pattern from `HSE_VerifySmrEntryOnDemand()` (`src/HSE_SecureBoot.c`) to issue an `HSE_SRV_ID_SMR_VERIFY` request and read back the response — it does **not** need the rest of GSLU_APP's crypto-demo surface (AES encrypt/decrypt, SHA, TRNG, etc.).

**Stage 2 — Register GSLU_APP's real flashed image as its own SMR entry — DONE (code side)**

This is "Region 1b" from the original `SECURE_BOOT_PLAN.md` (`__text_start`/`__text_end`, real code — not the throwaway Data Flash test region from Stage 1a). Implemented in `src/HSE_AppSmrProvision.c`/`.h` as SMR entry index **2** (0 is SHE-boot-special, 1 is `HSE_SecureBoot.c`'s Stage 1a test entry).

**Algorithm decision: ECDSA (secp256r1), not CMAC.** Unlike Stage 1a's demo (which used CMAC/AES-128 to prove the SMR pipeline quickly), this SMR is a real enforcement point once the bootloader's jump depends on it (Stage 3) — so the stronger asymmetric property matters here: the private signing key never needs to exist on any device, only on whatever build/signing setup produces the tag. This reuses the exact primitives already proven working in `src/HSE_Mac_Ecc_Example.c` (`HSE_Ecc_GenerateKeyPair()`/`HSE_Ecc_SignMessage()`/`HSE_Ecc_VerifyMessage()` — `HSE_SIGN_ECDSA`, `HSE_HASH_ALGO_SHA2_256`, curve `HSE_EC_SEC_SECP256R1`). Concretely for the SMR install (`hseSmrEntryInstallSrv_t`, `src/interface/inc_services/hse_srv_smr_install.h`):
- `authScheme` is a union (`hseAuthScheme_t`) of `macScheme`/`sigScheme` — use `.sigScheme` with `signSch = HSE_SIGN_ECDSA`, `sch.ecdsa.hashAlgo = HSE_HASH_ALGO_SHA2_256`, instead of the `.macScheme`/CMAC fields `HSE_SecureBoot.c` uses today.
- `authKeyHandle` becomes a `HSE_KEY_TYPE_ECC_PUB` key, verify-only (`HSE_KF_USAGE_VERIFY`), imported the same way `HSE_Ecc_ImportPublicKey_Ram()` already does — not an AES key.
- `pInstAuthTag`/`pAuthTag` become the (r, s) signature pair (two pointers, matching `hseSignSrv_t.pSignature[2]`'s shape) instead of a single 16-byte CMAC tag.
- Whoever produces the signature (see the provisioning-ownership decision right below) needs the *private* key — which, per the whole point of choosing ECDSA, should not be a key that ever gets imported onto the device at all.

**How the key pair and signature actually get produced — standard software, no HSE-specific tooling needed:**
- Key generation: `openssl ecparam -name prime256v1 -genkey -noout -out app_signing_key.pem` (private, stays on the build machine only) and `openssl ec -in app_signing_key.pem -pubout -out app_public_key.pem` (public, this is what gets imported onto the ECU as the `HSE_KEY_TYPE_ECC_PUB` key). `prime256v1` is OpenSSL's name for `secp256r1`.
- Signing: **OpenSSL's default `openssl dgst -sha256 -sign` output is DER-encoded** (an ASN.1 wrapper around r and s) — HSE's `hseSignSrv_t.pSignature[0]/[1]` need **raw, fixed 32-byte big-endian r and s** instead. The signing step needs to either use a small Python script (the `cryptography` or `ecdsa` package can emit raw r/s directly) or add a DER-unwrap step after `openssl dgst`. This script is the "signing tool" concept from earlier — it runs on the build machine, holds `app_signing_key.pem`, and outputs raw `(r, s)` alongside each build.

**Provisioning-ownership decision: made — option (a')**, a one-time-build variant of (a). Rather than GSLU_APP self-provisioning on *every* boot (option b), or a wholly separate host tool (option a), GSLU_APP itself carries the provisioning code, but only runs it once:
- `src/HSE_AppSmrProvision.c`/`.h`: `HSE_AppSmr_ImportVerifyKey_Nvm()` (imports the public key into the new **NVM** `ECC_PUB` catalog group added to `HSE_Main.c`'s `Hse_aNvmKeyCatalog` — NVM, not RAM, because RAM keys are wiped every reset and this key has to survive across reset for the bootloader's check to ever pass later), `HSE_AppSmr_InstallEntry()` (the SMR install itself), `HSE_AppSmr_VerifyEntryOnDemand()` (a sanity self-check), orchestrated by `HSE_AppSmr_Provision_Demo()`.
- Gated behind a new `RUN_APP_SMR_PROVISIONING` flag in `APP_Main.c` (same idiom as `RUN_SECURE_BOOT_PHASE1_DEMO`/`RUN_MAC_ECC_EXAMPLE`) — build it in once, flash, confirm `HSE_AppSmr_VerifyEntryResponse == HSE_SRV_RSP_OK` via debugger, then leave the flag undefined again for normal builds. The key and SMR entry persist in HSE regardless of whether this code is compiled in on subsequent builds.
- The actual key/signature bytes come from `src/app_smr_provision_data.h`, generated by `tools/sign_tool.py`'s new `header` subcommand (`genkey` → `sign` → `header`). The checked-in version is an **all-zero placeholder** on purpose — a zero-filled EC point isn't a valid public key, so it fails loudly instead of silently "working" if someone forgets to regenerate it before flashing a provisioning build.

**Stage 3 — Change the bootloader's jump logic (`Bootlaoder/src/main.c`)**

Today (`main.c:76-122`): blink an LED 10 times, then unconditionally read the app's VTOR/reset vector at `ADDR_APP` and branch into it — no check of any kind.

Needed: after the existing clock/LED bring-up, call the on-demand SMR verify service (Stage 1's ported primitive) against GSLU_APP's SMR entry (Stage 2) — only run the existing `Boot_SetMSP`/VTOR-read/`bx` sequence (`main.c:97-111`) if the response is `HSE_SRV_RSP_OK`. On failure, do **not** jump — enter a distinguishable, safe failure state (a different LED pattern, an explicit halt) instead of silently falling through into unverified code.

**Hard stop — recovery path, same principle as the original plan, doubly important now**

Confirm the debug probe can still reflash/recover the board *through this new two-image boot chain* before any HSE-side enforcement is turned on. There are now two images to get wrong instead of one, and a bad bootloader flash is exactly as capable of bricking the board as a bad HSE config is.

**Stage 4 — Phase 2 (separate go-ahead, HSE-side enforcement) — one important correction from the original plan**

The original `SECURE_BOOT_PLAN.md` said the eventual Core Reset entry's `pPassReset` should be GSLU_APP's own vector table address (`CM7_0_VTOR_ADDR`, computed in GSLU_APP's `startup_cm7.s`). **That's no longer correct** — since the bootloader now runs first, `pPassReset` must point at the **bootloader's** vector table address instead (`Bootlaoder`'s own `CM7_0_VTOR_ADDR`, i.e. its own `__text_start`-derived address, ≈`0x00402000`). HSE never directly checks GSLU_APP at the hardware level at all in this model — GSLU_APP is protected entirely by the bootloader's software-side SMR check from Stage 3.

Flipping `BOOT_SEQ` in the boot header also now applies to the **bootloader's** `.boot_header` (confirmed byte-for-byte identical unconfigured placeholder to GSLU_APP's, per `Bootlaoder/Project_Settings/Startup_Code/startup_cm7.s:197-261` and its "for demonstration purposes... needs to be defined and customized manually" comment, `LC_CONFIG_ADDR` still `#define`d to `(0)`) — not GSLU_APP's. GSLU_APP's own `.boot_header` becomes irrelevant to HSE's hardware check under this model.

---

## Grounded technical facts (file:line citations)

- Bootlaoder flash origin/length: `Bootlaoder/Project_Settings/Linker_Files/linker_flash_s32k312.ld:46` — `ORIGIN = 0x00400000, LENGTH = 0x00080000`.
- Bootlaoder `__text_start`/`__text_end`: `linker_flash_s32k312.ld:66` / `:113`; confirmed built value `0x00402000` / `0x0040c24c` via `Bootlaoder/Debug_FLASH/Boot_10.map:4031` / `:4425`.
- GSLU_APP flash origin: `Project_Settings/Linker_Files/linker_flash_s32k312.ld:46` — same `ORIGIN = 0x00400000`, `LENGTH = 0x001D4000` (needs to change, see Stage 0).
- `ADDR_APP` hardcoded expectation: `Bootlaoder/src/main.c:43` — `#define ADDR_APP 0x00482000UL`.
- Bootlaoder's `.boot_header`/IVT: `Bootlaoder/Project_Settings/Startup_Code/startup_cm7.s:197-261`, placeholder comment at `:180-196`, `LC_CONFIG_ADDR` at `:120` (`#define ... (0)`).
- `CM7_0_VTOR_ADDR` derivation (identical in both projects' `startup_cm7.s:116`): `#define CM7_0_VTOR_ADDR (__CORE0_VTOR)`, traced through `linker_flash_s32k312.ld:352` (`__CORE0_VTOR = __INIT_INTERRUPT_START`) → `:311` (`__INIT_INTERRUPT_START = __interrupts_init_start`) → `:67` (`__interrupts_init_start`, set immediately after `__text_start` at the same location-counter value) — i.e. each project's own `CM7_0_VTOR_ADDR` equals its own `__text_start`.
- Bootlaoder confirmed targeting the same part: `Bootlaoder/Boot_10.mex:4-5` — `<processor>S32K312</processor>`, `<package>S32K312_172HDQFP</package>`.
- No HSE code exists yet in Bootlaoder: searched all of `RTD/`, `src/`, `generate/` for "hse" (case-insensitive) — only unrelated hits (`HSE_CLK` clock enum in `generate/src/Clock_Ip_Cfg.c:416`; comments/placeholder symbol in the linker script and startup file).

---

## Open items to resolve before implementation starts

1. Whether the bootloader's HSE bring-up needs its own MU channel bookkeeping independent of GSLU_APP's (they run at different times, never concurrently, so this is likely low-risk, but worth confirming against the HSE Reference Manual's guidance on per-boot-stage MU state).
2. **Not yet done**: actually run `tools/sign_tool.py genkey`/`sign`/`header` against a real built GSLU_APP image, regenerate `src/app_smr_provision_data.h` for real, build once with `RUN_APP_SMR_PROVISIONING` defined, flash, and confirm `HSE_AppSmr_VerifyEntryResponse == HSE_SRV_RSP_OK` on real hardware before moving to Stage 1/3 (the bootloader side has nothing to verify against until this has actually run once).
