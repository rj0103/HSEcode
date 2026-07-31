/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           :
*   Dependencies         : none
*
*   Autosar Version      : 4.9.0
*   Autosar Revision     : ASR_REL_4_9_REV_0000
*   Autosar Conf.Variant :
*   SW Version           : 7.0.1
*   Build Version        : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
*   Copyright 2020 - 2026 NXP
*
*   NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/
/*
* Target device: This linker is demo and it is using for device S32K312 only. It is not part of the production code deliverables.
* Target core: ARM cortex M7
* Linker need to align with MPU default setup in system.c
*/

/*
* GCC Linker Command File:
* 0x00000000    0x00007FFF  32768   ITCM
* 0x20000000    0x2000FFFF  65536   DTCM
* 0x00400000    0x005FFFFF  2097152 Program Flash
* 0x10000000    0x1001FFFF  131072  Data Flash
* 0x20400000    0x20417FFF  98304  SRAM_0
* Last 176 KB of CODE_FLASH reserved by HSE Firmware
* standby_data section should contain only uninitialized data
*/
HEAP_SIZE  = DEFINED(__heap_size__)  ? __heap_size__  : 0x00000200;
__STANDBY_RAM_LIMIT_END  = 0x20407FFF;  /* 32Kbyte for standby ram */

ENTRY(Reset_Handler)

MEMORY
{
    int_pflash              : ORIGIN = 0x00482000, LENGTH = 0x00020000    /* 128kb app main*/
    int_dflash              : ORIGIN = 0x10000000, LENGTH = 0x00016000    /* 88KB */
    int_itcm                : ORIGIN = 0x00000000, LENGTH = 0x00008000    /* 32KB */
    int_dtcm                : ORIGIN = 0x20000000, LENGTH = 0x0000F000    /* 60KB */
    int_stack_dtcm          : ORIGIN = 0x2000F000, LENGTH = 0x00001000    /* 4KB */
    int_sram                : ORIGIN = 0x20400000, LENGTH = 0x00007F00    /* 32KB , needs to include int_sram_fls_rsv*/
    int_sram_fls_rsv        : ORIGIN = 0x20407F00, LENGTH = 0x00000100
    int_sram_no_cacheable   : ORIGIN = 0x20408000, LENGTH = 0x00007F00    /* 32KB , needs to include int_sram_results  */
    int_sram_results        : ORIGIN = 0x2040FF00, LENGTH = 0x00000100
    int_sram_shareable      : ORIGIN = 0x20410000, LENGTH = 0x00008000    /* 32KB */
    ram_rsvd2               : ORIGIN = 0x20418000, LENGTH = 0             /* End of SRAM */
}

