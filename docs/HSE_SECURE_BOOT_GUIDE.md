# Guide: Understanding HSE-Based Secure Boot (NXP S32K3xx / HSE_B)

This is a conceptual reference guide, not a task list for a specific project. It explains *what* HSE secure boot is, the building blocks involved, and the general order operations happen in on any S32K3xx part with the HSE_B firmware — so it applies whether you're looking at this codebase or a different HSE project entirely. For the concrete, file-and-line-specific implementation steps for *this* repository, see `SECURE_BOOT_PLAN.md` at the repo root.

---

## 1. What "Secure Boot" Actually Means

On a normal microcontroller, the CPU core comes out of reset and starts executing whatever code is sitting in flash — no questions asked. If an attacker (or a bad OTA update, or a corrupted flash write) replaces that code, the core happily runs it.

Secure boot inserts a gate before that happens: a piece of code the application *cannot* modify (the HSE firmware, running on a separate, isolated core/enclave) cryptographically checks that the flashed image matches what was authorized, and only then allows the application core to leave reset. If the check fails, the core is held in reset (or the SoC is reset) instead of running unverified code.

Two properties matter:
- **Authenticity** — the image was produced/signed by someone holding the right key.
- **Integrity** — the image hasn't been altered since it was signed.


A CMAC or a digital signature over the image, checked before release from reset, gives you both.

## 2. HSE Architecture Primer

HSE (Hardware Security Engine) is a separate on-chip subsystem — its own core, its own firmware, its own key storage — that the application core talks to over a message-unit (MU) interface, using request/response descriptors. The application never has direct access to key material; it can only ask HSE to perform operations (import a key, compute a MAC, verify a region) and read back a response code.

Two independent capabilities matter for secure boot:
- **Key management** — importing, generating, and storing cryptographic keys in protected catalogs, usable for everyday crypto operations (encrypt/decrypt, MAC) regardless of secure boot.
- **Boot enforcement** — HSE's own boot-time logic, which runs *before* the application core is released, checking configured memory regions and deciding whether to release the core.

These two capabilities share underlying primitives (keys, MAC schemes) but are configured through separate mechanisms — SMR/CR install services versus everyday crypto services.

## 3. Core Building Blocks

**Key catalogs.** HSE stores keys in indexed slots grouped into catalogs — typically a RAM catalog (volatile, cleared on reset/power-loss) and an NVM catalog (persists across resets, survives power cycles). Each key handle carries usage flags (`SIGN`, `VERIFY`, `ENCRYPT`, `DECRYPT`, ...) that restrict what operations it's allowed to perform — a key flagged verify-only can check a tag but can never mint a new one.

**Authentication scheme: symmetric vs asymmetric.**
- *CMAC (AES-based)* — sign and verify use the *same* secret key. Simple, reuses ordinary AES infrastructure, no PKI needed. The catch: the raw key exists somewhere off-device (wherever it was generated) before import, and that same key value can both produce and check tags — so a device that's been fully compromised once could theoretically re-sign data if the sign-capable key were ever present on it. Appropriate for early bring-up, single-device development, or closed systems where key distribution risk is already tightly controlled.
- *ECDSA (or similar asymmetric signing)* — only a *public* key is ever provisioned onto the device (verify-only, always). The private signing key lives on a build/signing server and never touches the target. This is what real production secure boot systems use, because compromising a fielded device can never yield the ability to forge new valid images.

Pick CMAC for proving the mechanism works and for learning/bring-up; plan to move to asymmetric signing before treating any deployment as production-secure.

**SMR — Secure Memory Region.** This is HSE's record of "here's a memory range, here's its expected size, here's the key and scheme to check it with, here's the expected tag." Installing an SMR entry does not, by itself, change boot behavior — it just teaches HSE how to verify a region on request. Verification can be triggered on-demand (a function call, for testing) or automatically at boot (once wired into the Core Reset table).

**CR — Core Reset table.** This is the actual enforcement link. A Core Reset entry says "before releasing this core from reset, check these SMR(s); if any fail, do X" where X is typically "keep the core in reset" or "reset the whole SoC." Until an SMR is referenced by a CR entry, its pass/fail result has no effect on boot — it's purely informational.

**Boot header / IVT flag.** The flashed image's interrupt vector table (or a header near it) contains a field HSE's own boot ROM/firmware reads *before* your code runs, indicating whether secure boot is enabled at all. Getting this field's bit layout wrong is one of the most common ways to brick a board, because it's read by code you don't control, before anything you've built gets a chance to run or report an error.

## 4. The General Workflow (Any HSE Secure-Boot Project)

