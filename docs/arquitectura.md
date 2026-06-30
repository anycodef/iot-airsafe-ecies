# Arquitectura — AirSafe-ECIES

Documento técnico de la arquitectura de tres planos del nodo IoT de monitoreo de
calidad del aire y detección de fugas de gas con telemetría cifrada ECIES.

![Arquitectura general](diagramas/arquitectura-general.png)

## Visión general

```
┌─────────────────────────┐      MQTT (cifrado)     ┌──────────────────────────┐      HTTPS POST     ┌───────────────────────┐
│        EDGE              │ ─────────────────────▶ │        GATEWAY           │ ──────────────────▶ │        CLOUD          │
│  ESP32-S3 DevKitC-1      │   airsafe/telemetria   │  Raspberry Pi 4 (Docker) │   JSON en claro     │  Google Apps Script   │
│  · sensado               │   airsafe/alerta       │  · Mosquitto (broker)    │                     │  · doPost()           │
│  · cifrado ECIES         │                        │  · Node-RED (descifra)   │                     │  · Google Sheets      │
│  · actuación fail-safe   │                        │  · ETL + reenvío         │                     │  · histórico/panel    │
└─────────────────────────┘                        └──────────────────────────┘                     └───────────────────────┘
```

## Plano Edge — ESP32-S3

**Responsabilidad:** sensar el ambiente, ejecutar la protección local y cifrar la
telemetría antes de transmitirla.

- **Framework:** Arduino-ESP32 sobre PlatformIO.
- **Sensado:** BME280 (I²C), MQ-2 y MQ-135 (ADC), PIR y LDR (GPIO/ADC).
- **Actuación:** relé 4ch, servo SG90 (electroválvula), buzzer, LED RGB, OLED SSD1306.
- **Control de acceso:** RFID RC522 (SPI) para rearme/silencio autorizado.
- **Cripto:** ECIES con micro-ecc (ECDH P-256) + mbedTLS (SHA-256, AES-128-CTR).

**Principio fail-safe:** la detección de fuga (`is_gas_alert`) y la actuación
(cerrar válvula, cortar relé, alarma) ocurren en el lazo local **antes** de tocar la
red. El nodo protege el ambiente aunque no haya WiFi, broker ni nube. La alarma de gas
queda **enclavada** hasta que una tarjeta RFID autorizada rearme el sistema.

## Plano Gateway — Raspberry Pi 4

**Responsabilidad:** recibir telemetría cifrada, descifrarla, normalizarla y reenviarla.

- **Mosquitto:** broker MQTT que recibe los tópicos `airsafe/telemetria` y `airsafe/alerta`.
- **Node-RED:** suscribe a los tópicos, ejecuta el nodo función **ECIES Decrypt**
  (Node.js `crypto`: ECDH + SHA-256 + AES-128-CTR), aplica ETL y hace `POST` HTTPS a la nube.
- **Docker Compose:** despliegue reproducible multi-arch (amd64 + arm64).

La clave privada del backend vive **solo** en el gateway (variable de entorno
`BACKEND_PRIV_KEY_HEX`), nunca en el firmware ni en el repositorio.

## Plano Cloud — Google Apps Script + Sheets

**Responsabilidad:** persistir la telemetría e implementar el panel histórico.

- **`doPost(e)`:** recibe el JSON descifrado y agrega una fila a la hoja `Telemetria`.
- **Google Sheets:** almacenamiento, visualización y base para analítica/alertas.

## Flujos de datos y tópicos

| Tópico MQTT | Origen | Contenido | Periodicidad |
|-------------|--------|-----------|--------------|
| `airsafe/telemetria` | ESP32-S3 | Envoltura ECIES `{epk, iv, ct}` | cada 30 s |
| `airsafe/alerta` | ESP32-S3 | Igual envoltura, ante fuga de gas | inmediata |

## Seguridad de la arquitectura

- **Confidencialidad de extremo a extremo:** el payload viaja cifrado incluso dentro de la LAN.
- **Perfect Forward Secrecy:** clave efímera por mensaje (ver [protocolo-ecies.md](protocolo-ecies.md)).
- **Separación de secretos:** la pública en el firmware, la privada solo en el gateway.
- **Resiliencia:** la lógica de seguridad no depende de la disponibilidad de la nube.
