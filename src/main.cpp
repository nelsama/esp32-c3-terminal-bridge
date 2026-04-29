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
#define TCP_PORT    3000  // 🔑 Puerto RAW (sin Telnet)

// ==========================================
// OBJETOS GLOBALES
// ==========================================
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
WebServer webServer(80);
WiFiServer tcpServer(TCP_PORT);  // 🔑 Servidor TCP puro
WiFiClient tcpClient;
Preferences prefs;

String savedSSID = "";
String savedPass = "";
bool wifiConnected = false;
unsigned long reconnectTimer = 0;
const unsigned long RECONNECT_INTERVAL = 8000;
unsigned long lastScreenUpdate = 0;

// ==========================================
// PROTOTIPOS
// ==========================================
void showBootScreen();
void updateOLED();
void setupWebRoutes();
String generateConfigPage();
void handleTcpBridge();

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
  
  showBootScreen();

  prefs.begin("wifi", false);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-Bridge", "12345678");
  Serial.println("AP listo: 192.168.4.1");

  setupWebRoutes();
  webServer.begin();
  tcpServer.begin();  // 🔑 Inicia servidor TCP en puerto 3000

  if (savedSSID != "") {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    Serial.print("Conectando a: "); 
    Serial.println(savedSSID);
    reconnectTimer = millis();
  }

  updateOLED();
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================
void loop() {
  webServer.handleClient();

  wl_status_t status = WiFi.status();
  bool justConnected = (status == WL_CONNECTED && !wifiConnected);
  bool justDisconnected = (status != WL_CONNECTED && wifiConnected);

  if (justConnected) {
    wifiConnected = true;
    updateOLED();
    Serial.println("✅ WiFi Conectado: " + WiFi.localIP().toString());
  } 
  else if (justDisconnected) {
    wifiConnected = false;
    updateOLED();
    Serial.println("⚠️ WiFi perdido. Cerrando sesión TCP...");
    if (tcpClient.connected()) tcpClient.stop();
  }

  if (wifiConnected) {
    handleTcpBridge();
  }

  if (status != WL_CONNECTED && savedSSID != "" && millis() - reconnectTimer > RECONNECT_INTERVAL) {
    Serial.println("🔄 Intentando reconexión...");
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    reconnectTimer = millis();
  }

  unsigned long interval = wifiConnected ? 5000 : 1000;
  if (millis() - lastScreenUpdate > interval) {
    updateOLED();
    lastScreenUpdate = millis();
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
      html += "<h1>ESP32 Bridge</h1>";
      html += "<p><b>Red:</b> " + WiFi.SSID() + "</p>";
      html += "<p><b>IP:</b> " + ip + "</p>";
      html += "<p><b>Conexión:</b> Usa <code>nc " + ip + " 3000</code> o PuTTY (Raw, puerto 3000)</p>";
      html += "<hr><p><a href='/reset' style='color:red'>🗑 Borrar WiFi</a></p>";
      html += "</body></html>";
      webServer.send(200, "text/html", html);
    } else {
      webServer.send(200, "text/html", generateConfigPage());
    }
  });

  webServer.on("/save", []() {
    if (webServer.hasArg("ssid") && webServer.hasArg("pass")) {
      String newSSID = webServer.arg("ssid");
      String newPass = webServer.arg("pass");
      
      prefs.begin("wifi", false);
      prefs.putString("ssid", newSSID);
      prefs.putString("pass", newPass);
      prefs.end();
      
      savedSSID = newSSID;
      savedPass = newPass;
      
      WiFi.begin(newSSID.c_str(), newPass.c_str());
      reconnectTimer = millis();
      
      String html = "<!DOCTYPE html><html><head>";
      html += "<meta http-equiv='refresh' content='5'>";
      html += "<script>setTimeout(()=>location.reload(), 4000)</script>";
      html += "<style>body{font-family:sans-serif;text-align:center;margin-top:50px}</style></head><body>";
      html += "<h2>🔄 Conectando a: <b>" + newSSID + "</b></h2>";
      html += "<p>Credenciales guardadas. Esperando enlace WiFi...</p>";
      html += "</body></html>";
      
      webServer.send(200, "text/html", html);
    } else {
      webServer.send(400, "text/plain", "Faltan parámetros");
    }
  });

  webServer.on("/reset", []() {
    prefs.begin("wifi", false); prefs.clear(); prefs.end();
    WiFi.disconnect(true);
    wifiConnected = false;
    reconnectTimer = millis();
    if (tcpClient.connected()) tcpClient.stop();
    
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
// 🔌 PUENTE TCP RAW (OPTIMIZADO PARA JUEGOS)
// ==========================================
void handleTcpBridge() {
  if (!tcpClient.connected()) {
    if (tcpServer.hasClient()) {
      if (tcpClient) tcpClient.stop();
      tcpClient = tcpServer.available();
      
      // 🔑 CRÍTICO: Desactiva Nagle para envío inmediato de teclas
      tcpClient.setNoDelay(true);
      
      Serial.println("Cliente TCP conectado (Modo RAW - Sin Telnet)");
      // No enviamos mensajes de bienvenida que puedan interferir con el juego
    }
    return;
  }

  // 📥 FPGA → Cliente (buffer fijo, no bloqueante)
  if (Serial1.available()) {
    uint8_t buf[128];
    size_t len = Serial1.read(buf, sizeof(buf));
    if (len > 0) tcpClient.write(buf, len);
  }

  // 📤 Cliente → FPGA (buffer fijo, no bloqueante, SIN FILTRO)
  // Al ser TCP puro, no hay negociación Telnet que filtrar
  if (tcpClient.available()) {
    uint8_t buf[128];
    size_t len = tcpClient.read(buf, sizeof(buf));
    if (len > 0) Serial1.write(buf, len);
  }
}

// ==========================================
// 🖥️ OLED
// ==========================================
void showBootScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 10, "ESP32-C3");
  u8g2.drawStr(0, 20, "Bridge");
  u8g2.drawStr(0, 30, "v3.0-RawTCP");
  u8g2.sendBuffer();
  delay(1000);
}

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
    u8g2.drawStr(0, 20, "IP:192.168.4.1");
    u8g2.drawStr(0, 30, "PW:12345678");
  }
  u8g2.sendBuffer();
}