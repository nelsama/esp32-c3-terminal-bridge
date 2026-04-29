# ESP32 UART/WiFi Bridge

Firmware para **ESP32-C3 SuperMini** que implementa un puente WiFi bidireccional para comunicación UART. Proporciona conectividad TCP/WiFi a dispositivos serie sin necesidad de adaptador USB-TTL.

## � Características

- **Puente UART/WiFi transparente**: Transporte TCP ↔ Serial sin procesamiento de datos
- **Modo dual WiFi**: Punto de acceso (AP) + Estación (STA) simultáneamente
- **Pantalla OLED integrada**: Visualización de estado en tiempo real (U8g2)
- **Interfaz web de configuración**: Selector de redes WiFi sin necesidad de código
- **Puerto TCP RAW**: Conexión cruda en puerto 23 con TCP_NODELAY para mínima latencia
- **Persistencia**: Almacenamiento de credenciales WiFi en memoria no volátil
- **Auto-reconexión**: Reintento automático en caso de pérdida de conexión
- **Sin adaptadores**: Elimina la necesidad de convertidores USB-TTL seriales

## 🛠️ Hardware

| Componente | Especificación |
|-----------|---------------|
| Microcontrolador | ESP32-C3 SuperMini |
| Pantalla SerialED 72×40 (SSD1306) |
| Conexión FPGA | UART (Serial1) |
| USB | USB-C nativo (CDC) |
| Pines I2C | SDA=5, SCL=6 |
| Pines UART | RX=20, TX=21 |

## 📋 Requisitos

- PlatformIO CLI o VS Code con extensión PlatformIO
- ESP32-C3 SuperMini
- Pantalla OLED SSD1306 (I2C)
- Conexión UART a FPGA

### Dependencias

```ini
lib_deps =
  U8g2
```

## 🚀 Instalación

### 1. Clonar o descargar el proyecto

```bash
git clone <url-del-repositorio>
cd esp-32-db9-joys
```

### 2. Compilar y cargar

```bash
# Compilar
platformio run -e esp32-c3-supermini

# Compilar y cargar por USB
platformio run -e esp32-c3-supermini --target upload

# Monitorear consola
platformio device monitor --baud 115200
```

### 3. Script de bootloader (opcional)

```bash
./test_bootloader.sh
```

## 📡 Configuración de Red

### Primer acceso (Modo AP)

1. Conectarse a la red WiFi: **ESP32-RAW** (contraseña: `12345678`)
2. Abrir navegador en `http://192.168.4.1`
3. Seleccionar red WiFi disponible y proporcionar contraseña
4. El dispositivo se conectará y mostrará su IP en la pantalla OLED

### Conexión posterior

El dispositivo mantiene las credenciales guardadas y se reconecta automáticamente cada 5 segundos si pierde la conexión.

## 🔌 Interfaz de Cliente

### Acceso Web

```
http://<IP-DEL-ESP32>
```

Muestra información de conexión y proporciona opción de reset.

### Conexión TCP RAW

Use telnet, netcat o herramientas personalizadas:

```bash
# Netcat
nc <IP-DEL-ESP32> 23

# Telnet
telnet <IP-DEL-ESP32> 23

# Bash
exec 3<>/dev/tcp/<IP-DEL-ESP32>/23; cat >&3; cat <&3
```

El puerto RAW (23) está optimizado con **TCP_NODELAY** activado para garantizar latencia mínima (< 1ms).

## 📊 Protocolo de Comunicación

El puente simplemente retransmite datos bidireccionales sin procesamiento:

```
Dispositivo Cliente (TCP/23) ↔ ESP32-C3 ↔ Dispositivo Serial (UART Serial1)
```

- **Velocidad UART**: 115200 bps (configurable)
- **Tamaño de búfer**: 64 bytes
- **Modo**: No bloqueante (lectura y escritura asincrónicas)
- **Transparencia**: Sin procesamiento ni interpretación de datos
 (Python)

```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('<IP-DEL-ESP32>', 23))

# Enviar datos al dispositivo serial
sock.sendall(b'\x01\x02\x03')

# Recibir datos del dispositivo serial
data = sock.recv(64)
print(data.hex())

sock.close()
```

