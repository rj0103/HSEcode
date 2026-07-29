@echo off
"C:\NXP\S32DS.3.6.7\S32DS\build_tools\gcc_v11.4\gcc-11.4-arm32-eabi\bin\arm-none-eabi-objcopy.exe" -O binary --start-address=0x484000 --stop-address=0x496998 "%~dp0Release_FLASH\GSLU_APP.elf" "%~dp0app_image_slice.bin"
echo Exit code: %ERRORLEVEL%
pause
