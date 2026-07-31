/**
 ******************************************************************************
 * @file     AppSmr_RegionPad.c
 * @brief    Anchor for linker_flash_s32k312_Release.ld's .appsmr_pad output section - see that
 *           script's own comment. A location-counter advance with no real input-section content
 *           becomes an SHT_NOBITS section (like .bss): the debugger's elf-flash step then has
 *           nothing to actually write there, leaving flash holding whatever was there from a
 *           previous build instead of the zero bytes the SMR signature (HSE_AppSmrProvision2.c's
 *           APP_SMR_REGION_START/SIZE, tools/sign_tool.py's slice --pad-to) assumes. This tiny
 *           real, non-discardable array gives the section genuine PROGBITS content, so the
 *           linker's subsequent pad-to-region-end advance for the remainder is materialized as
 *           real zero bytes too.
 * @location /test/src/AppSmr_RegionPad.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#include <stdint.h>

static const uint8_t AppSmrPadAnchor[4] __attribute__((section(".appsmr_pad"), used)) = {0U, 0U, 0U, 0U};

/** @} */
