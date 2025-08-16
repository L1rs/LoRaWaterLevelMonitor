// Deutsche Dokumentation
// LoRa-Frames (Sensor): Aufbau und Versand
#include "lora_frames.h"
#include <LoRa.h>
#include <memory>
#include <cstring>
#include "config.h"
// Gemeinsame Krypto-Funktionen
#include "crypto.h"

static const size_t NONCE_LEN = 8;
static const size_t MAC_LEN   = 8; // Hinweis: Für mehr Sicherheit ggf. 16 nutzen

bool loraSendEncrypted(uint8_t sensorId, const String& payload)
{
    if (!ENCRYPTION_ENABLED)
    {
        LoRa.beginPacket();
        LoRa.print(payload);
        LoRa.endPacket();
        return true;
    }

    const size_t ptLen = payload.length();
    std::unique_ptr<uint8_t[]> ct(new uint8_t[ptLen]);
    memcpy(ct.get(), (const uint8_t*)payload.c_str(), ptLen);

    uint8_t nonce[NONCE_LEN];
    for (size_t i = 0; i < NONCE_LEN; ++i) nonce[i] = (uint8_t)(esp_random() & 0xFF);

    if (!aesCtrCrypt(AES_KEY, nonce, ct.get(), ptLen)) return false;

    const size_t hdrLen = 1 + NONCE_LEN;
    const size_t frameLen = hdrLen + ptLen + MAC_LEN;
    std::unique_ptr<uint8_t[]> frame(new uint8_t[frameLen]);
    frame[0] = sensorId;
    memcpy(frame.get()+1, nonce, NONCE_LEN);
    memcpy(frame.get()+hdrLen, ct.get(), ptLen);

    uint8_t mac[MAC_LEN];
    if (!hmacSha256Trunc(HMAC_KEY, sizeof(HMAC_KEY), frame.get(), hdrLen + ptLen, mac, MAC_LEN)) return false;
    memcpy(frame.get()+hdrLen+ptLen, mac, MAC_LEN);

    LoRa.beginPacket();
    LoRa.write(frame.get(), frameLen);
    LoRa.endPacket();
    return true;
}
