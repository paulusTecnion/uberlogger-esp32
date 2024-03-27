@echo off
SET BIN_FILE=ota_support.bin
SET FLASH_ADDRESS=0x08000000
SET CUBE_PROGRAMMER_PATH="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

@echo off
SETLOCAL

:: Welcome text
echo ======== Welcome to the Uberlogger programmer script ========
echo This tool requires esptool in the root and the ota_support.bin and firmware.bin files as well.
echo Next to that, make sure that the path to STM32_Programmer_CLI is in %CUBE_PROGRAMMER_PATH%
echo =============================================================

:: Check if COM port is provided
IF "%~1"=="" (
    echo No COM port provided. Usage: uberlogger-prog.bat COMPORT
    GOTO :EOF
)

:: Flash STM32
echo Flashing STM32...
:: Set nBOOT_SEL to 0 here, modify based on your tool or method
:: For example, using STM32CubeProgrammer CLI (adjust according to your actual configuration)
%CUBE_PROGRAMMER_PATH% -c port=SWD mode=UR -ob nBOOT_SEL=0

:: Flash the .bin file
%CUBE_PROGRAMMER_PATH% -c port=SWD -w %BIN_FILE% %FLASH_ADDRESS% -v -rst

:: Flash ESP32-S2
echo Flashing ESP32-S2...
.\esptool.exe --chip esp32s2 --baud 921600 --port %COMPORT% --before usb_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0000 .\firmware.bin
GOTO DONE

:: End of script
echo Flashing complete.
ENDLOCAL