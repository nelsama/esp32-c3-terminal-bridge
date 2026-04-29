#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ==========================================
// ⚙️ CONFIGURACIÓN DE HARDWARE
// ==========================================
#define OLED_SDA 5
#define OLED_SCL 6
#define UART_RX_PIN 20
#define UART_TX_PIN 21
#define LED_PIN 8              // LED integrado ESP32-C3 SuperMini
#define BAUD_RATE   115200

// ==========================================
// OBJETOS GLOBALES
// ==========================================
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
WebServer webServer(80);
WiFiServer telnetServer(23);
WiFiClient telnetClient;
Preferences prefs;

String savedSSID = "";
String savedPass = "";
bool isConnected = false;
unsigned long lastScreenUpdate = 0;
unsigned long reconnectTimer = 0;
const unsigned long RECONNECT_MS = 5000;

// 🔑 Variables para LED (compartidas con hilo)
volatile unsigned long lastActivityTime = 0;

// ==========================================
// PROTOTIPOS
// ==========================================
void showBootScreen();
void updateOLED();
void setupWebRoutes();
String generateConfigPage();
void handleTelnetBridge();
void ledTask(void *pvParameters);  // Hilo del LED

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("UART FPGA iniciada");

  // Configurar LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.setContrast(255);
  showBootScreen();

  prefs.begin("wifi", false);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  // Modo AP+STA: El AP permanece activo para configuración
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-Bridge", "12345678");
  Serial.println("AP listo: 192.168.4.1");

  setupWebRoutes();
  webServer.begin();
  telnetServer.begin();

  // 🔑 Crear hilo dedicado para el LED (prioridad baja)
  xTaskCreate(ledTask, "LED_Task", 2048, NULL, 1, NULL);

  // Conexión automática al arranque si hay credenciales
  if (savedSSID.length() > 0) {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    Serial.print("Auto-conectando a: "); Serial.println(savedSSID);
  }
  updateOLED();
}

// ==========================================
// LOOP PRINCIPAL (Limpio y no bloqueante)
// ==========================================
void loop() {
  webServer.handleClient();  // Siempre activo

  // 1. 🔑 Sincronizar estado real del WiFi
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED && !isConnected) {
    isConnected = true;
    updateOLED();
    Serial.println("✅ WiFi Conectado: " + WiFi.localIP().toString());
  } else if (status != WL_CONNECTED && isConnected) {
    isConnected = false;
    updateOLED();
    Serial.println("⚠️ WiFi perdido. Cerrando Telnet...");
    if (telnetClient.connected()) telnetClient.stop();
  }

  // 2. Puente Telnet (solo si hay conexión STA)
  if (isConnected) {
    handleTelnetBridge();
  }

  // 3. 🔁 Auto-reconexión no bloqueante
  if (status != WL_CONNECTED && savedSSID.length() > 0 && millis() - reconnectTimer > RECONNECT_MS) {
    Serial.println("🔄 Reconectando...");
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    reconnectTimer = millis();
  }

  // 4. OLED (actualizar cada 1s)
  if (millis() - lastScreenUpdate > 1000) {
    updateOLED();
    lastScreenUpdate = millis();
  }
}

