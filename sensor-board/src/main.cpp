#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <memory>
#include <cstring>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// Einfache Mess-ID zur Nachverfolgung
static unsigned long g_measureCounter = 0;
static unsigned long g_lastMeasureMs = 0;

// OLED (Heltec WiFi LoRa 32 V2)
static const int OLED_SDA = 4;
static const int OLED_SCL = 15;
static const uint8_t OLED_ADDR = 0x3C;
static const int OLED_RST = 16; // benötigt Reset-Toggle
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
static bool g_oledOk = false;
static bool g_otaActive = false;

// Vorwärtsdeklaration, damit in OTA-Init verwendbar
static void oledPrint(const String &l1, const String &l2);
// Vorwärtsdeklaration, da unten definiert aber oben verwendet
static void initOtaAp();

static String macSuffix()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[13];
  // use last 3 bytes
  snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  return String(buf);
}

static void stopOtaAp()
{
  if (!g_otaActive) return;
  WiFi.softAPdisconnect(true);
  g_otaActive = false;
  Serial.println("OTA AP gestoppt");
}

static void processCommandPayload(const String &cmd)
{
  if (cmd == "CMD:OTA_AP_ON") {
    if (!g_otaActive) initOtaAp();
  } else if (cmd == "CMD:OTA_AP_OFF") {
    stopOtaAp();
  } else {
    Serial.print("Unbekannter Befehl: "); Serial.println(cmd);
  }
}

static void handleDownlinkPacket()
{
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;
  // Erwartet verschlüsseltes Binärformat: [sid(1)][nonce(8)][ciphertext][mac(8)]
  std::unique_ptr<uint8_t[]> buf(new uint8_t[packetSize]);
  size_t read = LoRa.readBytes(buf.get(), packetSize);
  if (read != (size_t)packetSize) return;
  if (read < 1 + 8 + 8) return;
  uint8_t sid = buf[0];
  if (sid != SENSOR_ID) return; // nicht für diesen Sensor
  const uint8_t *nonce = buf.get() + 1;
  size_t ctLen = read - 1 - 8 - 8;
  const uint8_t *ct = buf.get() + 1 + 8;
  const uint8_t *mac = buf.get() + 1 + 8 + ctLen;

  // HMAC prüfen über (sid|nonce|ciphertext)
  auto hmac_trunc = [&](const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t msgLen, uint8_t *out, size_t outLen)->bool{
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;
    mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, md, 1) != 0) { mbedtls_md_free(&ctx); return false; }
    if (mbedtls_md_hmac_starts(&ctx, key, keyLen) != 0) { mbedtls_md_free(&ctx); return false; }
    if (mbedtls_md_hmac_update(&ctx, buf.get(), 1 + 8 + ctLen) != 0) { mbedtls_md_free(&ctx); return false; }
    uint8_t full[32];
    if (mbedtls_md_hmac_finish(&ctx, full) != 0) { mbedtls_md_free(&ctx); return false; }
    mbedtls_md_free(&ctx);
    memcpy(out, full, outLen);
    return true;
  };
  uint8_t calc[8];
  if (!hmac_trunc(HMAC_KEY, sizeof(HMAC_KEY), nullptr, 0, calc, 8)) return;
  if (memcmp(mac, calc, 8) != 0) { Serial.println("Downlink HMAC ungültig"); return; }

  // Entschlüsseln
  auto aes_ctr_crypt = [&](const uint8_t *key, const uint8_t *nonce8, uint8_t *data, size_t len)->bool{
    uint8_t iv[16] = {0}; memcpy(iv, nonce8, 8);
    size_t nc_off = 0; uint8_t stream_block[16] = {0};
    mbedtls_aes_context aes; mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) { mbedtls_aes_free(&aes); return false; }
    int rc = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, iv, stream_block, data, data);
    mbedtls_aes_free(&aes);
    return rc == 0;
  };
  std::unique_ptr<uint8_t[]> pt(new uint8_t[ctLen+1]);
  memcpy(pt.get(), ct, ctLen);
  if (!aes_ctr_crypt(AES_KEY, nonce, pt.get(), ctLen)) return;
  pt[ctLen] = 0;
  String cmd = String((const char*)pt.get());
  Serial.print("Downlink CMD: "); Serial.println(cmd);
  processCommandPayload(cmd);
}

