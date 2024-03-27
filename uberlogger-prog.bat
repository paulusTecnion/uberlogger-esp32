@echo off
SET BIN_FILE=ota_support.bin
SET FLASH_ADDRESS=0x08000000
SET CUBE_PROGRAMMER_PATH="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

@echo off
SETLOCAL

:: Welcome text
echo Welcome to the Uberlogger programmer script

:: Check if COM port is provided
IF "%~1"=="" (
    echo No COM port provided. Usage: flash_devices.bat COMPORT
    GOTO :EOF
)

:: Set variables
SET COMPORT=%~1
SET STM32_BIN_FILE=your_stm32_firmware.bin

:: Flash STM32
echo Flashing STM32...
:: Set nBOOT_SEL to 0 here, modify based on your tool or method
:: For example, using STM32CubeProgrammer CLI (adjust according to your actual configuration)
%CUBE_PROGRAMMER_PATH% -c port=SWD mode=UR -ob nBOOT_SEL=0

:: Flash the .bin file
%CUBE_PROGRAMMER_PATH% -c port=SWD -w %BIN_FILE% %FLASH_ADDRESS% -v -rst

:: Flash ESP32-S2
echo Flashing ESP32-S2...
:: Ensure you're in the ESP32 code repository directory before executing this script
idf.py flash -p %COMPORT% 
IF %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to flash ESP32-S2. Please check your setup and try again.
    GOTO :EOF
)

:: End of script
echo Flashing complete.
ENDLOCAL