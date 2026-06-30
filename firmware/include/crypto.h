/**
 * @file    crypto.h
 * @brief   Esquema ECIES (ECDH P-256 + KDF SHA-256 + AES-128-CTR) para el nodo.
 *
 * Cada mensaje usa un par de claves EFÍMERO → Perfect Forward Secrecy (PFS).
 * El resultado de ecies_encrypt() está listo para empaquetar en JSON/MQTT.
 */
#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// Longitudes fijas del esquema P-256.
#define ECC_PUBKEY_LEN     64    // X(32) || Y(32) — formato uECC (sin 0x04)
#define ECC_PRIVKEY_LEN    32
#define ECC_SHARED_LEN     32    // coordenada X del punto compartido
#define AES_KEY_LEN        16    // AES-128
#define AES_IV_LEN         16    // bloque/contador CTR

/**
 * @brief Salida de ecies_encrypt(): todo lo que el backend necesita para descifrar.
 *        Los tres campos van codificados en Base64.
 */
struct EciesPayload {
    String ephemeral_pubkey_b64;  // clave pública efímera (65 bytes, 0x04||X||Y)
    String iv_b64;                // vector inicial del contador CTR
    String ciphertext_b64;        // telemetría cifrada
};

/**
 * @brief Inicializa el RNG de uECC con la fuente de entropía del ESP32.
 *        Llamar una vez en setup().
 */
void crypto_init();

/**
 * @brief Cifra `plaintext` para la clave pública del backend usando ECIES.
 *
 * Pasos: keygen efímero → ECDH → KDF SHA-256 → AES-128-CTR.
 *
 * @param backend_pubkey  Clave pública del backend, 65 bytes (0x04||X||Y).
 * @param plaintext       Datos en claro (JSON de telemetría).
 * @param len             Longitud de `plaintext`.
 * @param out             Estructura de salida con los 3 campos Base64.
 * @return true si el cifrado fue exitoso.
 */
bool ecies_encrypt(const unsigned char *backend_pubkey,
                   const unsigned char *plaintext, size_t len,
                   EciesPayload &out);

/**
 * @brief Sobrecarga conveniente que recibe/serializa Strings de Arduino.
 */
bool ecies_encrypt(const unsigned char *backend_pubkey,
                   const String &plaintext, EciesPayload &out);

#endif  // CRYPTO_H
