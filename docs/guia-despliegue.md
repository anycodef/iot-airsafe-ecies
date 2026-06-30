# Guía de despliegue — AirSafe-ECIES

Pasos para poner en marcha los tres planos: clonar, generar claves, configurar y
flashear el firmware, levantar el gateway y desplegar la nube.

## 0. Pre-requisitos

| Herramienta | Versión | Uso |
|-------------|---------|-----|
| PlatformIO Core | 6.x | Firmware ESP32-S3 |
| Docker + Compose | 24.x | Gateway |
| OpenSSL | 3.x | Generar claves ECC |
| Node.js | ≥ 18 | Node-RED / utilidades |
| Python | ≥ 3.10 | `test-ecies.py` |

## 1. Clonar

```bash
git clone https://github.com/anycodef/iot-airsafe-ecies.git
cd iot-airsafe-ecies
```

## 2. Generar el par de claves del backend

```bash
chmod +x scripts/keygen.sh
./scripts/keygen.sh
```

Esto crea `keys/backend_priv.pem` y `keys/backend_pub.pem`, e imprime:

- El arreglo C `BACKEND_PUBKEY[65]` → pégalo en `firmware/include/config.h`.
- `BACKEND_PRIV_KEY_HEX=...` → exponlo como variable de entorno para Node-RED.

> `keys/` está en `.gitignore`. **Nunca** subas la clave privada.

## 3. Configurar el firmware

Edita `firmware/include/config.h`:

- `WIFI_SSID`, `WIFI_PASS`.
- `MQTT_BROKER_IP` (IP de la Raspberry Pi).
- `BACKEND_PUBKEY` (del paso 2).
- Umbrales: `MQ2_GAS_THRESHOLD_PPM`, `MQ135_AQI_THRESHOLD_PPM`.

## 4. Flashear el ESP32-S3

```bash
cd firmware
pio run --environment esp32s3                 # compilar
pio run --environment esp32s3 --target upload # flashear
pio device monitor                            # ver serial (115200 baud)
```

## 5. Levantar el gateway (Raspberry Pi 4)

Crea `gateway/.env` con la clave privada del backend:

```bash
echo "BACKEND_PRIV_KEY_HEX=<hex_del_paso_2>" > gateway/.env
```

Asegúrate de que `docker-compose.yml` inyecte la variable en el servicio `node-red`
(añade `env_file: .env` o `environment: - BACKEND_PRIV_KEY_HEX=${BACKEND_PRIV_KEY_HEX}`).

```bash
cd gateway
docker compose up -d
docker compose logs -f node-red   # verificar arranque
```

- Editor Node-RED: `http://<raspberry-ip>:1880`
- Broker MQTT: `<raspberry-ip>:1883`

Si el flujo no se autocargó, importa `node-red/flows.json` desde el editor.

## 6. Desplegar la nube (Google Apps Script)

1. Crea una Google Sheet.
2. **Extensiones → Apps Script** y pega `cloud/AppScript/doPost.gs`.
3. **Implementar → Nueva implementación → Aplicación web**
   (ejecutar como tú; acceso "cualquiera").
4. Copia la URL `…/exec` y pégala en los nodos `http request` de `flows.json`
   (`POST Apps Script` y `POST alerta`).

## 7. Validar el canal cifrado

```bash
pip install cryptography
python scripts/test-ecies.py
```

Debe imprimir cada paso (keygen → ECDH → KDF → AES) y `✅ ÉXITO`.

## 8. Verificación de extremo a extremo

1. Monitor serial: el ESP32 publica cada 30 s.
2. Node-RED (debug `telemetria descifrada`): muestra el JSON en claro.
3. Google Sheet `Telemetria`: aparecen filas nuevas.
4. Prueba de fuga: acerca una fuente de gas/encendedor (sin llama) al MQ-2 → válvula
   cierra, buzzer suena, LED en rojo, llega alerta. Rearma con una tarjeta RFID autorizada.

## Solución de problemas

| Síntoma | Causa probable | Acción |
|---------|----------------|--------|
| `BME280 no detectado` | Dirección/cableado I²C | Verifica `0x76`/`0x77` y SDA/SCL |
| MQTT no conecta | IP/credenciales | Revisa `MQTT_BROKER_IP` y Mosquitto |
| Node-RED no descifra | Falta `BACKEND_PRIV_KEY_HEX` | Exporta la variable en `.env` |
| Sheets vacío | URL `/exec` incorrecta | Re-pega la URL de despliegue |
| Lecturas de gas erráticas | Falta precalentar / calibrar `R0` | Espera y recalibra en aire limpio |
