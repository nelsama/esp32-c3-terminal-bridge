# ESP32 UART/WiFi Bridge

Firmware para **ESP32-C3 SuperMini** que implementa un puente WiFi bidireccional para comunicación UART. Proporciona conectividad TCP/WiFi a dispositivos serie sin necesidad de adaptador USB-TTL.

## 🌉 Características

- **Puente UART/WiFi transparente**: Transporte TCP ↔ Serial sin procesamiento de datos
- **Modo dual WiFi**: Punto de acceso (AP) + Estación (STA) simultáneamente
- **Pantalla OLED integrada**: Visualización de estado en tiempo real (U8g2)
- **Interfaz web de configuración**: Selector de redes WiFi sin necesidad de código
- **Puerto Telnet (23)**: Conexión TCP con TCP_NODELAY para mínima latencia (< 1ms)
- **LED de estado inteligente**: Hilo RTOS independiente con patrones dinámicos
- **Persistencia**: Almacenamiento de credenciales WiFi en memoria no volátil
- **Auto-reconexión**: Reintento automático en caso de pérdida de conexión (cada 5s)
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

El puerto 23 expone un **socket Telnet** con TCP_NODELAY activado (no hay buffering de Nagle). Conecta directamente y envía datos sin presionar ENTER.

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

El puerto 23 es una **conexión Telnet** con `TCP_NODELAY` activado (sin algoritmo de Nagle). Los datos se envían inmediatamente, ideal para entrada de baja latencia. Elige tu herramienta:

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

**Características del Puerto Telnet (23)**:
- ✅ TCP_NODELAY activado: latencia mínima (< 1ms)
- ✅ Transparencia total: los datos pasan sin procesamiento
- ✅ Bidireccional: envío y recepción simultáneamente
- ✅ Sin buffering de Nagle: cada byte viaja inmediatamente

### Problemas Comunes de Cliente

| Síntoma | Causa | Solución |
|---------|-------|----------|
| "Connection refused" | Puerto no abierto o IP incorrecta | Verificar IP en OLED, revisar firewall |
| Datos no llegan al dispositivo serial | Búfer lleno o dispositivo desconectado | Verificar conexión física UART |
| Datos recibidos corruptos | Velocidad UART mismatch | Verificar BAUD_RATE (115200 en código) |
| Telnet muestra caracteres extraños | Negociación Telnet interfiere | Usar netcat o modo Raw en PuTTY |
| Latencia alta (> 100ms) | WiFi débil o cliente lento | Acercar ESP32 al router, revisar RSSI |
| LED siempre apagado | Firmware no inició correctamente | Comprobar consola serie, verificar voltaje |
| LED no parpadea con datos | `lastActivityTime` no se actualiza | Verificar conexión Telnet y tráfico UART |

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

### Pantalla OLED

Estados mostrados:
- Inicialización (ESP32-C3, Bridge, v4.0-Stable)
- Modo AP con IP
- Estado WiFi (conectado/desconectado)
- IP STA cuando está conectado

### LED de Estado (GPIO 8)

El LED integrado en el ESP32-C3 SuperMini parpadea automáticamente en un **hilo RTOS independiente** (prioridad baja) para indicar el estado sin bloquear el programa principal.

#### Tabla de Patrones del LED

| Patrón Visual | Frecuencia | Significado | Estado del Sistema |
|---------------|-----------|-------------|-------------------|
| **Apagado** | Continuo | Firmware no iniciado o en reset | Boot / Reinicio por WDT |
| **Parpadeo lento** | ~1 Hz (1000 ms ON/OFF) | 🔹 Heartbeat / Idle | Firmware activo, sin tráfico UART ni TCP |
| **Parpadeo rápido** | ~12–25 Hz (40 ms ON/OFF) | 🔹 Actividad de Datos | Tráfico bidireccional activo (datos FPGA, respuesta cliente Telnet) |
| **Encendido fijo** | Continuo | 🔴 Error Crítico | Bloqueo de tareas, stack overflow (no debería ocurrir) |

#### Cómo Funciona

```cpp
// Tarea RTOS dedicada al LED (no bloquea loop principal)
void ledTask(void *pvParameters) {
  // Verifica si hay actividad en los últimos 200ms
  bool hasActivity = (now - lastActivityTime < 200);
  
  // Intervalo dinámico basado en actividad
  unsigned long interval = hasActivity ? 40 : 1000;
  
  // Alterna el LED según el intervalo
  digitalWrite(LED_PIN, state);
  vTaskDelay(10 / portTICK_PERIOD_MS);  // Cede CPU cada 10ms
}
```

**Ventajas**:
- ✅ No bloquea el loop principal
- ✅ Parpadeo suave y sin jitter
- ✅ Cambio automático según carga de datos
- ✅ CPU eficiente (solo 10ms de quantum)

