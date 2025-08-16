#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include "measurement.h"
#include "lora_frames.h"
#include "oled.h"
#include "ota_ap.h"

// Zeitsteuerung Messung
static unsigned long g_lastMeasureMs = 0;

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(200);

  // OLED initialisieren (wenn aktiviert)
  oledInitSensor();

  // OTA-AP initialisieren (optional)
  if (OTA_AP_ENABLED) {
    otaApInit();
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
  oledPrint2Sensor("Init...", "LoRa 868 MHz");
}

void loop()
{
  // OTA-AP bedienen (falls aktiv)
  otaApLoop();

  // Downlink-Befehle könnten hier optional verarbeitet werden (separates Modul empfohlen)
  // ...

  const unsigned long now = millis();
  if (now - g_lastMeasureMs < MEASURE_INTERVAL_MS) {
    delay(50);
    return;
  }
  g_lastMeasureMs = now;

  // Messung durchführen
  uint32_t mv = readMilliVoltsAveraged(SENSOR_ADC_PIN, 32);
  float depthCm = mvToDepthCm(mv);

  // Plausibilität (lokal)
  bool ok = (depthCm >= DEPTH_MIN_CM) && (depthCm <= DEPTH_MAX_CM);
  String status = ok ? "OK" : "ERR";

  // Payload nur als Zahl mit einer Nachkommastelle
  String payload = String(depthCm, 1);

  // Senden (verschlüsselt, wenn aktiviert)
  loraSendEncrypted(SENSOR_ID, payload);

  // Debug & Anzeige
  Serial.print("mv_raw="); Serial.print(mv);
  Serial.print(" mv_scaled="); Serial.print((float)mv * SENSOR_MV_SCALE, 1);
  Serial.print("  tiefe_cm="); Serial.print(depthCm, 1);
  Serial.print("  status="); Serial.println(status);
  oledPrint2Sensor(String("Tiefe: ") + String(depthCm, 1) + " cm", String("Status: ") + status);
}
