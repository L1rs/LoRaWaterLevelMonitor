// Deutsche Dokumentation
// Implementierung der Krypto-Helfer

#include "crypto.h"
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <cstring>

bool aesCtrCrypt(const uint8_t key[16], const uint8_t nonce8[8], uint8_t* data, size_t len)
{
    uint8_t iv[16] = {0};
    memcpy(iv, nonce8, 8);
    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    int rc = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, iv, stream_block, data, data);
    mbedtls_aes_free(&aes);
    return rc == 0;
}

bool hmacSha256Trunc(const uint8_t* key, size_t keyLen,
                     const uint8_t* msg, size_t msgLen,
                     uint8_t* out, size_t outLen)
{
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
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