static void initOtaAp()
{
  if (!OTA_AP_ENABLED) return;
  WiFi.mode(WIFI_AP);
  String ssid = String(OTA_AP_SSID_PREFIX) + macSuffix();
  bool apOk = WiFi.softAP(ssid.c_str(), OTA_AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();

  ArduinoOTA.setHostname(ssid.c_str());
  ArduinoOTA.setPassword(OTA_AP_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start (Sensor AP)");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Ende");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Fortschritt: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\nOTA Fehler[%u]\n", error);
  });

  ArduinoOTA.begin();
  g_otaActive = true;

  Serial.print("AP SSID: "); Serial.println(ssid);
  Serial.print("AP IP: "); Serial.println(ip);
  Serial.println("OTA bereit (Sensor)");
  oledPrint(String("AP: ") + ssid,
            String("IP: ") + ip.toString());
}

static uint32_t readMilliVoltsAveraged(int pin, int samples = 32)
{
  uint64_t acc = 0;
  for (int i = 0; i < samples; ++i)
  {
    acc += analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }
  return (uint32_t)(acc / samples);
}

static float mvToDepthCm(uint32_t mv)
{
  // Gain-Skalierung und Offset berücksichtigen
  float mvCal = (float)mv * SENSOR_MV_SCALE;
  int32_t mvAdj = (int32_t)mvCal - (int32_t)SENSOR_OFFSET_MV;
  if (mvAdj < 0) mvAdj = 0;
  // lineare Skalierung auf SENSOR_MAX_CM bei SENSOR_VREF_MV
  return ((float)mvAdj / (float)SENSOR_VREF_MV) * SENSOR_MAX_CM;
}