// ==========================================
// 💡 HILO DEL LED (Se ejecuta en paralelo - NO bloquea)
// ==========================================
void ledTask(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  unsigned long lastToggle = 0;
  bool state = false;
  
  while (1) {
    unsigned long now = millis();
    // ¿Actividad reciente? (últimos 200ms)
    bool hasActivity = (now - lastActivityTime < 200);
    
    // Intervalo dinámico: rápido si hay datos, lento si está idle
    unsigned long interval = hasActivity ? 40 : 1000;
    
    if (now - lastToggle >= interval) {
      state = !state;
      digitalWrite(LED_PIN, state);
      lastToggle = now;
    }
    
    // Cede CPU al scheduler cada 10ms (no bloquea, no gasta ciclos)
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// 🔌 TELNET BRIDGE (Buffers fijos + Registro de actividad)
// ==========================================
void handleTelnetBridge() {
  if (!telnetClient.connected()) {
    if (telnetServer.hasClient()) {
      if (telnetClient) telnetClient.stop();
      telnetClient = telnetServer.available();
      
      // 🔑 Desactiva Nagle para envío inmediato de teclas (juegos)
      telnetClient.setNoDelay(true);
      
      Serial.println("Cliente Telnet conectado");
      lastActivityTime = millis();  // 🔑 Parpadeo al conectar
      telnetClient.println("--- ESP32-C3 FPGA BRIDGE ---");
      telnetClient.println("Modo RAW. Teclas enviadas directamente.");
    }
    return;
  }

  // 📥 FPGA → Telnet (Buffer fijo, lectura no bloqueante)
  if (Serial1.available()) {
    uint8_t buf[128];  // ✅ Buffer fijo (evita stack overflow)
    int len = Serial1.read(buf, min(Serial1.available(), 128));
    if (len > 0) {
      telnetClient.write(buf, len);
      lastActivityTime = millis();  // 🔑 Registrar actividad
    }
  }

  // 📤 Telnet → FPGA (Buffer fijo, lectura no bloqueante)
  if (telnetClient.available()) {
    uint8_t buf[128];  // ✅ Buffer fijo
    int len = telnetClient.read(buf, min(telnetClient.available(), 128));
    if (len > 0) {
      Serial1.write(buf, len);
      lastActivityTime = millis();  // 🔑 Registrar actividad
    }
  }
}

// ==========================================
// 📡 RUTAS WEB (No bloqueantes)
// ==========================================
void setupWebRoutes() {
  // Ruta principal
  webServer.on("/", []() {
    if (isConnected) {
      String ip = WiFi.localIP().toString();
      String html = "<!DOCTYPE html><html><body style='text-align:center;margin-top:30px;font-family:sans-serif;'>";
      html += "<h1>ESP32 Bridge</h1>";
      html += "<p><b>Red:</b> " + WiFi.SSID() + "</p>";
      html += "<p><b>IP:</b> " + ip + "</p>";
      html += "<p><b>Telnet:</b> Conecta al puerto 23</p>";
      html += "<hr><p><a href='/reset' style='color:red'>🗑 Borrar WiFi</a></p>";
      html += "</body></html>";
      webServer.send(200, "text/html", html);
    } else {
      webServer.send(200, "text/html", generateConfigPage());
    }
  });

  // 🔑 Guardado NO BLOQUEANTE
  webServer.on("/save", []() {
    if (webServer.hasArg("ssid") && webServer.hasArg("pass")) {
      String newSSID = webServer.arg("ssid");
      String newPass = webServer.arg("pass");
      
      prefs.begin("wifi", false);
      prefs.putString("ssid", newSSID);
      prefs.putString("pass", newPass);
      prefs.end();
      
      // Iniciar conexión en background (sin bloquear)
      WiFi.begin(newSSID.c_str(), newPass.c_str());
      reconnectTimer = millis();
      
      String html = "<!DOCTYPE html><html><head>";
      html += "<meta http-equiv='refresh' content='5'>";
      html += "<script>setTimeout(()=>location.reload(), 4000)</script>";
      html += "<style>body{font-family:sans-serif;text-align:center;margin-top:50px}</style></head><body>";
      html += "<h2>🔄 Conectando a: <b>" + newSSID + "</b></h2>";
      html += "<p>Credenciales guardadas. Esperando enlace WiFi...</p>";
      html += "<p>La página se recargará automáticamente.</p>";
      html += "</body></html>";
      
      webServer.send(200, "text/html", html);
    } else {
      webServer.send(400, "text/plain", "Faltan parámetros");
    }
  });

  // Reset
  webServer.on("/reset", []() {
    prefs.begin("wifi", false); prefs.clear(); prefs.end();
    WiFi.disconnect(true);
    isConnected = false;
    reconnectTimer = millis();
    if (telnetClient.connected()) telnetClient.stop();
    webServer.send(200, "text/html", "<h1>✓ Reset</h1><p>Configuración borrada. Recarga la página.</p>");
  });
}

String generateConfigPage() {
  String html = R"rawliteral(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><title>WiFi Config</title><style>body{font-family:sans-serif;text-align:center;margin:20px}select,input,button{padding:10px;margin:5px;width:85%;border-radius:6px;border:1px solid #ccc;font-size:14px}button{background:#4CAF50;color:#fff;border:none;font-size:16px}small{display:block;margin-top:10px}</style></head><body><h2>📡 Elegir Red WiFi</h2><form action="/save" method="POST"><select name="ssid" required>)rawliteral";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<option value=''>No se encontraron redes (recarga)</option>";
  } else {
    for (int i = 0; i < n; ++i) {
      html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>";
    }
  }
  WiFi.scanDelete();
  html += R"rawliteral(</select><br><input type="password" name="pass" placeholder="Contraseña"><br><button>Conectar</button><br><small><a href="/">🔄 Actualizar lista</a></small></form></body></html>)rawliteral";
  return html;
}

// ==========================================
// 🖥️ OLED
// ==========================================
void showBootScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 10, "ESP32-C3");
  u8g2.drawStr(0, 20, "Bridge");
  u8g2.drawStr(0, 30, "v4.0-Stable");
  u8g2.sendBuffer();
  delay(1000);
}

void updateOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  if (isConnected) {
    u8g2.drawStr(0, 10, "WiFi:OK");
    String s = WiFi.SSID();
    if (s.length() > 12) s = s.substring(0, 12);
    u8g2.drawStr(0, 20, s.c_str());
    u8g2.drawStr(0, 30, WiFi.localIP().toString().c_str());
  } else {
    u8g2.drawStr(0, 10, "AP Mode");
    u8g2.drawStr(0, 20, "192.168.4.1");
    u8g2.drawStr(0, 30, "PW:12345678");
  }
  u8g2.sendBuffer();
}