#### Registrar Actividad

La actividad se registra automáticamente en:

```cpp
lastActivityTime = millis();  // Se actualiza cuando:
```

1. Cliente Telnet se conecta
2. Datos llegan de UART (Serial1) → se envían a Telnet
3. Datos llegan de Telnet → se envían a UART (Serial1)

El LED se apaga después de **200ms sin actividad** (heartbeat suave).

### Monitoreo en Vivo (LED + OLED)

Ejemplo de sesión típica:

1. **Al encender**: LED apagado, OLED muestra boot
2. **Setup listo**: LED comienza **parpadeo lento** (~1 Hz) → firmware activo en AP mode
3. **Conectar WiFi**: LED acelera a **parpadeo rápido**, OLED muestra "Conectando..."
4. **WiFi conectado**: LED vuelve a **parpadeo lento**, OLED muestra IP STA
5. **Cliente Telnet se conecta**: LED → **parpadeo rápido**
6. **Enviar datos** (FPGA ↔ Telnet): LED continúa **rápido** (activo 200ms)
7. **Sin datos 200ms**: LED → **parpadeo lento** (idle)

**En código**: Todas las operaciones de red/datos actualizan `lastActivityTime = millis()` que es monitoreada por `ledTask()`.

### Consola Serie

La salida por USB muestra eventos importantes:

```
UART FPGA iniciada
AP listo: 192.168.4.1
✅ WiFi Conectado: 192.168.1.100
Cliente Telnet conectado
⚠️ WiFi perdido. Cerrando Telnet...
🔄 Reconectando...
```

## 🛡️ Seguridad

> ⚠️ **Advertencia**: Este firmware está diseñado para entorno local de confianza (LAN). La red AP tiene credencial por defecto (`12345678`) y el puente Telnet no tiene autenticación.
>
> Para uso en producción o redes públicas:
> - Cambiar contraseña AP en código
> - Implementar autenticación en puente Telnet
> - Usar WiFi con WPA3 si es posible
> - Considerar VPN si se expone a Internet

## 📝 Pines de Configuración

```cpp
#define OLED_SDA 5
#define OLED_SCL 6
#define UART_RX_PIN 20
#define UART_TX_PIN 21
#define LED_PIN 8              // LED integrado
#define BAUD_RATE 115200
#define RAW_PORT 23            // Puerto Telnet
```

Edita `src/main.cpp` para cambiar valores según tu hardware.

## 🔄 Arquitectura RTOS

El firmware usa **FreeRTOS** (incluido en ESP-IDF) con tareas independientes:

### Tareas (Tasks)

| Tarea | Prioridad | Función | Comportamiento |
|-------|-----------|---------|----------------|
| **loop()** | 1 (baja) | Loop principal del sketch Arduino | No bloqueante, maneja WiFi y Telnet |
| **ledTask** | 1 (baja) | Control del LED de estado | Parpadea según actividad, no bloquea |
| **WiFi handler** | 2 (media) | Manejo interno de WiFi | Gestionado por Arduino/ESP-IDF |
| **Web server** | 2 (media) | Servidor HTTP | No bloqueante (async) |

### Sincronización

- **`lastActivityTime`**: Variable `volatile` compartida entre `loop()` y `ledTask`
- **`vTaskDelay()`**: Cede CPU al scheduler, permite multithreading real
- **No hay mutex**: Escritura de entero es atómica en ESP32 (32-bit)

### Ventajas

✅ **No bloqueante**: El LED parpadea sin afectar al servidor web o Telnet  
✅ **Responsive**: La entrada del usuario no espera a actualizaciones de OLED  
✅ **Eficiente**: CPU comparte tiempo entre tareas, idle durante esperas

### Configuración FreeRTOS

```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Crear tarea del LED
xTaskCreate(
  ledTask,              // Función de tarea
  "LED_Task",           // Nombre legible
  2048,                 // Stack size (bytes)
  NULL,                 // Parámetros
  1,                    // Prioridad (1=baja)
  NULL                  // Handle (opcional)
);

// Dentro de la tarea: ceder CPU
vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms
```

## 🔧 Troubleshooting

| Problema | Solución |
|----------|----------|
| No aparece en AP | Verificar poder USB, revisar LED en placa |
| OLED no se ve | Comprobar conexión I2C (pines 5, 6) |
| UART no comunica | Verificar velocidad (115200), pines RX/TX del dispositivo serial |
| Datos no pasan | Verificar que el dispositivo esté conectado correctamente al UART |
| Pérdida de WiFi | Aumentar `RECONNECT_MS` en main.cpp (actualmente 5000ms) |
| Latencia alta | Verificar TCP_NODELAY está activo, revisar RSSI WiFi |
| Búferes llenos | Aumentar tamaño de búfer en código (actualmente 128 bytes) |
| LED siempre apagado | Firmware no inició, comprobar consola serie, verificar voltaje |
| LED no parpadea con datos | Verificar conexión Telnet y tráfico UART |

