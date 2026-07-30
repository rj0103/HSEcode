@echo off
setlocal enabledelayedexpansion

set OBJCOPY="C:\NXP\S32DS.3.6.7\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin\arm-none-eabi-objcopy.exe"
set NM="C:\NXP\S32DS.3.6.7\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin\arm-none-eabi-nm.exe"
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
REM fixed/hardcoded --start/--end here would silently go stale on every rebuild. Symbol table is
REM written to a temp file first (rather than piped straight into FOR /F) because a FOR /F
REM command that starts with a quoted path confuses cmd.exe's parser ("filename ... syntax is
REM incorrect") - reading back from a file sidesteps that entirely.
%NM% "%ELF%" > "%~dp0nm_symbols.tmp"
set TEXT_START=
set TEXT_END=
for /f "tokens=1" %%A in ('findstr /r /c:" __text_start$" "%~dp0nm_symbols.tmp"') do set TEXT_START=0x%%A
for /f "tokens=1" %%A in ('findstr /r /c:" __text_end$" "%~dp0nm_symbols.tmp"') do set TEXT_END=0x%%A
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

REM Step 3: trim off the leading .boot_header bytes, keeping only [__text_start, __text_end).
REM   base = ORIGIN(int_pflash) in linker_flash_s32k312_Release.ld (fixed - the app's own load
REM   address doesn't change, only where __text_end lands within it does).
python "%~dp0tools\sign_tool.py" slice --in "%~dp0pflash_full.bin" --base 0x482000 --start %TEXT_START% --end %TEXT_END% --out "%~dp0app_image_slice.bin"
echo Exit code: %ERRORLEVEL%
pause
