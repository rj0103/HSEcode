@echo off
setlocal enabledelayedexpansion

set OBJCOPY="C:\NXP\S32DS.3.6.7\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin\arm-none-eabi-objcopy.exe"
set READELF="C:\NXP\S32DS.3.6.7\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin\arm-none-eabi-readelf.exe"
set ELF=%~dp0Release_FLASH\GSLU_APP.elf

REM Step 1: pull out .pflash (boot_header + code/rodata) AND .ARM.exidx (a small toolchain-
REM generated exception-unwind table the linker places as an orphan section immediately after
REM .pflash - real flashed content, not covered by any name in linker_flash_s32k312_Release.ld,
REM but included in __text_end regardless) as one flat binary.
%OBJCOPY% -O binary -j .pflash -j .ARM.exidx "%ELF%" "%~dp0pflash_full.bin"
if errorlevel 1 (
    echo objcopy failed with exit code %ERRORLEVEL%
    pause
    exit /b 1
)

REM Step 2: look up __text_start/__text_end directly from the just-built ELF instead of a
REM hand-copied address from the .map file - these shift every time the code size changes, so a
REM fixed/hardcoded --start/--end here would silently go stale on every rebuild.
REM Using readelf, not nm/objdump - nm.exe on this machine fails to load one of its own BFD
REM plugin DLLs (bfd-plugins\libdep.so, "Bad Image" / 0xc000012f) before it even runs at all;
REM readelf implements its own ELF parsing and doesn't go through that BFD plugin-loading path,
REM so it isn't affected. Symbol table is written to a temp file first (rather than piped
REM straight into FOR /F) because a FOR /F command that starts with a quoted path confuses
REM cmd.exe's parser ("filename ... syntax is incorrect") - reading back from a file sidesteps
REM that entirely. readelf's symbol table puts the address in the 2nd column (nm: 1st).
%READELF% -s "%ELF%" > "%~dp0nm_symbols.tmp"
set TEXT_START=
set TEXT_END=
for /f "tokens=2" %%A in ('findstr /r /c:" __text_start$" "%~dp0nm_symbols.tmp"') do set TEXT_START=0x%%A
for /f "tokens=2" %%A in ('findstr /r /c:" __text_end$" "%~dp0nm_symbols.tmp"') do set TEXT_END=0x%%A
del "%~dp0nm_symbols.tmp"

if "%TEXT_START%"=="" (
    echo Could not find __text_start symbol in %ELF%
    pause
    exit /b 1
)
if "%TEXT_END%"=="" (
    echo Could not find __text_end symbol in %ELF%
    pause
    exit /b 1
)

echo __text_start = %TEXT_START%
echo __text_end   = %TEXT_END%

REM Step 3: trim off the leading .boot_header bytes, keeping only [__text_start, __text_end),
REM then zero-pad out to APP_SMR_REGION_SIZE (0x1E000 - int_pflash's own 0x20000 LENGTH minus the
REM 0x2000 __text_start already ate for .boot_header - see HSE_AppSmrProvision2.c) so the signed
REM blob's length always matches the fixed SMR region the Provisioning app installs, regardless of
REM how large Main app's actual compiled code is this build.
REM   base = ORIGIN(int_pflash) in linker_flash_s32k312_Release.ld (fixed - the app's own load
REM   address doesn't change, only where __text_end lands within it does).
python "%~dp0tools\sign_tool.py" slice --in "%~dp0pflash_full.bin" --base 0x482000 --start %TEXT_START% --end %TEXT_END% --pad-to 0x1E000 --out "%~dp0app2_main_slice.bin"
echo Exit code: %ERRORLEVEL%
pause