SECTIONS
{

    .pflash :
    {
        KEEP(*(.boot_header))
        . = ALIGN(8192);  /* The minimum erase size of C40 Flash IP is 8kb */
        __text_start = .;
        __interrupts_init_start = .;
        KEEP(*(.intc_vector))
        . = ALIGN(4);
        __interrupts_init_end = .;
        KEEP(*(.core_loop))
        . = ALIGN(4);
        *(.startup)
        . = ALIGN(4);
        *(.systeminit)
        . = ALIGN(4);
        *(.text.startup)
        . = ALIGN(4);
        *(.text)
        *(.text*)
        . = ALIGN(4);
        *(.mcal_text)
        . = ALIGN(4);
        *(.acmcu_code_rom)
        . = ALIGN(4);
        __acfls_code_rom_start = .;
        *(.acfls_code_rom)
        . = ALIGN(4);
        __acfls_code_rom_end = .;
        __acmem_43_infls_code_rom_start = .;
        *(.acmem_43_infls_code_rom)
        . = ALIGN(4);
        __acmem_43_infls_code_rom_end = .;
        KEEP(*(.init))
        . = ALIGN(4);
        KEEP(*(.fini))
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        *(.mcal_const_cfg)
        . = ALIGN(4);
        *(.mcal_const)
        . = ALIGN(4);
        __init_table = .;
        KEEP(*(.init_table))
        . = ALIGN(4);
        __zero_table = .;
        KEEP(*(.zero_table))
    } > int_pflash

    /* Exception-unwind index table. Must be its OWN output section, not folded into .pflash
       above - .ARM.exidx input sections carry SHF_LINK_ORDER ("ordered") attributes tied to
       their .text pieces, and GNU ld hard-errors ("has both ordered and unordered sections") if
       an output section mixes those with regular ("unordered") input sections like .zero_table.
       This used to be an implicit orphan section (the linker placed it wherever its default
       heuristics chose, right after .pflash); now explicit so it lands here, before the
       fixed-size pad below, instead of competing with it for the same leftover space.
       extract_app_slice.bat already extracts this alongside .pflash (objcopy -j .pflash
       -j .ARM.exidx) since it's real flashed content that must be part of what gets
       authenticated/signed. */
    .ARM.exidx :
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        . = ALIGN(4);
        __exidx_end = .;
    } > int_pflash

    /* Real compiled content ends here - for extract_app_slice.bat's __text_end symbol lookup.
       NOT used for __sram_data_rom below - the fixed-size pad immediately following physically
       owns every byte up to the end of int_pflash, so anything else needing flash storage has to
       be placed after that, not inside it. */
    . = ALIGN(4);
    __text_end = .;

    /* Explicitly zero-fill the rest of int_pflash's own declared region, so these bytes are real
       ELF content instead of an implicit gap. Main app is flashed by loading this .elf directly
       via a debugger: only bytes belonging to an actual section get written, so leaving this as
       a gap would leave flash holding whatever was there from a previous build instead of the
       zero bytes tools/sign_tool.py's slice --pad-to assumes when computing the SMR signature
       (see HSE_AppSmrProvision2.c's APP_SMR_REGION_START/SIZE, and extract_app_slice.bat). Sized
       off the MEMORY declaration above, not a literal, so it can never overflow the region.
       KEEP(*(.appsmr_pad)) below pulls in AppSmr_RegionPad.c's tiny anchor array FIRST - a
       location-counter advance with no real input-section content becomes an SHT_NOBITS section
       (like .bss), which the debugger's elf-flash step has nothing to actually write for; the
       anchor's real bytes force this whole section to PROGBITS, so the FILL-covered remainder
       gets materialized as real zero bytes too (confirmed empirically: without the anchor,
       readelf -S showed .appsmr_pad as NOBITS with 0 bytes in the file despite a nonzero size). */
    .appsmr_pad :
    {
        KEEP(*(.appsmr_pad))
        FILL(0x00000000);
        . = ORIGIN(int_pflash) + LENGTH(int_pflash);
    } > int_pflash

    /* .data's flash-resident init storage - deliberately placed AFTER the fixed-size pad above,
       not at the tight __text_end boundary, since that pad now physically owns every byte up to
       the end of int_pflash's own declared 128KB. This spills .data's (small) LMA storage a few
       bytes past that nominal boundary, into the still-unclaimed flash between here and the
       Provisioning app at 0x004C2000 - plenty of headroom, and still real, valid flash. */
    __sram_data_rom = ORIGIN(int_pflash) + LENGTH(int_pflash);

    /* Holds AppSmr_PublicKeyXY/SignatureR/SignatureS (HSE_AppSmrProvision.c) - deliberately
       placed in a wholly separate physical flash bank (Data Flash) from the SMR-measured region
       [__text_start, __text_end) in int_pflash above. HSE_SRV_ID_SMR_ENTRY_INSTALL (with
       HSE_SMR_CFG_FLAG_INSTALL_AUTH) re-checks the live bytes at [__text_start, __text_end)
       against the supplied signature AT INSTALL TIME, not just at verify time - so if this key/
       signature data lived inside that measured range, embedding a real (non-zero) signature
       would itself change the bytes being checked, and install would never succeed (confirmed:
       HSE_SRV_RSP_VERIFY_FAILED every time, and the entry was never actually recorded - checked
       directly via HSE_SMR_CORE_BOOT_STATUS_ATTR_ID). Keeping this data here means the measured
       region is identical bit-for-bit regardless of whether real or placeholder key/signature
       data is compiled in. */
    .appsmr_keydata :
    {
        . = ALIGN(4);
        __appsmr_keydata_start = .;
        KEEP(*(.appsmr_keydata))
        . = ALIGN(4);
        __appsmr_keydata_end = .;
    } > int_dflash

    .sram_bss (NOLOAD) :
    {
        . = ALIGN(8);
        __standby_ram_begin__ = .;
        *(.standby_data)
        . = ALIGN(8);
        __standby_ram_end__ = .;
        . = ALIGN(16);
        __sram_bss_start = .;
        *(.bss)
        *(.bss*)
        . = ALIGN(16);
        *(.mcal_bss)
        . = ALIGN(4);
        __sram_bss_end = .;
    } > int_sram

    .sram_data : AT(__sram_data_rom)
    {
        . = ALIGN(4);
        __sram_data_begin__ = .;
        . = ALIGN(4);
        *(.ramcode)
        . = ALIGN(4);
        *(.data)
        *(.data*)
        . = ALIGN(4);
        *(.mcal_data)
        . = ALIGN(4);
        __sram_data_end__ = .;
    } > int_sram

    __sram_data_rom_end = __sram_data_rom + (__sram_data_end__ - __sram_data_begin__);

    /* heap section */
    .heap (NOLOAD):
    {
        . += ALIGN(4);
        _end = .;
        end = .;
        _heap_start = .;
        . += HEAP_SIZE;
        _heap_end = .;
    } > int_sram

    .acfls_code_ram :
    {
        __acfls_code_ram_start  = .;
        *(.acfls_code_ram)
        __acfls_code_ram_end   = .;
        __acmem_43_infls_code_ram_start = .;
        *(.acmem_43_infls_code_ram)
        __acmem_43_infls_code_ram_end = .;
    } > int_sram_fls_rsv

    __non_cacheable_data_rom = __sram_data_rom_end;

    .non_cacheable_data : AT(__non_cacheable_data_rom)
    {
        . = ALIGN(4);
        __non_cacheable_data_start__ = .;
        *(.mcal_data_no_cacheable)
        . = ALIGN(4);
        *(.mcal_const_no_cacheable)
        . = ALIGN(4);
        HSE_LOOP_ADDR = .;
        LONG(0x0);
        __non_cacheable_data_end__ = .;
    } > int_sram_no_cacheable

    __non_cacheable_data_rom_end = __non_cacheable_data_rom + (__non_cacheable_data_end__ - __non_cacheable_data_start__);

    .non_cacheable_bss (NOLOAD) :
    {
        . = ALIGN(16);
        __non_cacheable_bss_start = .;
        *(.mcal_bss_no_cacheable)
        . = ALIGN(4);
        __non_cacheable_bss_end = .;
    } > int_sram_no_cacheable

    .int_results (NOLOAD):
    {
        . = ALIGN(4);
        KEEP(*(.int_results))
        . += 0x100;
    } > int_sram_results

    __shareable_data_rom = __non_cacheable_data_rom_end;

    .shareable_data : AT(__shareable_data_rom)
    {
        . = ALIGN(4);
        __shareable_data_start__ = .;
        KEEP(*(.mcal_shared_data))
        . = ALIGN(4);
        __shareable_data_end__ = .;
    } > int_sram_shareable

    __shareable_data_rom_end = __shareable_data_rom + (__shareable_data_end__ - __shareable_data_start__);

    .int_vector :
    {
        . = ALIGN(2048);
        __interrupts_ram_start = .;
        . += __interrupts_init_end - __interrupts_init_start;
        . = ALIGN(4);
        __interrupts_ram_end = .;
    } > int_dtcm

    __tcm_data_rom = __shareable_data_rom_end;

    .data_tcm_data : AT(__tcm_data_rom)
    {
        . = ALIGN(4);
        __dtcm_data_start__ = .;
        *(.dtcm_data*)
        . = ALIGN(4);
        __dtcm_data_end__ = .;
    } > int_dtcm

    __tcm_data_rom_end = __tcm_data_rom + (__dtcm_data_end__ - __dtcm_data_start__);

    .bss_tcm_data (NOLOAD) :
    {
        . = ALIGN(4);
        __dtcm_bss_start__ = .;
        *(.dtcm_bss*)
        . = ALIGN(4);
        __dtcm_bss_end__ = .;
    } > int_dtcm

    __tcm_code_rom_start = __tcm_data_rom_end;

   .itcm_text : AT(__tcm_code_rom_start)
    {
        . = ALIGN(4);
        __itcm_start__ = .;
        *(.itcm_text*)
        . = ALIGN(4);
        __itcm_end__ = .;
    } > int_itcm

    __tcm_code_rom_end = __tcm_code_rom_start + (__itcm_end__ - __itcm_start__);

    .shareable_bss (NOLOAD) :
    {
        . = ALIGN(16);
        __shareable_bss_start = .;
        *(.mcal_shared_bss)
        . = ALIGN(4);
        __shareable_bss_end = .;
    } > int_sram_shareable

    __Stack_dtcm_end        = ORIGIN(int_stack_dtcm);
    __Stack_dtcm_start      = ORIGIN(int_stack_dtcm) + LENGTH(int_stack_dtcm);

    __INT_SRAM_START         = ORIGIN(int_sram);
    __INT_SRAM_END           = ORIGIN(ram_rsvd2);

    __INT_ITCM_START         = ORIGIN(int_itcm);
    __INT_ITCM_END           = ORIGIN(int_itcm) + LENGTH(int_itcm);

    __INT_DTCM_START         = ORIGIN(int_dtcm);
    __INT_DTCM_END           = ORIGIN(int_dtcm) + LENGTH(int_dtcm) + LENGTH(int_stack_dtcm);

    __RAM_SHAREABLE_START    = __shareable_data_start__;
    __RAM_SHAREABLE_SIZE     = 0xF;  /* 32kbyte in power of 2 */
    __ROM_SHAREABLE_START    = __shareable_data_rom;
    __ROM_SHAREABLE_END      = __shareable_data_rom_end;
    __RAM_NO_CACHEABLE_START = __non_cacheable_data_start__;
    __RAM_NO_CACHEABLE_SIZE  = 0xF;  /* 32kbyte in power of 2 */
    __ROM_NO_CACHEABLE_START = __non_cacheable_data_rom;
    __ROM_NO_CACHEABLE_END   = __non_cacheable_data_rom_end;
    __RAM_CACHEABLE_START    = __sram_data_begin__;
    __RAM_CACHEABLE_SIZE     = 0xF;  /* 32kbyte in power of 2 */
    __ROM_CACHEABLE_START    = __sram_data_rom;
    __ROM_CACHEABLE_END      = __sram_data_rom_end;
    __ROM_CODE_START         = ORIGIN(int_pflash);
    __ROM_DATA_START         = ORIGIN(int_dflash);

    __BSS_SRAM_START         = __sram_bss_start;
    __BSS_SRAM_END           = __sram_bss_end;
    __BSS_SRAM_SIZE          = __sram_bss_end - __sram_bss_start;

    __BSS_SRAM_NC_START      = __non_cacheable_bss_start;
    __BSS_SRAM_NC_SIZE       = __non_cacheable_bss_end - __non_cacheable_bss_start;
    __BSS_SRAM_NC_END        = __non_cacheable_bss_end;

    __BSS_SRAM_SH_START      = __shareable_bss_start;
    __BSS_SRAM_SH_SIZE       = __shareable_bss_end - __shareable_bss_start;
    __BSS_SRAM_SH_END        = __shareable_bss_end;

    __RAM_INTERRUPT_START    = __interrupts_ram_start;
    __INIT_INTERRUPT_START   = __interrupts_init_start;
    __INIT_INTERRUPT_END     = __interrupts_init_end;

    __RAM_ITCM_START         = __itcm_start__;
    __ROM_ITCM_START         = __tcm_code_rom_start;
    __ROM_ITCM_END           = __tcm_code_rom_end;

    __RAM_DTCM_DATA_START    = __dtcm_data_start__;
    __ROM_DTCM_DATA_START    = __tcm_data_rom;
    __ROM_DTCM_END           = __tcm_data_rom_end;

    __INIT_TABLE             = __init_table;
    __ZERO_TABLE             = __zero_table;

    __RAM_INIT               = 1;
    __ITCM_INIT              = 1;
    __DTCM_INIT              = 1;

    Fls_ACEraseRomStart      = __acfls_code_rom_start;
    Fls_ACEraseRomEnd        = __acfls_code_rom_end;
    Fls_ACEraseSize          = (__acfls_code_rom_end - __acfls_code_rom_start) / 4; /* Copy 4 bytes at a time*/

    Fls_ACWriteRomStart      = __acfls_code_rom_start;
    Fls_ACWriteRomEnd        = __acfls_code_rom_end;
    Fls_ACWriteSize          = (__acfls_code_rom_end - __acfls_code_rom_start) / 4; /* Copy 4 bytes at a time*/

    _ERASE_FUNC_ADDRESS_     = __acfls_code_ram_start;
    _WRITE_FUNC_ADDRESS_     = __acfls_code_ram_start;

    Mem_43_INFLS_ACEraseRomStart         = __acmem_43_infls_code_rom_start;
    Mem_43_INFLS_ACEraseRomEnd           = __acmem_43_infls_code_rom_end;
    Mem_43_INFLS_ACEraseSize             = ((__acmem_43_infls_code_rom_end - __acmem_43_infls_code_rom_start)+3) / 4; /* Copy 4 bytes at a time*/

    Mem_43_INFLS_ACWriteRomStart         = __acmem_43_infls_code_rom_start;
    Mem_43_INFLS_ACWriteRomEnd           = __acmem_43_infls_code_rom_end;
    Mem_43_INFLS_ACWriteSize             = ((__acmem_43_infls_code_rom_end - __acmem_43_infls_code_rom_start)+3) / 4; /* Copy 4 bytes at a time*/

    _MEM_43_INFLS_ERASE_FUNC_ADDRESS_    = __acmem_43_infls_code_ram_start;
    _MEM_43_INFLS_WRITE_FUNC_ADDRESS_    = __acmem_43_infls_code_ram_start;

    __ENTRY_VTABLE           = __INIT_INTERRUPT_START;

    __CORE0_VTOR             = __INIT_INTERRUPT_START;
    __CORE1_VTOR             = __INIT_INTERRUPT_START;
    __CORE2_VTOR             = __INIT_INTERRUPT_START;
    __CORE3_VTOR             = __INIT_INTERRUPT_START;

    __INDEX_COPY_CORE2       = 3;    /* This symbol is used to initialize data of ITCM/DTCM for CORE2 */

    ASSERT(__standby_ram_end__ <= __STANDBY_RAM_LIMIT_END, "Memory for standby ram overflow")

}