static void oledPrint(const String &l1, const String &l2)
{
  if (!OLED_ENABLED || !g_oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Sensor-Board");
  display.setCursor(0, 12);
  display.println(l1);
  display.setCursor(0, 24);
  display.println(l2);
  display.display();
}

static void sendLoraPacket(const String &payload)
{
  if (ENCRYPTION_ENABLED)
  {
    // Paket: [sensor_id(1)] [nonce(8)] [ciphertext(N)] [mac(8)]
    const size_t NONCE_LEN = 8; const size_t MAC_LEN = 8;
    // Nonce erzeugen
    uint8_t nonce[NONCE_LEN];
    for (int i = 0; i < (int)NONCE_LEN; ++i) nonce[i] = (uint8_t)(esp_random() & 0xFF);

    // Klartext vorbereiten
    size_t ptLen = payload.length();
    std::unique_ptr<uint8_t[]> ct(new uint8_t[ptLen]);
    memcpy(ct.get(), (const uint8_t*)payload.c_str(), ptLen);

    // AES-CTR verschlüsseln (in-place)
    auto aes_ctr_crypt = [&](const uint8_t *key, const uint8_t *nonce8, uint8_t *data, size_t len)->bool{
      uint8_t iv[16] = {0}; memcpy(iv, nonce8, NONCE_LEN);
      size_t nc_off = 0; uint8_t stream_block[16] = {0};
      mbedtls_aes_context aes; mbedtls_aes_init(&aes);
      if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) { mbedtls_aes_free(&aes); return false; }
      int rc = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, iv, stream_block, data, data);
      mbedtls_aes_free(&aes);
      return rc == 0;
    };
    if (!aes_ctr_crypt(AES_KEY, nonce, ct.get(), ptLen)) return;

    // MAC über (sid|nonce|ciphertext) berechnen (HMAC-SHA256 gekürzt)
    uint8_t mac[MAC_LEN];
    auto hmac_trunc = [&](const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t msgLen, uint8_t *out, size_t outLen)->bool{
      const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
      if (!md) return false;
      mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
      if (mbedtls_md_setup(&ctx, md, 1) != 0) { mbedtls_md_free(&ctx); return false; }
      if (mbedtls_md_hmac_starts(&ctx, key, keyLen) != 0) { mbedtls_md_free(&ctx); return false; }
      uint8_t sid = SENSOR_ID;
      mbedtls_md_hmac_update(&ctx, &sid, 1);
      mbedtls_md_hmac_update(&ctx, nonce, NONCE_LEN);
      mbedtls_md_hmac_update(&ctx, ct.get(), ptLen);
      uint8_t full[32];
      if (mbedtls_md_hmac_finish(&ctx, full) != 0) { mbedtls_md_free(&ctx); return false; }
      mbedtls_md_free(&ctx);
      memcpy(out, full, outLen);
      return true;
    };
    if (!hmac_trunc(HMAC_KEY, sizeof(HMAC_KEY), nullptr, 0, mac, MAC_LEN)) return;

    // Frame schreiben
    LoRa.beginPacket();
    LoRa.write(&SENSOR_ID, 1);
    LoRa.write(nonce, NONCE_LEN);
    LoRa.write(ct.get(), ptLen);
    LoRa.write(mac, MAC_LEN);
    LoRa.endPacket();
  }
  else
  {
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
  }
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(200);

  // OLED initialisieren (nur wenn aktiviert)
  if (OLED_ENABLED)
  {
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(400000);
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D))
    {
      g_oledOk = true;
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("HELTEC OLED OK");
      display.setCursor(0, 12);
      display.println("Init...");
      display.display();
      delay(300);
    }
  }

  // OTA-AP initialisieren (optional)
  if (OTA_AP_ENABLED)
  {
    initOtaAp();
  }

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

  // ADC vorbereiten
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_ADC_PIN, ADC_11db); // bis ~3.3V messbar
  oledPrint("Init...", "LoRa 868 MHz");
}

void loop()
{
  const unsigned long now = millis();
  // Immer Downlink prüfen
  handleDownlinkPacket();
  if (g_otaActive)
  {
    ArduinoOTA.handle();
  }
  if (now - g_lastMeasureMs < MEASURE_INTERVAL_MS)
  {
    delay(50);
    return;
  }
  g_lastMeasureMs = now;

  g_measureCounter++;

  // Messung durchführen (gemittelte Millivolt, bessere Kalibrierung auf ESP32 ADC1)
  uint32_t mv = readMilliVoltsAveraged(SENSOR_ADC_PIN, 32);
  float depthCm = mvToDepthCm(mv);

  // Plausibilität prüfen (nur für lokale Anzeige/Debug)
  bool ok = (depthCm >= DEPTH_MIN_CM) && (depthCm <= DEPTH_MAX_CM);
  String status = ok ? "OK" : "ERR";

  // Packet bauen: nur numerischer Wert in cm mit einer Nachkommastelle
  // Beispiel: "18.6"
  String payload = String(depthCm, 1);

  // Senden
  sendLoraPacket(payload);

  // Ausgabe seriell & OLED
  Serial.print("ADC: ");
  Serial.print("mv_raw=");
  Serial.print(mv);
  Serial.print(" mv_scaled=");
  Serial.print((float)mv * SENSOR_MV_SCALE, 1);
  Serial.print("  tiefe_cm=");
  Serial.print(depthCm, 1);
  Serial.print("  status=");
  Serial.println(status);
  Serial.print("Gesendet: "); Serial.println(payload);

  oledPrint(String("Tiefe: ") + String(depthCm, 1) + " cm",
            String("Status: ") + status);
}
