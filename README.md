# ESP32 UART/WiFi Bridge

Firmware para **ESP32-C3 SuperMini** que implementa un puente WiFi bidireccional para comunicación UART. Proporciona conectividad TCP/WiFi a dispositivos serie sin necesidad de adaptador USB-TTL.

## 🌉 Características

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
| Pantalla | OLED 72×40 (SSD1306) |
| Conexión Serial | UART (Serial1) |
| USB | USB-C nativo (CDC) |
| Pines I2C | SDA=5, SCL=6 |
| Pines UART | RX=20, TX=21 |

## 📋 Requisitos

- PlatformIO CLI o VS Code con extensión PlatformIO
- ESP32-C3 SuperMini
- Pantalla OLED SSD1306 (I2C)
- Dispositivo serial para conectar al UART

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

Muestra información de conexión WiFi, IP STA y opción de reset.

### Conexión TCP RAW

El puerto 23 expone un **socket TCP puro** sin protocolo Telnet. No hay negociación ni buffering de línea. Conecta directamente y envía datos sin presionar ENTER.

## ⚙️ Configuración de Cliente

### Obtener la IP del ESP32

**Opción 1: Pantalla OLED**
- Se muestra automáticamente en la pantalla integrada

**Opción 2: Interfaz web**
- Conectarse a `http://192.168.4.1` (AP por defecto)
- Configurar WiFi y se mostrará la IP STA

**Opción 3: Monitor serie**
- Conectar por USB y revisar la salida:
  ```
  ✅ WiFi Conectado: 192.168.1.100
  ```

### Validación de Conectividad

Antes de conectar al puerto RAW, verifica acceso básico:

```bash
# Ping a la IP
ping 192.168.1.100

# Prueba HTTP (debe responder)
curl http://192.168.1.100

# Nmap para verificar puerto 23 abierto
nmap -p 23 192.168.1.100
```

### Cómo Conectarte (sin pedir ENTER)

El puerto 23 es una **conexión TCP pura**, sin protocolo Telnet ni negociación de opciones. Elige tu herramienta:

#### Linux / macOS - Netcat (recomendado)

```bash
nc 192.168.1.100 23
```

Escribe datos directamente. Cada byte se envía sin esperar ENTER.

#### Linux / macOS - Bash Nativo (sin herramientas)

```bash
exec 3<>/dev/tcp/192.168.1.100/23
# Enviar datos
echo -n "DATO" >&3
# Recibir datos
cat <&3
```

#### Windows - PowerShell

```powershell
$socket = New-Object Net.Sockets.TcpClient
$socket.Connect('192.168.1.100', 23)
$stream = $socket.GetStream()

# Enviar datos
$writer = New-Object System.IO.StreamWriter($stream)
$writer.Write("DATO")
$writer.Flush()

# Recibir datos
$reader = New-Object System.IO.StreamReader($stream)
while ($true) {
    $line = $reader.ReadLine()
    if ($null -eq $line) { break }
    Write-Host $line
}

$socket.Close()
```

#### Windows - Netcat

```bash
nc 192.168.1.100 23
```