## 🛠️ Personalización del LED

Para modificar el comportamiento del LED:

```cpp
// En main.cpp - función ledTask()

// Cambiar sensibilidad de detección de actividad:
unsigned long ACTIVITY_THRESHOLD = 200;  // Actualmente 200ms
bool hasActivity = (now - lastActivityTime < ACTIVITY_THRESHOLD);

// Cambiar velocidades de parpadeo:
unsigned long IDLE_INTERVAL = 1000;      // Heartbeat: 1000ms (1 Hz)
unsigned long ACTIVE_INTERVAL = 40;      // Actividad: 40ms (~25 Hz)
unsigned long interval = hasActivity ? ACTIVE_INTERVAL : IDLE_INTERVAL;

// Cambiar tiempo de respuesta del scheduler:
unsigned long SCHEDULER_QUANTUM = 10;    // Quantum: 10ms
vTaskDelay(SCHEDULER_QUANTUM / portTICK_PERIOD_MS);

// Cambiar prioridad de la tarea (1=baja, 3=alta):
xTaskCreate(ledTask, "LED_Task", 2048, NULL, 1, NULL);  // ← prioridad aquí
```

**Nota**: Aumentar prioridad del LED puede afectar responsividad de Telnet. Mantener en prioridad 1 (igual que loop).

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

## 🔄 Ciclo de Vida de Conexión y Ejecución

### Booteo (Setup)

```
┌─────────────────────────────────────────┐
│  Inicialización (setup())               │
├─────────────────────────────────────────┤
│ 1. Inicializar UART con dispositivo     │
│ 2. Inicializar pantalla OLED            │
│ 3. Cargar credenciales WiFi guardadas   │
│ 4. Activar modo AP + STA                │
│ 5. Iniciar servidor web + Telnet        │
│ 6. ✅ Crear tarea RTOS del LED          │
│ 7. Mostrar pantalla de boot             │
└────────────────┬──────────────────────┘
                 │
         ┌───────▼──────────┐
         │  Loop + LED en   │
         │  paralelo (RTOS) │
         └────────┬─────────┘
```

### Runtime (Loop Principal + Tareas RTOS)

```
                    ┌─────────────────────────────────┐
                    │  FreeRTOS Scheduler             │
                    │  (~10ms de quantum por tarea)   │
                    └──────────┬─────────────────────┘
                              │
            ┌─────────────────┼─────────────────┐
            │                 │                 │
      ┌─────▼────┐   ┌────────▼────────┐  ┌───▼──────┐
      │ loop()   │   │  ledTask()      │  │ WiFi/Web │
      │ (Puerto  │   │  (LED State)    │  │ (RTOS)   │
      │ Principal│   │                 │  │          │
      │ )        │   │ • Verifica      │  │ Handlers │
      ├──────────┤   │   actividad     │  ├──────────┤
      │• Telnet  │   │ • Parpadea      │  │ • DNS    │
      │• WiFi    │   │   dinámica      │  │ • HTTP   │
      │  status  │   │ • Cede CPU cada │  │ • Crypto │
      │• Sync    │   │   10ms          │  │ • Timers │
      │• OLED    │   │                 │  │          │
      │  update  │   └─────────────────┘  └──────────┘
      │          │
      └──────────┘
            ↓
      Actualizar
      lastActivityTime
      ↓
      ledTask() lee
      el valor
```

**Puntos clave**:
- `loop()` **nunca bloquea** (no hay `delay()` prolongados)
- `ledTask()` cede CPU con `vTaskDelay(10ms)` → no consume ciclos esperando
- Scheduler de FreeRTOS alterna entre tareas automáticamente
- `lastActivityTime` es la única variable compartida (atómica en ESP32)

## 💡 Casos de Uso

- **Comunicación remota**: Acceder a dispositivos seriales desde la red local
- **IoT**: Conectar sensores/actuadores seriales a WiFi sin hardware adicional
- **Debugging**: Inspeccionar tráfico serial desde cualquier cliente TCP
- **Prototipado**: Alternativa a adaptadores USB-TTL costosos
- **Monitoreo visual**: El LED indica estado del sistema en tiempo real (heartbeat, actividad de datos, errores)

## 📚 Referencias

- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [ESP32-C3 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [PlatformIO ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [Arduino WiFi API](https://www.arduino.cc/reference/en/libraries/wifi/)
- [FreeRTOS ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_idf.html) - Documentación de multithreading y tareas
- [TCP_NODELAY](https://linux.die.net/man/7/tcp) - Explicación del algoritmo de Nagle y su desactivación

## 📄 Licencia

Especifica según tu proyecto.

---

**Última actualización**: 29 de abril de 2026
