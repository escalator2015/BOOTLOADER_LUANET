#!/bin/bash
# Activar el entorno ESP-IDF 5.5.4 para el proyecto BOOTLOADER_LUANET
# NOTA: Se eliminó dependencia de ESP-ADF
export IDF_PATH=/home/aa/.espressif/v5.5.4/esp-idf
. $IDF_PATH/export.sh
cd /home/aa/vs-workspace/BOOTLOADER_LUANET
exec "$SHELL"