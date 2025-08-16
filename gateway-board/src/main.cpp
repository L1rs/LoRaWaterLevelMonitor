#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <memory>
#include <cstring>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer web(80);

static unsigned long g_lastWifiAttempt = 0;
static unsigned long g_lastMqttAttempt = 0;
static String g_lastValue = "-";
static String g_lastStatus = "-";
static bool g_otaInitialized = false;

static void publishDiscovery();
static void processPayload(const String &payload);
// Vorwärtsdeklaration, da in buildStatusPage() verwendet
static String fmtAge(unsigned long sinceMs);
// Vorwärtsdeklaration für OLED-Hilfsfunktion
static void oledPrint(const String &line1, const String &line2);
static void initOta()
{
  if (!OTA_ENABLED || g_otaInitialized) return;
  // Hostname bilden: PREFIX + MAC
  String host = String(OTA_HOSTNAME_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
  ArduinoOTA.setHostname(host.c_str());
  if (OTA_PASSWORD && strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Ende");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int pct = (total ? (progress * 100 / total) : 0);
    Serial.printf("OTA Fortschritt: %u%%\n", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler[%u]\n", error);
  });

  ArduinoOTA.begin();
  g_otaInitialized = true;
  Serial.printf("OTA bereit als %s\n", host.c_str());
}

// OLED Display (Heltec WiFi LoRa 32 V2): I2C SDA=4, SCL=15, Adresse 0x3C
static const int OLED_SDA = 4;
static const int OLED_SCL = 15;
static const uint8_t OLED_ADDR = 0x3C;
static const int OLED_RST = 16; // Heltec OLED Reset-Pin
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Anzeige-/Messwertverwaltung
struct Measurement { String value; String status; unsigned long ts; };
static const int MAX_MEAS = 4;
static Measurement g_hist[MAX_MEAS];
static int g_histCount = 0;
static unsigned long g_lastLoRaMs = 0;
static int g_lastRssi = 0;
static float g_lastSnr = 0.0f;
static bool g_oledOk = false;
static bool g_oledEnabled = OLED_ENABLED; // zur Laufzeit schaltbar

// Heltec PRG/KEY Button (GPIO0, intern Pullup)
static const int BUTTON_PIN = 0;
static bool g_btnStable = true; // HIGH = nicht gedrückt
static bool g_btnPrevStable = true;
static unsigned long g_btnLastChangeMs = 0;
static unsigned long g_btnPressStartMs = 0;

// Crypto/Packet Parameter
static const size_t NONCE_LEN = 8;
static const size_t MAC_LEN = 8; // HMAC-SHA256 gekürzt

// Replay-Schutz: pro Sensor letzte 8 Nonces merken
struct ReplayMem { uint8_t sensorId; uint64_t nonces[8]; uint8_t count; };
static ReplayMem g_replay[4] = {0};

static bool isAllowedSensor(uint8_t sid)
{
  for (size_t i = 0; i < ALLOWED_SENSOR_IDS_COUNT; ++i)
    if (ALLOWED_SENSOR_IDS[i] == sid) return true;
  return false;
}

static bool replaySeen(uint8_t sid, uint64_t nonce)
{
  for (ReplayMem &m : g_replay)
  {
    if (m.sensorId == sid)
    {
      for (uint8_t i = 0; i < m.count; ++i)
        if (m.nonces[i] == nonce) return true;
      return false;
    }
  }
  return false;
}

static void rememberNonce(uint8_t sid, uint64_t nonce)
{
  for (ReplayMem &m : g_replay)
  {
    if (m.sensorId == sid || m.sensorId == 0)
    {
      if (m.sensorId == 0) { m.sensorId = sid; m.count = 0; }
      if (m.count < 8) { m.nonces[m.count++] = nonce; }
      else { for (int i = 7; i > 0; --i) m.nonces[i] = m.nonces[i-1]; m.nonces[0] = nonce; }
      return;
    }
  }
  // Fällt durch, wenn mehr als 4 Sensoren -> überschreiben Slot 0
  g_replay[0].sensorId = sid; g_replay[0].nonces[0] = nonce; g_replay[0].count = 1;
}

static bool hmac_trunc(const uint8_t *key, size_t keyLen,
                       const uint8_t *msg, size_t msgLen,
                       uint8_t *out, size_t outLen)
{
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md) return false;
  mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, md, 1) != 0) { mbedtls_md_free(&ctx); return false; }
  if (mbedtls_md_hmac_starts(&ctx, key, keyLen) != 0) { mbedtls_md_free(&ctx); return false; }
  if (mbedtls_md_hmac_update(&ctx, msg, msgLen) != 0) { mbedtls_md_free(&ctx); return false; }
  uint8_t full[32];
  if (mbedtls_md_hmac_finish(&ctx, full) != 0) { mbedtls_md_free(&ctx); return false; }
  mbedtls_md_free(&ctx);
  memcpy(out, full, outLen);
  return true;
}

