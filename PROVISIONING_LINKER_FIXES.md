# Linker Fixes and Bring-Up Process: Getting SMR Install to Actually Succeed

This documents what changed in `linker_flash_s32k312_Release.ld` (and the supporting source
files) to get `HSE_SRV_ID_SMR_ENTRY_INSTALL` to succeed for Main app's real SMR entry, and the
debugging process that got there. Written after the fact, once the chain was confirmed working
end-to-end on hardware (Provisioning app installs → Bootlaoder verifies → Main app boots).

## The architecture this supports

Three images flashed together at the EOL/manufacturing station:

- **Bootlaoder** (`0x00400000`) - verify-only, no install/bypass code ever.
- **Main app** (`GSLU_APP`, `Release_FLASH`, `0x00482000`) - the clean production application.
  Zero SMR-install capability, key material, or signature data compiled in.
- **Provisioning app** (`GSLU_APP`, `Flash_Load_KEY` build config, `0x004C2000`) - factory/EOL
  only, never shipped. Imports Main app's real offline-signed verify key into HSE NVM and
  installs Main app's SMR entry, then halts.

This works because all three images' real bytes are already physically present in flash by the
time anything runs - see the next section for why that specifically matters.

## Why this was hard: install authenticates live flash, not a copy

`HSE_SRV_ID_SMR_ENTRY_INSTALL` (with `HSE_SMR_CFG_FLAG_INSTALL_AUTH`) authenticates the **live
bytes physically present in flash** at the declared region, **at install time** - not a copy
handed to it, and not deferred to the first verify. Whatever region gets declared to
`HSE_AppSmrProvision2.c`'s `HSE_AppSmr2_InstallEntry()` has to contain, byte-for-byte, exactly
what `tools/sign_tool.py` computed the signature over. Any mismatch - wrong start address, wrong
length, or any byte anywhere in the region differing - produces
`HSE_SRV_RSP_VERIFY_FAILED (0x55A5A164)` at install time, with no indication of *which* byte was
wrong.

Getting a byte-for-byte match required fixing three separate, independently-discovered problems.

## Problem 1: region start was inside `.boot_header`

`HSE_AppSmrProvision2.c` originally declared `APP_SMR_REGION_START = 0x00482000UL` -
`ORIGIN(int_pflash)`, the very start of Main app's flash region. But the linker script's own
`.pflash` section places `.boot_header` first, then rounds up to the next 8KB sector boundary
*before* `__text_start`:

```ld
.pflash :
{
    KEEP(*(.boot_header))
    . = ALIGN(8192);      /* rounds up to the next 8KB sector */
    __text_start = .;
    ...
```

That lands `__text_start` at `0x00484000` - 8KB *after* `ORIGIN(int_pflash)`. `sign_tool.py`
(via `extract_app_slice.bat`) signs starting at `__text_start`, not `ORIGIN`. So HSE was
authenticating a region that started 8KB before what was actually signed - completely different
bytes, guaranteed failure regardless of keys or signature correctness.

**Fix:** `APP_SMR_REGION_START` corrected to `0x00484000UL` in `HSE_AppSmrProvision2.c`.

## Problem 2: region size overflowed `int_pflash`'s own declared bounds

The region size went through two wrong values before landing on the right one:

- First `0x00040000` (256KB) - stale, based on an earlier assumption that Main app's SMR region
  spanned the whole gap up to the Provisioning app at `0x004C2000`.
- Then corrected to `0x00020000` (128KB) to match `int_pflash`'s declared `LENGTH` in the
  `MEMORY` block - but this didn't account for `__text_start` already having consumed `0x2000`
  of that `LENGTH` for `.boot_header`. Padding a full `0x20000` bytes *from* `__text_start`
  overflows past `int_pflash`'s own end (`ORIGIN + LENGTH = 0x004A2000`) by exactly `0x2000`
  bytes - this is what produced the linker error `region 'int_pflash' overflowed by 8 bytes`
  (units differ, same root cause).

