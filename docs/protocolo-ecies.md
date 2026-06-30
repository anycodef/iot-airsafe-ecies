# Protocolo ECIES — AirSafe-ECIES

Explicación paso a paso del esquema de cifrado de extremo a extremo usado entre el
nodo ESP32-S3 (emisor) y el backend Node-RED (receptor).

![Flujo ECIES](diagramas/flujo-ecies.png)

## ¿Qué es ECIES?

**ECIES (Elliptic Curve Integrated Encryption Scheme)** es un esquema de cifrado
**híbrido**: usa criptografía de curva elíptica para **acordar** una clave simétrica y
luego un cifrador simétrico (AES) para cifrar los datos. Combina la seguridad de la
clave pública con la eficiencia del cifrado simétrico — ideal para un microcontrolador.

## Primitivas

| Primitiva | Valor | Rol |
|-----------|-------|-----|
| Curva | NIST P-256 / secp256r1 | Acuerdo de clave (ECDH). |
| Acuerdo | ECDH | Deriva el secreto compartido. |
| Par de claves | Efímero por mensaje | Perfect Forward Secrecy. |
| KDF | SHA-256 (16 primeros bytes) | Secreto ECDH → clave AES. |
| Cifrado | AES-128-CTR | Cifra el payload. |
| Codificación | Base64 | Transporte en JSON/MQTT. |

## Actores y claves

- **Backend (Node-RED):** par **estático** `(priv_B, pub_B)`.
  - `pub_B` se incrusta en el firmware (`config.h → BACKEND_PUBKEY`).
  - `priv_B` vive solo en el gateway (`BACKEND_PRIV_KEY_HEX`).
- **Nodo (ESP32-S3):** genera un par **efímero** `(priv_E, pub_E)` **por cada mensaje**.

## Cifrado (ESP32-S3) — `firmware/src/crypto.cpp`

1. **Keygen efímero**
   `uECC_make_key(pub_E, priv_E, secp256r1)` — par nuevo por mensaje.

2. **ECDH**
   `S = uECC_shared_secret(pub_B, priv_E)` → secreto compartido `S` (coordenada X, 32 B).

3. **KDF**
   `K = SHA-256(S)[0:16]` → clave AES-128 (`mbedtls_sha256`).

4. **Cifrado simétrico**
   `IV ← TRNG(16)`, `C = AES-128-CTR(K, IV, plaintext)` (`mbedtls_aes_crypt_ctr`).

5. **Empaquetado**
   Se publica `{ epk: B64(0x04‖X‖Y), iv: B64(IV), ct: B64(C) }`.

## Descifrado (Node-RED) — `gateway/node-red/flows.json`

1. **Decodifica** `epk`, `iv`, `ct` desde Base64.
2. **ECDH inverso**
   `S = ECDH(priv_B, pub_E)` (`crypto.createECDH('prime256v1')`). Da el **mismo** `S`
   por la propiedad de simetría del producto escalar: `priv_E·pub_B = priv_B·pub_E`.
3. **KDF idéntica:** `K = SHA-256(S)[0:16]`.
4. **Descifra:** `plaintext = AES-128-CTR(K, IV, C)`.
5. **Parsea** el JSON de telemetría.

## ¿Por qué Perfect Forward Secrecy?

Cada mensaje usa un `priv_E` distinto que se **descarta** tras cifrar. Aunque un atacante:

- capture todo el tráfico MQTT, o
- comprometa más tarde una clave efímera,

…no puede descifrar mensajes **anteriores ni posteriores**, porque cada uno depende de un
secreto ECDH único e irrecuperable. Comprometer `priv_B` permitiría descifrar tráfico
futuro, pero no el pasado ya transmitido con claves efímeras desechadas.

## Consideraciones y mejoras futuras

- **Integridad/autenticidad:** este esquema da confidencialidad. Para detectar manipulación
  conviene añadir un **MAC** (HMAC-SHA-256 sobre `C`) derivando una segunda clave de la KDF,
  o migrar a **AES-128-GCM** (AEAD).
- **KDF robusta:** sustituir SHA-256 directo por **HKDF-SHA-256** con `info`/`salt`.
- **Validación de punto:** rechazar claves efímeras que no estén en la curva.

## Validación

`scripts/test-ecies.py` reproduce ambos lados (cifrado del nodo y descifrado del backend)
e imprime cada paso, confirmando que el canal funciona de extremo a extremo.
