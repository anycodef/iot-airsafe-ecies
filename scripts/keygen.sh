#!/usr/bin/env bash
#
# keygen.sh — Genera el par de claves ECC P-256 del backend para AirSafe-ECIES.
#
#   · keys/backend_priv.pem   → clave privada (úsala en Node-RED / variable de entorno)
#   · keys/backend_pub.pem    → clave pública (PEM/SPKI)
#   · Imprime la clave pública como arreglo C de 65 bytes para firmware/include/config.h
#   · Imprime el escalar privado en hex para BACKEND_PRIV_KEY_HEX (Node-RED)
#
# Requiere: OpenSSL 3.x
#
set -euo pipefail

KEY_DIR="keys"
PRIV="${KEY_DIR}/backend_priv.pem"
PUB="${KEY_DIR}/backend_pub.pem"

command -v openssl >/dev/null 2>&1 || { echo "❌ OpenSSL no encontrado."; exit 1; }

mkdir -p "${KEY_DIR}"

echo "🔑 Generando par ECC P-256 (prime256v1)..."
# 1) Clave privada (formato PKCS#8/SEC1).
openssl ecparam -name prime256v1 -genkey -noout -out "${PRIV}"

# 2) Clave pública (SPKI/PEM).
openssl ec -in "${PRIV}" -pubout -out "${PUB}" 2>/dev/null

echo "✅ Claves guardadas en ${KEY_DIR}/"
echo

# ─── Clave pública como bytes SEC1 sin comprimir (0x04||X||Y, 65 bytes) ───
# El texto de 'openssl ec -text' incluye el bloque "pub:" en hex.
PUB_HEX=$(openssl ec -in "${PRIV}" -text -noout 2>/dev/null \
  | sed -n '/pub:/,/ASN1 OID/p' \
  | grep -E '^[[:space:]]+[0-9a-f]{2}:' \
  | tr -d ' :\n')

echo "──────────────────────────────────────────────────────────────"
echo " 1) Pega esto en firmware/include/config.h → BACKEND_PUBKEY[65]:"
echo "──────────────────────────────────────────────────────────────"
echo -n "static const unsigned char BACKEND_PUBKEY[65] = {"
echo "${PUB_HEX}" | fold -w2 | awk '{printf "%s0x%s,", (NR-1)%12?" ":"\n    ", $0}'
echo
echo "};"
echo

# ─── Escalar privado en hex (32 bytes) para Node-RED ───
PRIV_HEX=$(openssl ec -in "${PRIV}" -text -noout 2>/dev/null \
  | sed -n '/priv:/,/pub:/p' \
  | grep -E '^[[:space:]]+[0-9a-f]{2}:' \
  | tr -d ' :\n')

echo "──────────────────────────────────────────────────────────────"
echo " 2) Exporta esto para Node-RED (docker-compose / .env):"
echo "──────────────────────────────────────────────────────────────"
echo "BACKEND_PRIV_KEY_HEX=${PRIV_HEX}"
echo
echo "⚠  NO commitees keys/ — ya está en .gitignore."
