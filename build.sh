#!/bin/bash

# Check if the -n flag is present
NO_FLASH=false
if [[ "$1" == "-n" ]]; then
  NO_FLASH=true
fi

# Build and flash the firmware if the flag is not present
if [[ "$NO_FLASH" == "false" ]]; then
  idf.py build flash -p /dev/ttyACM0
else
  idf.py build
fi

# Copy the binaries to the root folder and rename them
cp -n build/uberlogger-esp32.bin ota_main.bin
cp -n build/www.bin ota_filesystem.bin

# Print a success message
echo "Output file ota_main.bin and ota_filesystem.bin copied successfully to desktop."

