#pragma once
// Deutsche Dokumentation
// LoRa-Frame-Build für Sensor-Uplink (verschlüsselt/optional unverschlüsselt)
#include <Arduino.h>

// Sendet einen Messwert-Payload (ASCII) als verschlüsseltes Paket.
// Nutzt AES_KEY/HMAC_KEY aus config.h und LORA_FREQUENCY_HZ (bereits initialisiert in setup).
bool loraSendEncrypted(uint8_t sensorId, const String& payload);