static bool aes_ctr_crypt(const uint8_t *key, const uint8_t *nonce8,
                          uint8_t *data, size_t len)
{
  uint8_t iv[16] = {0}; memcpy(iv, nonce8, NONCE_LEN);
  size_t nc_off = 0; uint8_t stream_block[16] = {0};
  mbedtls_aes_context aes; mbedtls_aes_init(&aes);
  if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) { mbedtls_aes_free(&aes); return false; }
  int rc = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, iv, stream_block, data, data);
  mbedtls_aes_free(&aes);
  return rc == 0;
}

static void setOledPower(bool on)
{
  if (!g_oledOk) { g_oledEnabled = on; return; }
  if (on)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
    g_oledEnabled = true;
    oledPrint("OLED EIN", "");
  }
  else
  {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    g_oledEnabled = false;
  }
}

static void handleButton()
{
  int raw = digitalRead(BUTTON_PIN); // HIGH = nicht gedrückt, LOW = gedrückt
  bool cur = (raw == HIGH);
  unsigned long now = millis();
  if (cur != g_btnStable && (now - g_btnLastChangeMs) > 30)
  {
    g_btnStable = cur;
    g_btnLastChangeMs = now;
    if (g_btnStable == false) // Übergang HIGH->LOW = gedrückt
      g_btnPressStartMs = now;
    else // LOW->HIGH = losgelassen
    {
      unsigned long dur = now - g_btnPressStartMs;
      if (dur > 30 && dur < 800)
      {
        setOledPower(!g_oledEnabled);
      }
    }
  }
}

static void sendLoRaCommand(uint8_t targetSid, const String &cmd)
{
  // Paketformat wie Sensor-Uplink: [sid(1)][nonce(8)][ciphertext][mac(8)]
  const size_t NONCE = NONCE_LEN; const size_t MAC = MAC_LEN;
  uint8_t nonce[NONCE];
  for (size_t i = 0; i < NONCE; ++i) nonce[i] = (uint8_t)(esp_random() & 0xFF);
  const size_t ptLen = cmd.length();
  std::unique_ptr<uint8_t[]> buf(new uint8_t[1 + NONCE + ptLen + MAC]);
  buf[0] = targetSid;
  memcpy(buf.get() + 1, nonce, NONCE);
  // copy plaintext then encrypt in place after header
  std::unique_ptr<uint8_t[]> ct(new uint8_t[ptLen]);
  memcpy(ct.get(), (const uint8_t*)cmd.c_str(), ptLen);
  if (!aes_ctr_crypt(AES_KEY, nonce, ct.get(), ptLen)) return;
  memcpy(buf.get() + 1 + NONCE, ct.get(), ptLen);
  // MAC über (sid|nonce|ciphertext)
  uint8_t mac[MAC];
  if (!hmac_trunc(HMAC_KEY, sizeof(HMAC_KEY), buf.get(), 1 + NONCE + ptLen, mac, MAC)) return;
  memcpy(buf.get() + 1 + NONCE + ptLen, mac, MAC);
  // Senden
  LoRa.beginPacket();
  LoRa.write(buf.get(), 1 + NONCE + ptLen + MAC);
  LoRa.endPacket();
}

