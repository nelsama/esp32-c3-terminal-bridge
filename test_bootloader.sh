#!/bin/bash

echo "============================================"
echo "Prueba de Bootloader ESP32"
echo "============================================"
echo ""
echo "1. Asegúrate de que el cable USB está conectado"
echo "2. Presiona BOOT y mantenlo"
echo "3. Presiona RESET una vez"  
echo "4. Espera 1 segundo"
echo "5. Suelta BOOT"
echo ""
echo "Presiona ENTER cuando esté en bootloader..."
read

ESPTOOL="/home/nelsama/.platformio/packages/tool-esptoolpy/esptool.py"

for port in /dev/ttyUSB{0,1,2,3,4}; do
    if [ -e "$port" ]; then
        echo ""
        echo "Probando: $port"
        python3 "$ESPTOOL" --port "$port" read_mac 2>&1 | grep -q "MAC" && {
            echo "✓ ¡ENCONTRADO EN $port!"
            echo "Dirección MAC:"
            python3 "$ESPTOOL" --port "$port" read_mac 2>&1 | grep "MAC"
            exit 0
        }
    fi
done

echo ""
echo "✗ No se encontró ESP32 en modo bootloader"
echo "Intenta nuevamente siguiendo los pasos"
