#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>

// ==========================================
// ⚙️ CONFIGURACIÓN DE HARDWARE
// ==========================================
#define OLED_SDA 5
#define OLED_SCL 6
#define UART_RX_PIN 20
#define UART_TX_PIN 21
#define BAUD_RATE   115200
#define RAW_PORT    23  // Puerto TCP puro (sin protocolo Telnet)

// ==========================================
// OBJETOS GLOBALES
// ==========================================
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
WebServer webServer(80);
WiFiServer rawServer(RAW_PORT);
WiFiClient rawClient;
Preferences prefs;

String savedSSID = "";
String savedPass = "";
bool wifiConnected = false;
unsigned long reconnectTimer = 0;
const unsigned long RECONNECT_MS = 5000;
unsigned long lastOledUpdate = 0;

// ==========================================
// PROTOTIPOS
// ==========================================
void updateOLED();
void setupWebRoutes();
String generateConfigPage();
void handleRawBridge();

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("UART FPGA iniciada");

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.setContrast(255);
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 10, "ESP32-C3");
  u8g2.drawStr(0, 20, "Bridge RAW");
  u8g2.drawStr(0, 30, "v1.0");
  u8g2.sendBuffer();
  delay(1000);

  prefs.begin("wifi", false);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-RAW", "12345678");
  Serial.println("AP listo: 192.168.4.1");

  setupWebRoutes();
  webServer.begin();
  rawServer.begin();

  if (savedSSID.length() > 0) {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    Serial.print("Auto-conectando a: "); Serial.println(savedSSID);
  }
  updateOLED();
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================
void loop() {
  webServer.handleClient(); // No bloqueante

  // 1. Monitoreo de estado WiFi
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED && !wifiConnected) {
    wifiConnected = true;
    updateOLED();
    Serial.println("✅ WiFi Conectado: " + WiFi.localIP().toString());
  } else if (status != WL_CONNECTED && wifiConnected) {
    wifiConnected = false;
    updateOLED();
    Serial.println("️ WiFi perdido. Cerrando sesión RAW...");
    if (rawClient.connected()) rawClient.stop();
  }

  // 2. Puente RAW (solo si hay conexión STA)
  if (wifiConnected) handleRawBridge();

  // 3. Auto-reconexión no bloqueante
  if (status != WL_CONNECTED && savedSSID.length() > 0 && millis() - reconnectTimer > RECONNECT_MS) {
    Serial.println("🔄 Reconectando...");
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    reconnectTimer = millis();
  }

  // 4. OLED (5s conectado, 1s en AP)
  unsigned long interval = wifiConnected ? 5000 : 1000;
  if (millis() - lastOledUpdate > interval) {
    updateOLED();
    lastOledUpdate = millis();
  }
}

// ==========================================
// 🔌 PUENTE TCP RAW (OPTIMIZADO PARA JUEGOS)
// ==========================================
void handleRawBridge() {
  if (!rawClient.connected()) {
    if (rawServer.hasClient()) {
      if (rawClient) rawClient.stop();
      rawClient = rawServer.available();
      
      // 🔑 CRÍTICO: Desactiva algoritmo de Nagle. Cada tecla viaja en <1ms
      rawClient.setNoDelay(true);
      Serial.println("Cliente RAW conectado");
    }
    return;
  }

  // 📥 FPGA → Cliente (buffer fijo, lectura no bloqueante)
  if (Serial1.available()) {
    uint8_t buf[64];
    int avail = Serial1.available();
    int len = Serial1.read(buf, min(avail, 64));
    if (len > 0) rawClient.write(buf, len);
  }

  // 📤 Cliente → FPGA (buffer fijo, lectura no bloqueante)
  if (rawClient.available()) {
    uint8_t buf[64];
    int avail = rawClient.available();
    int len = rawClient.read(buf, min(avail, 64));
    if (len > 0) Serial1.write(buf, len);
  }
}

// ==========================================
// 📡 RUTAS WEB
// ==========================================
void setupWebRoutes() {
  webServer.on("/", []() {
    if (wifiConnected) {
      String ip = WiFi.localIP().toString();
      String html = "<!DOCTYPE html><html><body style='text-align:center;margin-top:30px;font-family:sans-serif;'>";
      html += "<h1>ESP32 RAW Bridge</h1>";
      html += "<p><b>Red:</b> " + WiFi.SSID() + "</p>";
      html += "<p><b>IP:</b> " + ip + "</p>";
      html += "<p><b>Conexión:</b> <code>nc " + ip + " " + String(RAW_PORT) + "</code> o PuTTY (Raw)</p>";
      html += "<hr><p><a href='/reset' style='color:red'>🗑 Borrar WiFi</a></p>";
      html += "</body></html>";
      webServer.send(200, "text/html", html);
    } else {
      webServer.send(200, "text/html", generateConfigPage());
    }
  });

  webServer.on("/save", HTTP_POST, []() {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    
    savedSSID = ssid;
    savedPass = pass;
    WiFi.begin(ssid.c_str(), pass.c_str());
    reconnectTimer = millis();
    
    webServer.send(200, "text/html", "<html><body style='text-align:center;font-family:sans-serif;'><h2>Guardando...</h2><p>Conectando a " + ssid + "</p><meta http-equiv='refresh' content='3;url=/'></body></html>");
  });

  webServer.on("/reset", []() {
    prefs.begin("wifi", false); prefs.clear(); prefs.end();
    WiFi.disconnect(true);
    wifiConnected = false;
    reconnectTimer = millis();
    if (rawClient.connected()) rawClient.stop();
    webServer.send(200, "text/html", "<html><body style='text-align:center;font-family:sans-serif;'><h2>✓ Reset</h2><p>Recarga la página.</p></body></html>");
  });
}

String generateConfigPage() {
  String html = R"rawliteral(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><title>WiFi Config</title><style>body{font-family:sans-serif;text-align:center;margin:20px}select,input,button{padding:10px;margin:5px;width:85%;border-radius:6px;border:1px solid #ccc;font-size:14px}button{background:#4CAF50;color:#fff;border:none;font-size:16px}small{display:block;margin-top:10px}</style></head><body><h2>📡 Elegir Red WiFi</h2><form action="/save" method="POST"><select name="ssid" required>)rawliteral";
  
  int n = WiFi.scanNetworks();
  if (n == 0) html += "<option value=''>No se encontraron redes</option>";
  else {
    for (int i = 0; i < n; ++i)
      html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>";
  }
  WiFi.scanDelete();
  
  html += R"rawliteral(</select><br><input type="password" name="pass" placeholder="Contraseña"><br><button>Conectar</button><br><small><a href="/">🔄 Actualizar</a></small></form></body></html>)rawliteral";
  return html;
}

// ==========================================
// 🖥️ OLED
// ==========================================
void updateOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  if (wifiConnected) {
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