static String htmlEscape(const String &s)
{
  String o; o.reserve(s.length()+8);
  for (size_t i=0;i<s.length();++i){char c=s[i];
    if(c=='&') o+="&amp;"; else if(c=='<') o+="&lt;"; else if(c=='>') o+="&gt;"; else o+=c;}
  return o;
}

// OTA-AP Wunschzustand pro Sensor (unbestätigt)
struct OtaState { uint8_t sid; int8_t desired; unsigned long ts; };
static OtaState g_otaStates[4] = {{0, -1, 0},{0, -1, 0},{0, -1, 0},{0, -1, 0}};
static void setOtaDesired(uint8_t sid, bool on)
{
  for (auto &x : g_otaStates) {
    if (x.sid == sid || x.sid == 0) { if (x.sid == 0) x.sid = sid; x.desired = on?1:0; x.ts = millis(); return; }
  }
  g_otaStates[0] = {sid, (int8_t)(on?1:0), millis()};
}
static int8_t getOtaDesired(uint8_t sid)
{
  for (auto &x : g_otaStates) if (x.sid == sid) return x.desired;
  return -1;
}

static String buildStatusPage()
{
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("-");
  String wifi = (WiFi.status() == WL_CONNECTED) ? String("verbunden") : String("--");
  String mqtt = mqttClient.connected()? String("verbunden") : String("--");
  String loraAge = g_lastLoRaMs ? fmtAge(millis()-g_lastLoRaMs) : String("--");

  String html;
  html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Drainage Gateway</title><style>");
  html += F("body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,sans-serif;margin:0;background:#f6f7fb;color:#222}");
  html += F("header{display:flex;align-items:center;gap:12px;padding:12px 16px;background:#1f2937;color:#fff}");
  html += F("main{padding:16px;display:grid;grid-template-columns:1fr;gap:16px;max-width:900px;margin:0 auto}");
  html += F(".card{background:#fff;border:1px solid #e5e7eb;border-radius:10px;box-shadow:0 1px 2px rgba(0,0,0,.04)}");
  html += F(".card h2{margin:0;padding:12px 16px;border-bottom:1px solid #eee;font-size:16px}");
  html += F(".card .body{padding:12px 16px}");
  html += F("table{border-collapse:collapse;width:100%} td,th{border:1px solid #e5e7eb;padding:8px;text-align:left}");
  html += F(".row{display:flex;gap:16px;align-items:flex-start;flex-wrap:wrap}");
  html += F(".muted{color:#6b7280;font-size:12px}");
  html += F(".btn{display:inline-block;margin-right:8px;padding:8px 10px;border-radius:8px;border:1px solid #d1d5db;background:#f9fafb}");
  html += F(".btn:hover{background:#f3f4f6}");
  html += F(".badge{display:inline-block;padding:2px 8px;border-radius:999px;background:#eef2ff;color:#3730a3;border:1px solid #c7d2fe;font-size:12px}");
  html += F("img.pump{width:120px;height:auto;border:1px solid #e5e7eb;border-radius:8px;background:#fff}");
  html += F("</style></head><body>");
  html += F("<header style='padding:12px 16px;background:#1f2937;color:#fff;margin-bottom:16px;'>");
  html += F("<div style='font-size:18px;font-weight:600'>Drainage Gateway</div>");
  html += F("<div class='muted'>LoRa · WLAN · MQTT</div>");
  html += F("</header>");
  html += F("<main>");
  html += F("<section class='card'><h2>Status</h2><div class='body'>");
  html += F("<div class='row'>");
  html += F("<div style='flex:1;min-width:260px'><table>");
  html += F("<tr><th>WLAN</th><td>"); html += htmlEscape(wifi); html += F("</td></tr>");
  html += F("<tr><th>IP</th><td>"); html += htmlEscape(ip); html += F("</td></tr>");
  html += F("<tr><th>MQTT</th><td>"); html += htmlEscape(mqtt); html += F("</td></tr>");
  html += F("<tr><th>LoRa letzte RX</th><td>"); html += htmlEscape(loraAge); html += F("</td></tr>");
  html += F("<tr><th>RSSI</th><td>"); html += String(g_lastRssi); html += F("</td></tr>");
  // OTA-AP Status (gewünschter Zustand, unbestätigt)
  int8_t ota = getOtaDesired(1);
  html += F("<tr><th>Drainage WLAN-AP</th><td>");
  if (ota < 0) html += F("<span class='badge'>unbekannt</span>");
  else if (ota == 1) html += F("<span class='badge'>Eingeschaltet</span>");
  else html += F("<span class='badge'>Ausgeschaltet</span>");
  html += F("</td></tr>");
  html += F("</table></div>");
  html += F("</div></div></section>");

  html += F("<section class='card'><h2>Letzte Messwerte</h2><div class='body'><table><tr><th>#</th><th>Wert (cm)</th><th>Alter</th></tr>");
  for (int i=0;i<g_histCount;i++){
    unsigned long a = millis()-g_hist[i].ts;
    html += F("<tr><td>"); html += String(i+1);
    html += F("</td><td>"); html += htmlEscape(g_hist[i].value);
    html += F("</td><td>"); html += htmlEscape(fmtAge(a)); html += F("</td></tr>");
  }
  html += F("</table></div></section>");

  html += F("<section class='card'><h2>Sensor OTA-AP steuern</h2><div class='body'>");
  html += F("<form method='POST' action='/sensor/ota'>");
  html += F("Sensor-ID: <input type='number' name='sid' min='1' max='255' value='1'>\n");
  html += F("<button class='btn' name='enable' value='1' type='submit'>AP einschalten</button>\n");
  html += F("<button class='btn' name='enable' value='0' type='submit'>AP ausschalten</button>\n");
  html += F("</form>");
  html += F("<div class='muted'>Hinweis: Status basiert auf dem zuletzt angeforderten Zustand (ohne Bestätigung).</div>");
  html += F("</div></section>");

  // Schwellwerte-Beschreibung
  html += F("<section class='card'><h2>Wasserstand · Schwellwerte</h2><div class='body'>");
  html += F("<p>Die wichtigsten Schwellenwerte für den Wasserstand im Drainageschacht:</p>");
  html += F("<div style='margin: 16px 0;'><b>🟢 Normaler Bereich (bis 30 cm)</b><ul><li><b>Pumpe 1 \"Jung U5 KS\":</b> Hält den Wasserstand bei unter <b>19cm</b>.</li></ul></div>");
  html += F("<div style='margin: 16px 0;'><b>🟡 Erhöter Bereich (bis 65 cm)</b><ul><li><b>Pumpe 2 \"Makita PF1110\":</b> Springt an bei <b>56 cm</b> pumt ab auf <b>37cm</b>.</li></ul></div>");
  html += F("<div style='margin: 16px 0;'><b>🟠 Hoher Wasserstand (bis 80 cm)</b><ul><li><b>Drainage-Zulauf Bodenplatte:</b> Etwa auf Höhe <b>80-90cm</b>.</li></ul></div>");
  html += F("<div style='margin: 16px 0;'><b>🔴 Kritischer Bereich (ab 180 cm)</b><ul><li><b>Drainage-Zulauf Kellerfenster:</b> Etwa auf Höhe <b>180-190cm</b>.</li><li><b>Wassereintritt:</b> Bei einem Wasserstand von <b>±230cm</b> kommt es zu einem sicheren Wassereintritt im Keller.</li></ul></div>");
  html += F("</div></section>");

  html += F("</main></body></html>");
  return html;
}