### Ejemplo de uso (Node.js)

```javascript
const net = require('net');

const socket = net.createConnection(23, '<IP-DEL-ESP32>', () => {
  console.log('Conectado');
  socket.write(Buffer.from([0x01, 0x02, 0x03]));
});

socket.on('data', (data) => {
  console.log('Recibido:', data.toString('hex'));
});

socket.on('end', () => {
  console.log('Desconectado');
});
sock.close()
```

## 🔍 Monitoreo

### Consola Serie

La salida por USB muestra eventos importantes: (LAN). La red AP tiene credencial por defecto (`12345678`) y el puente RAW no tiene autenticación.
> 
> Para uso en producción o redes públicas:
> - Cambiar contraseña AP en código
> - Implementar autenticación en puente RAW
> - Usar WiFi con WPA3 si es posible
> - Considerar VPN si se expone a Internet
AP listo: 192.168.4.1
✅ WiFi Conectado: 192.168.1.100
Cliente RAW conectado
🔄 Reconectando...
```

### Pantalla OLED

Estados mostrados:
- Inicialización (ESP32-C3, Bridge RAW, v1.0)
- Modo AP con IP
- Estado WiFi (conectado/desconectado)
- IP STA cuando está conectado

## 🛡️ Seguridad

> ⚠️ **Advertencia**: Este firmware está diseñado para entorno local de confianza. La red AP tiene credencial por defecto (`12345678`). Para uso en producción:
> - Cambiar contraseña AP en códigodel dispositivo serial |
| Datos no pasan | Verificar que el dispositivo esté conectado correctamente al UART |
| Pérdida de WiFi | Aumentar `RECONNECT_MS` en main.cpp |
| Latencia alta | Verificar TCP_NODELAY está activo, revisar RSSI WiFi |
| Búferes llenos | Aumentar `#define` de tamaño de búfer en códig

## 📝 Pines de Configuración

```cpp
#define OLED_SDA 5
#define OLED_SCL 6
#define UART_RX_PIN 20
#define UART_TX_PIN 21
#define BAUD_RATE 115200
#define RAW_PORT 23
```

Edita `src/main.cpp` para cambiar valores según tu hardware.

## 🔧 Troubleshooting

| Problema | Solución |
|----------|----------|
| No aparece en AP | Verificar poder USB, revisar LED en placa |
| OLED no se ve | Comprobar conexión I2C (pines 5, 6) |
| UART no comunica | Verificar velocidad (115200), pines RX/TX |
| Pérdida de WiFi | Aumentar `RECONNECT_MS` en main.cpp |
| Latencia alta | Verificar TCP_NODELAY está activo |

## 📦 Estructura del Proyecto

```
.
├── platformio.ini          # Configuración PlatformIO
├── src/
│   └── main.cpp            # Firmware principal
├── include/                # Headers (si es necesario)
├── lib/                    # Librerías personalizadas
├── test/                   # Tests y scripts de validación
└── test_bootloader.sh      # Script de prueba bootloader
```

## 🔄 Ciclo de Vida de Conexión

```
┌─────────────────────────────────────────┐
│  Inicio (Setup)                         │
├─────────────────────────────────────────┤
│ 1. Inicializar UART con FPGA            │
│ 2ESP32-C3 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [PlatformIO ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [A� Casos de Uso

- **Comunicación remota**: Acceder a dispositivos seriales desde la red local
- **IoT**: Conectar sensores/actuadores seriales a WiFi sin hardware adicional
- **Debugging**: Inspeccionar tráfico serial desde cualquier cliente TCP
- **Prototipado**: Alternativa a adaptadores USB-TTL costosos

## 📄 Licencia

Especifica según tu proyecto
         └────────┬─────────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
    ▼             ▼             ▼
Verificar   Puente RAW    Reconexión
WiFi status (si STA OK)   automática
    │             │             │
    └─────────────┼─────────────┘
                  │
            Actualizar OLED
```

## 📚 Referencias

- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [PlatformIO ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)

## 📄 Licencia

Especifica según tu proyecto.

## 👤 Autor

Generado como análisis de proyecto ESP32.

---

**Última actualización**: 29 de abril de 2026
