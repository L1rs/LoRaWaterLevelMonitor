#pragma once
// Deutsche Dokumentation
// Krypto-Helfer: AES-CTR (AES-128) und HMAC-SHA256 (trunkiert)
// Hinweis: Schlüssel stammen aus config.h der jeweiligen Boards.

#include <cstddef>
#include <cstdint>

// AES-128 im CTR-Modus, in-place Verschlüsselung/Entschlüsselung.
// - key: 16-Byte Schlüssel
// - nonce8: 8-Byte Nonce (wird auf 16-Byte IV erweitert)
// - data: Datenpuffer (wird in-place bearbeitet)
// - len: Länge des Puffers
bool aesCtrCrypt(const uint8_t key[16], const uint8_t nonce8[8], uint8_t* data, size_t len);

// HMAC-SHA256 berechnen und auf outLen Bytes kürzen.
// - key/keyLen: HMAC-Schlüssel
// - msg/msgLen: Nachricht
// - out/outLen: Ausgabepuffer und gewünschte Länge (z. B. 8 oder 16)
bool hmacSha256Trunc(const uint8_t* key, size_t keyLen,
                     const uint8_t* msg, size_t msgLen,
                     uint8_t* out, size_t outLen);