static void handleRoot(){ String html = buildStatusPage(); web.send(200, "text/html; charset=utf-8", html); }
static void handleSensorOta()
{
  if (!web.hasArg("sid") || !web.hasArg("enable")) { web.send(400, "text/plain", "Bad Request"); return; }
  int sid = web.arg("sid").toInt();
  bool en = web.arg("enable")=="1";
  String cmd = en ? String("CMD:OTA_AP_ON") : String("CMD:OTA_AP_OFF");
  sendLoRaCommand((uint8_t)sid, cmd);
  setOtaDesired((uint8_t)sid, en);
  web.sendHeader("Location", "/"); web.send(303);
}

static bool processEncryptedPacketBytes(const uint8_t *buf, size_t len)
{
  if (len < 1 + NONCE_LEN + MAC_LEN) return false;
  uint8_t sid = buf[0];
  if (!isAllowedSensor(sid)) return false;
  const uint8_t *nonce = buf + 1;
  size_t ctLen = len - 1 - NONCE_LEN - MAC_LEN;
  if (ctLen == 0) return false;
  const uint8_t *ct = buf + 1 + NONCE_LEN;
  const uint8_t *mac = buf + 1 + NONCE_LEN + ctLen;

  // MAC prüfen über (sid | nonce | ciphertext)
  uint8_t calc[MAC_LEN];
  if (!hmac_trunc(HMAC_KEY, sizeof(HMAC_KEY), buf, 1 + NONCE_LEN + ctLen, calc, MAC_LEN)) return false;
  if (memcmp(mac, calc, MAC_LEN) != 0) return false;

  // Replay prüfen
  uint64_t nonce64 = 0; memcpy(&nonce64, nonce, NONCE_LEN);
  if (replaySeen(sid, nonce64)) return false;

  // Entschlüsseln in-place
  std::unique_ptr<uint8_t[]> pt(new uint8_t[ctLen+1]);
  memcpy(pt.get(), ct, ctLen);
  if (!aes_ctr_crypt(AES_KEY, nonce, pt.get(), ctLen)) return false;
  pt[ctLen] = 0;

  // Gültig -> merken und verarbeiten
  rememberNonce(sid, nonce64);
  String plain = String((const char*)pt.get());
  processPayload(plain);
  return true;
}