**Fix:** `APP_SMR_REGION_SIZE` corrected to `0x0001E000UL` (120KB) - exactly
`int_pflash`'s remaining space after `.boot_header`. `extract_app_slice.bat`'s
`sign_tool.py slice --pad-to` value was updated to match (`0x1E000`, not `0x20000`).

## Problem 3: the padding tail was never actually written to flash

Even with the region bounds correct, `tools/sign_tool.py`'s `slice --pad-to` zero-pads the
*signed file* out to the fixed region size - but that's a tool-side operation on a `.bin`, not
something that touches the device. Main app is flashed by loading the `.elf` directly via
debugger, which only writes bytes belonging to real ELF sections. The gap between Main app's
actual compiled content (`__text_end`) and the fixed region boundary was never covered by any
section - live flash held whatever was there from a previous build (confirmed directly: a
decoded S-record memory dump showed a repeating `0xF1D4D633` pattern there, not the zeros the
signature assumed).

Fixing this required three linker-level changes, each one surfacing the next:

### 3a. Make the padding real ELF content

Restructured `.pflash` to explicitly advance the location counter out to
`ORIGIN(int_pflash) + LENGTH(int_pflash)` (symbolic, not a hardcoded address, so it tracks the
`MEMORY` declaration automatically) with an explicit `FILL(0x00000000)`:

```ld
.appsmr_pad :
{
    KEEP(*(.appsmr_pad))
    FILL(0x00000000);
    . = ORIGIN(int_pflash) + LENGTH(int_pflash);
} > int_pflash
```

### 3b. `.ARM.exidx` needed its own output section

Once the pad consumed all remaining space in `int_pflash`, there was no room left for
`.ARM.exidx` - previously placed there implicitly by the linker's default orphan-section
heuristics (`region int_pflash overflowed by 8 bytes`, and separately
`.pflash section will not fit in region int_pflash`). Worse, when `.ARM.exidx` was first moved
inside `.pflash` itself (to reclaim the space), linking failed with
`.pflash has both ordered and unordered sections` - `.ARM.exidx` input sections carry an
`SHF_LINK_ORDER` ("ordered") attribute tied to their originating `.text` pieces that GNU ld
refuses to mix into the same output section as regular ("unordered") content like
`.zero_table`. Fixed by giving `.ARM.exidx` its own explicit output section, placed between real
`.pflash` content and the pad:

```ld
.ARM.exidx :
{
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    . = ALIGN(4);
    __exidx_end = .;
} > int_pflash
```

`extract_app_slice.bat` already extracts this alongside `.pflash`
(`objcopy -j .pflash -j .ARM.exidx`) since it's real flashed content inside the signed range.

### 3c. `.data`'s flash-resident init storage collided with the pad

