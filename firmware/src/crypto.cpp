/**
 * @file    crypto.cpp
 * @brief   Implementación ECIES en el borde (ESP32-S3).
 *
 *  ECIES = Elliptic Curve Integrated Encryption Scheme.
 *  Esquema híbrido: ECC para acordar la clave, AES para cifrar los datos.
 *
 *  Flujo por mensaje (otorga Perfect Forward Secrecy):
 *    1. Generar par EFÍMERO (priv_E, pub_E) con micro-ecc          → uECC_make_key
 *    2. ECDH: S = ECDH(priv_E, pub_B)  (pub_B = clave del backend)  → uECC_shared_secret
 *    3. KDF:  K = SHA-256(S)[0:16]     (clave AES-128)              → mbedtls_sha256
 *    4. AES:  C = AES-128-CTR(K, IV, plaintext)                     → mbedtls_aes_crypt_ctr
 *    5. Enviar { pub_E, IV, C } en Base64 al backend.
 *
 *  El backend recupera la misma K con ECDH(priv_B, pub_E) y descifra.
 */
#include "crypto.h"
#include "config.h"

#include <uECC.h>
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"
#include "esp_random.h"
#include <base64.h>   // Arduino-ESP32 core (base64.h)

// ─────────────────────────────────────────────────────────────────────────────
//  RNG: micro-ecc necesita una fuente de aleatoriedad criptográfica.
//  El ESP32 expone un TRNG vía esp_random() (alimentado por el subsistema RF).
// ─────────────────────────────────────────────────────────────────────────────
static int esp32_rng(uint8_t *dest, unsigned size) {
    while (size) {
        uint32_t r = esp_random();          // 32 bits de entropía hardware
        unsigned chunk = (size < 4) ? size : 4;
        memcpy(dest, &r, chunk);
        dest += chunk;
        size -= chunk;
    }
    return 1;  // 1 = éxito (contrato de uECC_RNG_Function)
}

void crypto_init() {
    // Registramos el TRNG del ESP32 como generador de uECC.
    uECC_set_rng(&esp32_rng);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paso 3 — KDF: deriva una clave AES-128 a partir del secreto ECDH.
//  Usamos SHA-256 y tomamos los primeros 16 bytes (KDF simple, estilo ECIES).
// ─────────────────────────────────────────────────────────────────────────────
static void kdf_sha256(const uint8_t *shared, size_t shared_len, uint8_t out_key[AES_KEY_LEN]) {
    uint8_t digest[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);              // 0 = SHA-256 (no SHA-224)
    mbedtls_sha256_update(&ctx, shared, shared_len);
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);

    memcpy(out_key, digest, AES_KEY_LEN);        // primeros 16 bytes → clave AES-128
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paso 4 — Cifrado AES-128 en modo CTR (flujo, sin padding).
// ─────────────────────────────────────────────────────────────────────────────
static bool aes128_ctr(const uint8_t key[AES_KEY_LEN], const uint8_t iv[AES_IV_LEN],
                       const uint8_t *in, uint8_t *out, size_t len) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {   // CTR cifra/descifra con setkey_enc
        mbedtls_aes_free(&aes);
        return false;
    }

    size_t  nc_off = 0;                  // offset dentro del bloque del stream
    uint8_t nonce_counter[AES_IV_LEN];   // mbedTLS modifica el contador in-place
    uint8_t stream_block[AES_IV_LEN];
    memcpy(nonce_counter, iv, AES_IV_LEN);

    int rc = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, nonce_counter, stream_block, in, out);
    mbedtls_aes_free(&aes);
    return rc == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ECIES completo.
// ─────────────────────────────────────────────────────────────────────────────
bool ecies_encrypt(const unsigned char *backend_pubkey,
                   const unsigned char *plaintext, size_t len,
                   EciesPayload &out) {
    const struct uECC_Curve_t *curve = uECC_secp256r1();   // P-256 / secp256r1

    // --- Paso 1: par de claves EFÍMERO (clave nueva por mensaje → PFS) ---
    uint8_t eph_priv[ECC_PRIVKEY_LEN];
    uint8_t eph_pub[ECC_PUBKEY_LEN];        // X||Y (formato uECC, sin 0x04)
    if (!uECC_make_key(eph_pub, eph_priv, curve)) {
        return false;
    }

    // micro-ecc usa la clave pública del par como X||Y (64 bytes). La del backend
    // viene en formato SEC1 sin comprimir (0x04||X||Y, 65 bytes): saltamos el 0x04.
    const uint8_t *backend_xy = backend_pubkey + 1;

    // --- Paso 2: ECDH → secreto compartido S (coordenada X, 32 bytes) ---
    uint8_t shared[ECC_SHARED_LEN];
    if (!uECC_shared_secret(backend_xy, eph_priv, shared, curve)) {
        return false;
    }

    // --- Paso 3: KDF → clave AES-128 ---
    uint8_t aes_key[AES_KEY_LEN];
    kdf_sha256(shared, sizeof(shared), aes_key);

    // --- IV aleatorio para CTR (16 bytes desde el TRNG) ---
    uint8_t iv[AES_IV_LEN];
    esp32_rng(iv, sizeof(iv));

    // --- Paso 4: AES-128-CTR ---
    uint8_t *cipher = (uint8_t *)malloc(len);
    if (!cipher) return false;
    bool ok = aes128_ctr(aes_key, iv, plaintext, cipher, len);
    if (!ok) { free(cipher); return false; }

    // --- Paso 5: empaquetar en Base64 (clave efímera en formato 0x04||X||Y) ---
    uint8_t eph_pub_sec1[1 + ECC_PUBKEY_LEN];
    eph_pub_sec1[0] = 0x04;
    memcpy(eph_pub_sec1 + 1, eph_pub, ECC_PUBKEY_LEN);

    out.ephemeral_pubkey_b64 = base64::encode(eph_pub_sec1, sizeof(eph_pub_sec1));
    out.iv_b64               = base64::encode(iv, sizeof(iv));
    out.ciphertext_b64       = base64::encode(cipher, len);

    // Higiene: borrar material sensible de la pila/heap.
    free(cipher);
    memset(eph_priv, 0, sizeof(eph_priv));
    memset(shared,   0, sizeof(shared));
    memset(aes_key,  0, sizeof(aes_key));

    return true;
}

bool ecies_encrypt(const unsigned char *backend_pubkey,
                   const String &plaintext, EciesPayload &out) {
    return ecies_encrypt(backend_pubkey,
                         (const unsigned char *)plaintext.c_str(),
                         plaintext.length(), out);
}