static String fmtAge(unsigned long sinceMs)
{
  unsigned long s = sinceMs / 1000UL;
  if (s < 60) return String(s) + "s";
  unsigned long m = s / 60UL; s %= 60UL;
  if (m < 60) return String(m) + "m" + (s?String(" ")+String(s)+"s":"");
  unsigned long h = m / 60UL; m %= 60UL;
  return String(h) + "h" + (m?String(" ")+String(m)+"m":"");
}

static void addMeasurement(const String &val, const String &stat)
{
  for (int i = (g_histCount < (MAX_MEAS-1) ? g_histCount : (MAX_MEAS-1)); i > 0; --i)
    g_hist[i] = g_hist[i-1];
  g_hist[0] = { val, stat, millis() };
  if (g_histCount < MAX_MEAS) g_histCount++;
}

static void drawStatus()
{
  if (!g_oledEnabled || !g_oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Kopfzeile
  display.setCursor(0, 0);
  display.println("LoRa Drainage GW");

  // Zeile 1: WLAN + IP
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("-");
  display.setCursor(0, 10);
  display.print("MQTT:");
  display.print(mqttClient.connected()?"OK ":"-- ");
  display.print(ip);

  // Zeile 2: LoRa letzte RX (direkt unter MQTT-Zeile)
  display.setCursor(0, 20);
  if (g_lastLoRaMs)
  {
    unsigned long age = millis() - g_lastLoRaMs;
    display.print("LoRa:");
    display.print(fmtAge(age));
    display.print(" RSSI:");
    display.println(g_lastRssi);
  }
  else
  {
    display.println("LoRa: --");
  }

  // Zeilen 3-5: letzte Messwerte
  int lineY = 30;
  for (int i = 0; i < g_histCount && i < 3; ++i)
  {
    unsigned long a = millis() - g_hist[i].ts;
    display.setCursor(0, lineY);
    display.print(i+1);
    display.print(") ");
    display.print(g_hist[i].value);
    display.print("cm ");
    display.println(fmtAge(a));
    lineY += 10;
  }

  display.display();
}

static void oledPrint(const String &line1, const String &line2)
{
  if (!g_oledEnabled) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Gateway-Board");
  display.setCursor(0, 12);
  display.println(line1);
  display.setCursor(0, 24);
  display.println(line2);
  display.display();
}

static void ensureWifi()
{
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - g_lastWifiAttempt < WIFI_RECONNECT_INTERVAL_MS) return;
  g_lastWifiAttempt = now;

  Serial.printf("Verbinde mit WLAN '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void ensureMqtt()
{
  if (mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - g_lastMqttAttempt < MQTT_RECONNECT_INTERVAL_MS) return;
  g_lastMqttAttempt = now;

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // Client-ID bilden
  String clientId = MQTT_CLIENT_ID_PREFIX;
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.printf("Verbinde mit MQTT %s:%u als %s...\n", MQTT_HOST, MQTT_PORT, clientId.c_str());
  // Verbindung ohne Availability/LWT-Topic herstellen (Status-Topic entfällt)
  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
  {
    Serial.println("MQTT verbunden.");
    publishDiscovery();
    oledPrint("MQTT verbunden", String(MQTT_HOST));
  }
  else
  {
    Serial.printf("MQTT-Verbindung fehlgeschlagen, rc=%d\n", mqttClient.state());
  }
}

static void publishReadings()
{
  if (!mqttClient.connected()) return;
  if (g_lastValue.length())
  {
    mqttClient.publish(TOPIC_WATERLEVEL, g_lastValue.c_str(), true);
  }
  // Status-Topic entfällt
  // RSSI des letzten LoRa-Pakets zusätzlich veröffentlichen (retained)
  if (g_lastLoRaMs)
  {
    String rssiStr = String(g_lastRssi);
    mqttClient.publish(TOPIC_RSSI, rssiStr.c_str(), true);
  }
}

static void processPayload(const String &payload)
{
  Serial.print("LoRa empfangen: ");
  Serial.println(payload);

  // Erwartetes Format (alt): WATER_CM:<wert>;STATUS:<OK|ERR>;MID:<id>
  // Neu: reine Zahl als Payload, z.B. "18.6"
  String waterStr = "";
  String statusStr = "";

  String s = payload; s.trim();
  int wIdx = s.indexOf("WATER_CM:");
  if (wIdx >= 0)
  {
    int wEnd = s.indexOf(';', wIdx);
    if (wEnd < 0) wEnd = s.length();
    waterStr = s.substring(wIdx + 9, wEnd);
    waterStr.trim();

    int sIdx = s.indexOf("STATUS:");
    if (sIdx >= 0)
    {
      int sEnd = s.indexOf(';', sIdx);
      if (sEnd < 0) sEnd = s.length();
      statusStr = s.substring(sIdx + 7, sEnd);
      statusStr.trim();
    }
  }
  else
  {
    // numeric-only Pfad: evtl. "cm" Suffix entfernen
    if (s.endsWith("cm")) s = s.substring(0, s.length()-2);
    s.trim();
    // einfache Numerik-Validierung: optionales +/- am Anfang, Ziffern, ein '.' erlaubt
    bool numeric = true; bool dotSeen = false; int start = 0;
    if (s.length() == 0) numeric = false;
    if (numeric && (s[0] == '+' || s[0] == '-')) start = 1;
    for (int i = start; numeric && i < s.length(); ++i)
    {
      char c = s[i];
      if (c == '.') { if (dotSeen) numeric = false; else dotSeen = true; continue; }
      if (c < '0' || c > '9') numeric = false;
    }
    if (numeric) waterStr = s;
  }

  if (waterStr.length() == 0)
  {
    statusStr = "parse_error";
  }

  g_lastValue = waterStr.length() ? waterStr : "-";
  g_lastStatus = statusStr.length() ? statusStr : "-";
  g_lastLoRaMs = millis();
  g_lastRssi = LoRa.packetRssi();
  g_lastSnr = LoRa.packetSnr();
  addMeasurement(g_lastValue, g_lastStatus);

  publishReadings();
  oledPrint(String("Wasser: ") + g_lastValue + " cm",
            String("Status: ") + g_lastStatus);
  drawStatus();
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(200);

  // OLED initialisieren (Heltec: Reset über GPIO16 notwendig), immer initialisieren
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000); // 400kHz I2C
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(50);
  bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledOk) {
    // Fallback auf alternative Adresse 0x3D
    oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  }
  if (!oledOk) {
    Serial.println("SSD1306 Init fehlgeschlagen! Adresse 0x3C/0x3D nicht erreichbar.");
  } else {
    g_oledOk = true;
    // Setze Power-Zustand laut g_oledEnabled
    if (g_oledEnabled) {
      display.ssd1306_command(SSD1306_DISPLAYON);
      oledPrint("HELTEC OLED OK", "Starte...");
      delay(300);
    } else {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
    }
  }

  // PRG/KEY Button aktivieren
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // LoRa Pins für Heltec WiFi LoRa 32 (V2)
  const int LORA_SCK = 5;
  const int LORA_MISO = 19;
  const int LORA_MOSI = 27;
  const int LORA_SS = 18;
  const int LORA_RST = 14;
  const int LORA_DIO0 = 26;

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    Serial.println("LoRa Init fehlgeschlagen!");
    while (true) { delay(1000); }
  }

  // Kein Interrupt-Callback verwenden; stattdessen Polling im loop()

  // WLAN/MQTT init
  WiFi.mode(WIFI_STA);
  ensureWifi();
  mqttClient.setKeepAlive(30);
  mqttClient.setBufferSize(256);
  ensureMqtt();

  // Webserver Routen registrieren und starten
  web.on("/", handleRoot);
  web.on("/sensor/ota", HTTP_POST, handleSensorOta);
  web.begin();
  Serial.println("Webserver gestartet auf Port 80");

  oledPrint("Init...", "WLAN/MQTT verbinden");
}