(Necesita netcat instalado, descargable desde https://nmap.org/ncat/)

#### Multiplataforma - PuTTY (interfaz gráfica)

1. Abre **PuTTY**
2. **Session → Connection type**: selecciona **Raw** (⚠️ NO SSH, NO Telnet)
3. **Host Name**: `192.168.1.100` (reemplaza con tu IP)
4. **Port**: `23`
5. **Terminal → Force local echo**: OFF (importante)
6. **Terminal → Local line editing**: OFF
7. Click **Open**

Escribe datos directamente sin ENTER.

#### Telnet (NO RECOMENDADO)

```bash
telnet 192.168.1.100 23
```

⚠️ **Advertencia**: Telnet añade negociación de opciones que pueden interferir con datos binarios. Usa netcat o modo Raw de PuTTY.

#### Cliente Personalizado (Python)

```python
import socket
import sys
import threading

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.100', 23))
print("Conectado a 192.168.1.100:23")

# Thread para recibir datos del servidor
def receive():
    while True:
        try:
            data = sock.recv(1024)
            if not data:
                break
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
        except:
            break

threading.Thread(target=receive, daemon=True).start()

# Enviar entrada del usuario
try:
    while True:
        data = sys.stdin.buffer.read(1)  # Leer byte a byte
        if data:
            sock.sendall(data)
except KeyboardInterrupt:
    print("\nDesconectado")
finally:
    sock.close()
```

#### Cliente Personalizado (Node.js)

```javascript
const net = require('net');

const socket = net.createConnection(23, '192.168.1.100', () => {
  console.log('Conectado a 192.168.1.100:23');
});

// Recibir datos del servidor
socket.on('data', (data) => {
  process.stdout.write(data);
});

// Enviar datos desde stdin
process.stdin.on('data', (data) => {
  socket.write(data);
});

socket.on('end', () => {
  console.log('Desconectado');
  process.exit(0);
});

socket.on('error', (err) => {
  console.error('Error:', err.message);
  process.exit(1);
});
```

**Características del Puerto RAW (23)**:
- ✅ TCP_NODELAY activado: latencia mínima (< 1ms)
- ✅ Transparencia total: los datos pasan sin procesamiento
- ✅ Bidireccional: envío y recepción simultáneamente
- ✅ Sin protocolo: solo datos puros

### Problemas Comunes de Cliente

| Síntoma | Causa | Solución |
|---------|-------|----------|
| "Connection refused" | Puerto no abierto o IP incorrecta | Verificar IP en OLED, revisar firewall |
| Datos no llegan al dispositivo serial | Búfer lleno o dispositivo desconectado | Verificar conexión física UART |
| Datos recibidos corruptos | Velocidad UART mismatch | Verificar BAUD_RATE (115200 en código) |
| Telnet muestra caracteres extraños | Negociación Telnet interfiere | Usar netcat o modo Raw en PuTTY |
| Latencia alta (> 100ms) | WiFi débil o cliente lento | Acercar ESP32 al router, revisar RSSI |

## 📊 Protocolo de Comunicación

El puente simplemente retransmite datos bidireccionales sin procesamiento:

```
Dispositivo Cliente (TCP/23) ↔ ESP32-C3 ↔ Dispositivo Serial (UART Serial1)
```

- **Velocidad UART**: 115200 bps (configurable en `#define BAUD_RATE`)
- **Tamaño de búfer**: 64 bytes
- **Modo**: No bloqueante (lectura y escritura asincrónicas)
- **Transparencia**: Sin procesamiento ni interpretación de datos

## 🔍 Monitoreo

### Consola Serie

La salida por USB muestra eventos importantes:

```
UART FPGA iniciada
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

> ⚠️ **Advertencia**: Este firmware está diseñado para entorno local de confianza (LAN). La red AP tiene credencial por defecto (`12345678`) y el puente RAW no tiene autenticación.
>
> Para uso en producción o redes públicas:
> - Cambiar contraseña AP en código
> - Implementar autenticación en puente RAW
> - Usar WiFi con WPA3 si es posible
> - Considerar VPN si se expone a Internet

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
| UART no comunica | Verificar velocidad (115200), pines RX/TX del dispositivo serial |
| Datos no pasan | Verificar que el dispositivo esté conectado correctamente al UART |
| Pérdida de WiFi | Aumentar `RECONNECT_MS` en main.cpp |
| Latencia alta | Verificar TCP_NODELAY está activo, revisar RSSI WiFi |
| Búferes llenos | Aumentar tamaño de búfer en código |

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
│ 1. Inicializar UART con dispositivo     │
│ 2. Inicializar pantalla OLED            │
│ 3. Cargar credenciales WiFi guardadas   │
│ 4. Activar modo AP + STA                │
│ 5. Iniciar servidor web + RAW           │
└────────────────┬──────────────────────┘
                 │
         ┌───────▼──────────┐
         │  Loop principal  │
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

## 💡 Casos de Uso

- **Comunicación remota**: Acceder a dispositivos seriales desde la red local
- **IoT**: Conectar sensores/actuadores seriales a WiFi sin hardware adicional
- **Debugging**: Inspeccionar tráfico serial desde cualquier cliente TCP
- **Prototipado**: Alternativa a adaptadores USB-TTL costosos

## 📚 Referencias

- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [ESP32-C3 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [PlatformIO ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [Arduino WiFi API](https://www.arduino.cc/reference/en/libraries/wifi/)

## 📄 Licencia

Especifica según tu proyecto.

---

**Última actualización**: 29 de abril de 2026
