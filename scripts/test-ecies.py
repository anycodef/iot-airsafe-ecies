#!/usr/bin/env python3
"""
test-ecies.py — Validación de extremo a extremo del canal ECIES de AirSafe-ECIES.

Simula el cifrado que hace el ESP32-S3 (firmware/src/crypto.cpp) y el descifrado
que hace Node-RED (gateway/node-red/flows.json), e imprime cada paso del esquema:
keygen efímero → ECDH → KDF SHA-256 → AES-128-CTR.

Esquema:  ECDH P-256 + SHA-256[0:16] + AES-128-CTR, par efímero por mensaje (PFS).

Requisitos:
    pip install cryptography

Uso:
    python scripts/test-ecies.py
"""
import base64
import hashlib
import json
import os

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.serialization import (
    Encoding, PublicFormat,
)

CURVE = ec.SECP256R1()


def hexd(label: str, data: bytes) -> None:
    print(f"  {label:<22}: {data.hex()}")


# ───────────────────────── Lado ESP32-S3 (cifrado) ─────────────────────────
def ecies_encrypt(backend_pub: ec.EllipticCurvePublicKey, plaintext: bytes) -> dict:
    print("\n[ESP32-S3] Cifrado ECIES")

    # 1) Par EFÍMERO por mensaje (esto otorga Perfect Forward Secrecy).
    eph_priv = ec.generate_private_key(CURVE)
    eph_pub_bytes = eph_priv.public_key().public_bytes(
        Encoding.X962, PublicFormat.UncompressedPoint   # 0x04||X||Y (65 B)
    )
    hexd("clave efimera (epk)", eph_pub_bytes)

    # 2) ECDH → secreto compartido (coordenada X, 32 B).
    shared = eph_priv.exchange(ec.ECDH(), backend_pub)
    hexd("secreto ECDH", shared)

    # 3) KDF: clave AES-128 = SHA-256(shared)[0:16].
    key = hashlib.sha256(shared).digest()[:16]
    hexd("clave AES-128 (KDF)", key)

    # 4) AES-128-CTR con IV aleatorio.
    iv = os.urandom(16)
    hexd("IV (CTR)", iv)
    encryptor = Cipher(algorithms.AES(key), modes.CTR(iv)).encryptor()
    ciphertext = encryptor.update(plaintext) + encryptor.finalize()
    hexd("ciphertext", ciphertext)

    # 5) Envoltura Base64 (igual que el firmware publica por MQTT).
    return {
        "epk": base64.b64encode(eph_pub_bytes).decode(),
        "iv": base64.b64encode(iv).decode(),
        "ct": base64.b64encode(ciphertext).decode(),
    }


# ───────────────────────── Lado Node-RED (descifrado) ──────────────────────
def ecies_decrypt(backend_priv: ec.EllipticCurvePrivateKey, env: dict) -> bytes:
    print("\n[Node-RED] Descifrado ECIES")

    epk = base64.b64decode(env["epk"])
    iv = base64.b64decode(env["iv"])
    ct = base64.b64decode(env["ct"])

    # 1) Reconstruir la clave pública efímera del nodo.
    eph_pub = ec.EllipticCurvePublicKey.from_encoded_point(CURVE, epk)

    # 2) ECDH con la clave privada estática del backend → mismo secreto.
    shared = backend_priv.exchange(ec.ECDH(), eph_pub)
    hexd("secreto ECDH", shared)

    # 3) Misma KDF.
    key = hashlib.sha256(shared).digest()[:16]
    hexd("clave AES-128 (KDF)", key)

    # 4) AES-128-CTR descifrado.
    decryptor = Cipher(algorithms.AES(key), modes.CTR(iv)).decryptor()
    plaintext = decryptor.update(ct) + decryptor.finalize()
    return plaintext


def main() -> int:
    print("=" * 64)
    print(" AirSafe-ECIES — prueba de canal cifrado de extremo a extremo")
    print("=" * 64)

    # Par estático del backend (en producción viene de scripts/keygen.sh).
    backend_priv = ec.generate_private_key(CURVE)
    backend_pub = backend_priv.public_key()
    print("\n[setup] Par estático del backend generado (P-256)")

    # Telemetría de ejemplo, igual al JSON que arma el firmware.
    telemetria = {
        "ts": 30000, "temp": 23.4, "hum": 61.0, "pres": 1009.2,
        "mq2_ppm": 180, "mq135_ppm": 420, "pir": False,
        "ldr_dark": False, "gas_alert": False,
    }
    plaintext = json.dumps(telemetria, separators=(",", ":")).encode()
    print(f"\n[plano] {plaintext.decode()}")

    envelope = ecies_encrypt(backend_pub, plaintext)
    print("\n[MQTT] envoltura publicada:")
    print("  " + json.dumps(envelope))

    recovered = ecies_decrypt(backend_priv, envelope)
    print(f"\n[recuperado] {recovered.decode()}")

    ok = recovered == plaintext
    print("\n" + ("✅ ÉXITO: el texto descifrado coincide con el original."
                   if ok else "❌ FALLO: los textos no coinciden."))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
