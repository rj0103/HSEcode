@echo off
REM Step 1: pull out .pflash (boot_header + code/rodata) AND .ARM.exidx (a small toolchain-
REM generated exception-unwind table the linker places as an orphan section immediately after
REM .pflash - real flashed content, not covered by any name in linker_flash_s32k312_Release.ld,
REM but included in __text_end regardless) as one flat binary.
"C:\NXP\S32DS.3.6.7\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin\arm-none-eabi-objcopy.exe" -O binary -j .pflash -j .ARM.exidx "%~dp0Release_FLASH\GSLU_APP.elf" "%~dp0pflash_full.bin"
if errorlevel 1 (
    echo objcopy failed with exit code %ERRORLEVEL%
    pause
    exit /b 1
)

REM Step 2: trim off the leading .boot_header bytes, keeping only [__text_start, __text_end).
REM   base  = ORIGIN(int_pflash) in linker_flash_s32k312_Release.ld
REM   start = __text_start from Release_FLASH\GSLU_APP.map
REM   end   = __text_end   from Release_FLASH\GSLU_APP.map
python "%~dp0tools\sign_tool.py" slice --in "%~dp0pflash_full.bin" --base 0x482000 --start 0x484000 --end 0x496998 --out "%~dp0app_image_slice.bin"
echo Exit code: %ERRORLEVEL%
pause