`__sram_data_rom` (the flash address `.data`'s initial values are copied from at startup) was
still computed at the tight `__text_end` boundary - which the padding above now physically
occupies. This produced `section .sram_data LMA [...] overlaps section .pflash LMA [...]`.
Fixed by moving `__sram_data_rom` to *after* the padded region:

```ld
__sram_data_rom = ORIGIN(int_pflash) + LENGTH(int_pflash);
```

This spills `.data`'s (small) LMA storage a few bytes past `int_pflash`'s nominal 128KB boundary,
into the still-unclaimed flash before the Provisioning app at `0x004C2000` - plenty of headroom,
still real valid flash.

### 3d. The pad section still wasn't real content

After all of the above, the build linked clean - but `readelf -S Release_FLASH\GSLU_APP.elf`
showed `.appsmr_pad` as `NOBITS` (like `.bss`), not `PROGBITS`, with **0 bytes actually in the
file** despite a nonzero declared size. A location-counter advance with no real input-section
content gets optimized by the linker into "just reserve this address range" rather than real
file bytes - so the debugger's elf-flash step still had nothing to write there, silently
reproducing problem 3's original symptom.

**Fix:** added a tiny real anchor array,
[`src/AppSmr_RegionPad.c`](src/AppSmr_RegionPad.c):

```c
static const uint8_t AppSmrPadAnchor[4] __attribute__((section(".appsmr_pad"), used)) = {0U, 0U, 0U, 0U};
```

pulled in via `KEEP(*(.appsmr_pad))` at the top of the `.appsmr_pad` section (3a, above). Having
*some* real content forces the whole section to `PROGBITS`, so the `FILL`-covered remainder
after it is materialized as real zero bytes too. Confirmed via `readelf -S` before moving on:
`.appsmr_pad PROGBITS ... 008414` (real bytes, not `NOBITS`).

## Everything else that had to line up

Getting the linker to produce a correct, byte-matching image wasn't sufficient on its own -
these had to be fixed alongside it:

- **`Hse_aNvmKeyCatalog`'s `FORCE_KEY_CATALOG_REFORMAT`** (`HSE_Main.c`) was unconditional,
  meaning Main app itself would reformat (wipe) the NVM catalog on every one of its own boots -
  destroying the verify key Bootlaoder needs on the *next* reset. Scoped to
  `#ifdef PROVISION_APP_BUILD` so only the Provisioning app ever force-reformats.
- Even after that, Main app's fallback "reformat if not already installed" path tried to
  reformat *already-populated* catalogs and failed with `HSE_SRV_RSP_GENERAL_ERROR
  (0x33D6D4F1)` - `HSE_STATUS_INSTALL_OK` isn't reliable across a Bootlaoder-mediated jump.
  Main app now skips catalog formatting unconditionally; only the Provisioning app ever touches
  it.
- **The sign/slice/header pipeline has a strict order dependency**: Main app must be built and
  flashed *first* (so its real final bytes are what gets extracted), then
  `extract_app_slice.bat` → `sign_tool.py sign` → `sign_tool.py header` regenerates
  `src/app_smr_2_provision_data.h`, then the Provisioning app (`Flash_Load_KEY`) is rebuilt so it
  picks up the new header, then all three images are reflashed together. Any linker-level change
  to Main app (including everything in this document) invalidates the previous
  slice/signature/header and requires redoing this whole sequence.

## The empirical debugging pattern that found all of this

Every one of the fixes above was confirmed with a concrete artifact, not inferred from source
reading alone:

- `HSE_SRV_RSP_VERIFY_FAILED` responses (Problems 1 & 2) were narrowed down by cross-referencing
  the linker script's own `.boot_header`/`ALIGN(8192)` logic against `int_pflash`'s `MEMORY`
  declaration - i.e., computing exactly where `__text_start` and the region end actually land,
  rather than assuming the constants already in `HSE_AppSmrProvision2.c` were right.
- The stale-flash-tail theory (Problem 3) was confirmed by decoding a raw S-record memory dump
  the user pasted from a debugger memory view - the repeating `0xF1D4D633` byte pattern was
  directly visible sitting where zeros were expected.
- The `NOBITS` section bug (3d) was confirmed by running `arm-none-eabi-readelf -S` and
  `-l` on the actual built `.elf` and reading the `FileSiz`/`MemSiz` mismatch in the `LOAD`
  program header directly, rather than guessing at linker semantics.
- Once `InstallEntry`/`VerifyEntry` both returned `HSE_SRV_RSP_OK (0x55A5AA33)` and Bootlaoder's
  `AppVerified: true` led to a successful jump into Main app, the chain was considered proven.

## Current state (region constants, for reference)

| Constant | Value | Defined in |
|---|---|---|
| `APP_SMR_REGION_START` | `0x00484000` | `src/HSE_AppSmrProvision2.c` |
| `APP_SMR_REGION_SIZE` | `0x0001E000` (120KB) | `src/HSE_AppSmrProvision2.c` |
| `--pad-to` (slice step) | `0x1E000` | `extract_app_slice.bat` |
| `int_pflash` `ORIGIN`/`LENGTH` | `0x00482000` / `0x00020000` | `Project_Settings/Linker_Files/linker_flash_s32k312_Release.ld` |
| `__text_start` (post `.boot_header`) | `0x00484000` | computed by the linker script |