void loop()
{
  ensureWifi();
  if (WiFi.status() == WL_CONNECTED)
  {
    ensureMqtt();
    initOta();
  }

  if (mqttClient.connected()) { mqttClient.loop(); }
  if (OTA_ENABLED && g_otaInitialized) { ArduinoOTA.handle(); }
  web.handleClient();

  // Button abfragen (kurzer Druck toggelt OLED)
  handleButton();

  // LoRa-Pakete im Polling-Modus verarbeiten
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    if (ENCRYPTION_ENABLED)
    {
      // Binärpaket lesen
      std::unique_ptr<uint8_t[]> buf(new uint8_t[packetSize]);
      size_t read = LoRa.readBytes(buf.get(), packetSize);
      if (read == (size_t)packetSize)
      {
        if (!processEncryptedPacketBytes(buf.get(), read))
        {
          Serial.println("Verschl. Paket ungültig/verworfen");
        }
      }
    }
    else
    {
      String payload;
      while (LoRa.available()) payload += (char)LoRa.read();
      if (payload.length()) processPayload(payload);
    }
  }

  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 1000)
  {
    lastDraw = millis();
    drawStatus();
  }

  delay(10);
}

static void publishDiscovery()
{
  if (!ENABLE_HA_DISCOVERY || !mqttClient.connected()) return;

  // Home Assistant Discovery für den Wasserstand-Sensor
  // Topic: <prefix>/sensor/<node_id>/waterlevel/config
  String topic = String(HA_DISCOVERY_PREFIX) + "/sensor/" + HA_NODE_ID + "/waterlevel/config";

  // JSON manuell erstellen (klein halten, ohne ArduinoJson)
  // Enthält Verfügbarkeit, Gerätedaten und Zustandsthema
  String payload = "{";
  payload += "\"name\":\"Drainage Wasserstand\",";
  payload += "\"state_topic\":\"" + String(TOPIC_WATERLEVEL) + "\",";
  payload += "\"unit_of_measurement\":\"cm\",";
  payload += "\"device_class\":\"distance\",";
  payload += "\"state_class\":\"measurement\",";
  // Availability entfällt (kein Status-Topic mehr)
  payload += "\"unique_id\":\"" + String(HA_NODE_ID) + "_waterlevel\",";
  payload += "\"device\":{\"name\":\"" + String(HA_DEVICE_NAME) + "\",\"identifiers\":[\"" + String(HA_NODE_ID) + "\"]}";
  payload += "}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}