Regardless of project specifics, HSE secure boot bring-up follows the same shape, and each stage should be fully proven before moving to the next:

**Stage 1 — Prove the crypto pipeline, zero boot risk.**
1. Import a temporary sign+verify key (RAM catalog — cleared on reset, never meant to persist).
2. Compute an authentication tag (CMAC or signature) over a *throwaway* test region — not your real application image. Using scratch data means you can safely corrupt it later without any risk to the running system.
3. Import the real verification key, permanently, into NVM, flagged verify-only.
4. Install an SMR entry describing that test region, the verify key, and the expected tag.
5. Erase the temporary sign-capable key — nothing sign-capable should linger on the device.
6. Trigger on-demand verification and confirm it reports success.
7. **Negative control**: deliberately corrupt the test region and re-verify — confirm it now reports failure. Skipping this step means you have no evidence the check does anything at all; a pipeline that always reports "pass" is indistinguishable from a correctly-working one until you test the failure path.

At the end of Stage 1, nothing about the boot process itself has changed. The board still boots exactly as before, unconditionally — you've only proven the sign→install→verify mechanism is sound.

**Hard stop.** Before enabling any real enforcement, make sure there is a tested, working way to recover the board if enforcement misbehaves (see Section 6). Do not skip this because Stage 1 "went fine" — Stage 2 is a different risk category entirely.

**Stage 2 — Turn on enforcement (separate go-ahead, higher risk).**
8. Point the SMR at the real application image region (not the throwaway test data) and re-run Stage 1's steps against it.
9. Install a Core Reset entry linking that SMR to the core's release decision — choose "keep core in reset" over "reset SoC" as the failure action while you're still validating, since a halted core is a far more diagnosable state than a reset loop.
10. Flip the secure-boot-enabled bit in the flashed boot header/IVT, matching the exact bit layout your part's HSE reference manual specifies — never guess this field.
11. Reflash and test, ideally on hardware you can afford to lose, with the recovery path (Section 6) verified and ready.

## 5. Life Cycle and Authorization

HSE parts typically implement a life-cycle state machine (e.g., unprovisioned → in-field → customer-deliverable states), and some operations — particularly installing Core Reset entries or other boot-critical configuration — require an elevated authorization/super-user state to be active first. Check what life-cycle state your part is in and what authorization exchange (if any) is a prerequisite before attempting CR install; assuming default rights are sufficient without confirming can produce a confusing "service refused" response late in the process.

## 6. Recovery Planning — Non-Negotiable Before Enforcement

Before enabling any boot-time enforcement:
- Confirm there's a working, tested path to erase HSE's configuration / reflash the board via a debug probe or bootloader, independent of the application core booting successfully.
- Test that recovery path *before* you need it — a "should work" recovery mechanism that's never been exercised is not a safety net.
- Prefer testing enforcement first on hardware you can afford to lose or that has a known-good recovery path, not your only board.

## 7. Testing Methodology

Two checks matter at every stage, not just once at the end:
- **Positive control** — the mechanism reports success on unmodified, correctly-signed data.
- **Negative control** — the mechanism reports *failure* on deliberately corrupted data. Without this, "it passed" is unfalsifiable — you can't distinguish a working check from a check that always returns success.

Run both after every stage where behavior could plausibly regress, not just once at the very end.

## 8. Common Pitfalls

- **Skipping the negative-control test.** A verify call that always returns success looks identical to a correctly-working one until you test the failure path.
- **Pointing the SMR at the real code image before the pipeline is proven.** Any mistake in region bounds, alignment, or tag computation is then a mistake against the thing that has to boot.
- **Guessing the boot header bit layout.** This field is read by firmware you don't control, before your code can log anything — a wrong guess here fails silently, from your code's perspective.
- **Enabling Core Reset enforcement without a tested recovery path.** This is the step that turns "the check failed" into "the board won't boot."
- **Leaving a sign-capable key on the device after tag generation.** Only verify-only keys should persist; sign-capable keys should be transient and erased once their one job is done.
- **Treating CMAC as a production posture.** It proves the mechanism; it does not give you the security property (private key never on-device) that asymmetric signing does.
- **Length/alignment assumptions.** Some MAC schemes have alignment requirements (e.g., block-size multiples) that apply differently across streaming vs one-pass API modes — confirm against the reference manual rather than assuming a real image's size will "just work."

## 9. Where to Look for Authoritative Details

Struct field meanings, exact service IDs, and bit-level formats (especially the boot header/IVT and any life-cycle/authorization requirements) are chip- and firmware-version-specific. Treat the vendor's HSE Reference Manual as the authority whenever the RTD/SDK headers are ambiguous or silent — don't infer these from example code or by analogy with a different part number.
