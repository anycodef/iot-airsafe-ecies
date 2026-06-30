<h1 align="center">🛡️ AirSafe-ECIES</h1>
<p align="center">
  <b>Nodo IoT de monitoreo de calidad del aire y detección de fugas de gas<br>con telemetría cifrada de extremo a extremo (ECIES)</b>
</p>

<p align="center">
  <a href="../../actions/workflows/lint-firmware.yml"><img alt="PlatformIO Build" src="https://img.shields.io/badge/PlatformIO-build-orange?logo=platformio&logoColor=white"></a>
  <a href="../../actions/workflows/docker-build.yml"><img alt="Docker multi-arch" src="https://img.shields.io/badge/Docker-multi--arch%20(amd64%2Barm64)-2496ED?logo=docker&logoColor=white"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green.svg"></a>
  <a href="#-contribuir"><img alt="PRs Welcome" src="https://img.shields.io/badge/PRs-welcome-brightgreen.svg"></a>
  <img alt="ECC P-256" src="https://img.shields.io/badge/crypto-ECIES%20P--256-blueviolet">
  <img alt="Curso" src="https://img.shields.io/badge/FISI-UNMSM%20%C2%B7%20IoT-red">
</p>

---

## 📋 Descripción

**AirSafe-ECIES** es un nodo de Internet de las Cosas que monitorea de forma continua la
**calidad del aire** (temperatura, humedad, presión, gases tóxicos y CO₂ equivalente) y
detecta **fugas de gas licuado de petróleo (GLP)** en ambientes domésticos e industriales.
Cuando se supera un umbral peligroso, el nodo actúa **localmente y de inmediato** —corta el
suministro con una electroválvula, activa una alarma y notifica— sin depender de la nube.

El problema es real y urgente. Según el sistema de vigilancia de la calidad del aire del
SENAMHI, **Lima Metropolitana supera de forma recurrente los límites de PM2.5 recomendados
por la OMS**, convirtiendo la contaminación del aire en un factor de riesgo respiratorio para
millones de personas. En paralelo, las **fugas de GLP** son una causa frecuente de incendios
e intoxicaciones domésticas en el Perú, donde el balón de gas es el combustible de cocina
predominante. Un sistema de detección temprana de bajo costo tiene impacto directo en la
seguridad de los hogares.

Lo que diferencia a este proyecto es su **postura de seguridad**: toda la telemetría viaja
**cifrada de extremo a extremo con ECIES** (ECDH P-256 + KDF SHA-256 + AES-128-CTR), usando
un **par de claves efímero por cada mensaje** para garantizar **Perfect Forward Secrecy (PFS)**.
Aunque un atacante comprometa el broker MQTT o capture el tráfico, no podrá descifrar lecturas
pasadas ni futuras. A esto se suma una arquitectura **fail-safe**: la lógica crítica de
seguridad corre en el borde y sigue funcionando aunque se caigan el gateway, el WiFi o la nube.

---

## 📑 Tabla de contenidos

- [Arquitectura](#-arquitectura)
- [Seguridad — Flujo ECIES](#-seguridad--flujo-ecies)
- [Componentes de hardware](#-componentes-de-hardware)
- [Stack de software y protocolos](#-stack-de-software-y-protocolos)
- [Estructura del repositorio](#-estructura-del-repositorio)
- [Inicio rápido](#-inicio-rápido)
- [Contribuir](#-contribuir)
- [Licencia](#-licencia)
- [Referencias](#-referencias)

---

## 🏗️ Arquitectura

![Arquitectura general](docs/diagramas/arquitectura-general.png)

El sistema se organiza en **tres planos** desacoplados que se comunican mediante MQTT y HTTPS:

| Plano | Hardware / Plataforma | Responsabilidad |
|-------|-----------------------|-----------------|
| **Edge (Borde)** | ESP32-S3 DevKitC-1 (Arduino-ESP32 + mbedTLS + micro-ecc) | Sensa el ambiente, ejecuta la lógica fail-safe de actuación, **cifra** cada lectura con ECIES y la publica por MQTT. |
| **Gateway (Pasarela)** | Raspberry Pi 4 — Docker Compose (multi-arch amd64+arm64) | **Mosquitto** recibe la telemetría cifrada; **Node-RED** la **descifra**, normaliza (ETL) y reenvía a la nube por HTTPS. |
| **Cloud (Nube)** | Google Apps Script (`doPost`) + Google Sheets | Persiste la telemetría descifrada, sirve de panel histórico y permite analítica y alertas. |

- **Edge → Gateway:** MQTT sobre la LAN, _payload_ ya cifrado (confidencialidad incluso dentro de la red local).
- **Gateway → Cloud:** HTTPS `POST` con JSON en claro hacia el endpoint de Apps Script.
- **Fail-safe:** la detección de fuga y el corte de válvula **no requieren** red; ocurren en el ESP32-S3.

---

## 🔐 Seguridad — Flujo ECIES

![Flujo ECIES](docs/diagramas/flujo-ecies.png)

El nodo implementa **ECIES (Elliptic Curve Integrated Encryption Scheme)**, un esquema híbrido
que combina criptografía asimétrica para el acuerdo de clave y criptografía simétrica para el
cifrado masivo. **Por cada mensaje** el ESP32-S3 genera un par de claves efímero; esto es lo que
otorga **Perfect Forward Secrecy**.

**Pasos del esquema:**

1. El backend (Node-RED) posee un par estático ECC P-256 (`pub_B`, `priv_B`). `pub_B` está embebida en el firmware.
2. El nodo genera un par **efímero** `(pub_E, priv_E)` con `uECC_make_key()`.
3. **ECDH:** `S = ECDH(priv_E, pub_B)` → secreto compartido (coordenada X).
4. **KDF:** `K = SHA-256(S)[0:16]` → clave AES de 128 bits.
5. **AES-128-CTR:** `C = AES_CTR(K, IV, plaintext)`.
6. El nodo transmite `{ pub_E, IV, C }` en Base64. El backend deriva la misma `K` con `ECDH(priv_B, pub_E)` y descifra.

> Como `priv_E` se descarta tras cada mensaje, comprometer una clave **no** revela tráfico anterior ni posterior.

| Primitiva | Valor | Rol |
|-----------|-------|-----|
| Curva elíptica | **NIST P-256 / secp256r1** | Base del acuerdo de clave (ECDH). |
| Acuerdo de clave | **ECDH** | Deriva un secreto compartido sin transmitirlo. |
| Par de claves | **Efímero por mensaje** | Aporta **Perfect Forward Secrecy (PFS)**. |
| KDF | **SHA-256** (primeros 16 bytes) | Convierte el secreto ECDH en clave simétrica. |
| Cifrado simétrico | **AES-128-CTR** | Cifra el _payload_ de telemetría (modo flujo). |
| Codificación | **Base64** | Empaqueta clave efímera + IV + ciphertext para MQTT/JSON. |
| Formato de clave | **PEM / DER (SPKI)** | Intercambio e incrustación de la clave pública del backend. |

---

## 🔌 Componentes de hardware

| Subsistema | Componente | Interfaz |
|------------|------------|----------|
| Cómputo / Edge | ESP32-S3 DevKitC-1 | — (MCU principal) |
| Ambiente | BME280 (temperatura, humedad, presión) | I²C |
| Gas combustible | MQ-2 (GLP / humo / propano) | ADC (analógico) |
| Calidad de aire | MQ-135 (CO₂ eq, NH₃, NOx) | ADC (analógico) |
| Presencia | Sensor PIR HC-SR501 | GPIO (digital) |
| Luz ambiente | LDR (fotorresistencia) | ADC (analógico) |
| Corte de suministro | Módulo relé 4 canales | GPIO (digital) |
| Válvula | Servomotor SG90 | PWM |
| Alarma sonora | Buzzer activo | GPIO (digital) |
| Estado visual | LED RGB | GPIO (PWM) |
| Pantalla | OLED 0.96" SSD1306 | I²C |
| Control de acceso | Lector RFID RC522 | SPI |

> El pinout completo y el esquema de conexiones está en [`docs/guia-hardware.md`](docs/guia-hardware.md).

---

## 🧰 Stack de software y protocolos

| Capa | Tecnología | Función |
|------|------------|---------|
| Firmware | Arduino-ESP32 + PlatformIO | Entorno de compilación y runtime del MCU. |
| Cripto (edge) | mbedTLS + micro-ecc (uECC) | ECDH P-256, SHA-256, AES-128-CTR. |
| Sensores | Adafruit_BME280, Adafruit_SSD1306 | Drivers de sensores y pantalla. |
| Mensajería | MQTT (Mosquitto) | Transporte pub/sub de telemetría cifrada. |
| Orquestación | Node-RED | Descifrado ECIES, ETL y reenvío HTTPS. |
| Contenedores | Docker Compose (amd64 + arm64) | Despliegue reproducible del gateway. |
| Nube | Google Apps Script + Sheets API | Ingesta (`doPost`) y persistencia histórica. |
| Cripto (backend) | Node.js `crypto` (ECDH/SHA-256/AES) | Contraparte de descifrado del esquema ECIES. |
| Validación | Python `cryptography` | Pruebas de extremo a extremo del canal cifrado. |

---

## 🗂️ Estructura del repositorio

```text
iot-airsafe-ecies/
├── .github/
│   ├── workflows/
│   │   ├── lint-firmware.yml     # CI: build PlatformIO del firmware
│   │   └── docker-build.yml      # CI: build/push multi-arch del gateway
│   └── ISSUE_TEMPLATE/           # Plantillas de bug y feature
├── docs/                         # Documentación técnica (arquitectura, ECIES, hardware, despliegue)
│   └── diagramas/                # Diagramas (draw.io → PNG)
├── firmware/                     # ESP32-S3 (PlatformIO)
│   ├── platformio.ini
│   ├── include/                  # config.h, crypto.h, sensors.h, actuators.h
│   └── src/                      # main.cpp, crypto.cpp, sensors.cpp, actuators.cpp, rfid_access.cpp
├── gateway/                      # Raspberry Pi (Docker)
│   ├── docker-compose.yml
│   ├── mosquitto/                # Configuración del broker MQTT
│   └── node-red/                 # flows.json (descifrado ECIES) + settings.js
├── cloud/AppScript/doPost.gs     # Endpoint de ingesta a Google Sheets
├── scripts/
│   ├── keygen.sh                 # Genera par ECC P-256 (OpenSSL) para el backend
│   └── test-ecies.py             # Valida el canal cifrado extremo a extremo
├── .gitignore
├── LICENSE                       # MIT
└── README.md
```

---

## 🚀 Inicio rápido

### a) Pre-requisitos

| Herramienta | Versión mínima | Uso |
|-------------|----------------|-----|
| [PlatformIO Core](https://platformio.org/install/cli) | 6.x | Compilar y flashear el firmware. |
| [Docker](https://docs.docker.com/get-docker/) + Compose | 24.x | Levantar el gateway. |
| [OpenSSL](https://www.openssl.org/) | 3.x | Generar el par de claves ECC. |
| [Node.js](https://nodejs.org/) | ≥ 18 | Node-RED y utilidades. |
| [Python](https://www.python.org/) | ≥ 3.10 | Script de validación ECIES. |

### b) Clonar y generar claves

```bash
git clone https://github.com/anycodef/iot-airsafe-ecies.git
cd iot-airsafe-ecies

# Genera el par ECC P-256 del backend y la clave pública lista para config.h
chmod +x scripts/keygen.sh
./scripts/keygen.sh
```

`keygen.sh` deja `keys/backend_priv.pem` (úsalo en Node-RED) e imprime la clave pública como
arreglo C para pegar en el firmware.

### c) Configurar el firmware

Edita [`firmware/include/config.h`](firmware/include/config.h):

```c
#define WIFI_SSID        "TU_SSID"
#define WIFI_PASS        "TU_PASSWORD"
#define MQTT_BROKER_IP   "192.168.1.50"   // IP de la Raspberry Pi
#define BACKEND_PUBKEY   { 0x04, /* ... 65 bytes de keygen.sh ... */ }
#define MQ2_GAS_THRESHOLD_PPM   2000      // umbral de alerta de fuga
```

### d) Flashear el ESP32-S3

```bash
cd firmware
pio run --environment esp32s3 --target upload
pio device monitor          # ver telemetría y estado por serial
```

### e) Levantar el gateway

```bash
cd ../gateway
docker compose up -d        # mosquitto + node-red (multi-arch)
# Node-RED en http://<raspberry-ip>:1880 — importa flows.json si no se autocargó
```

### f) Desplegar el Apps Script

1. Crea una Google Sheet y abre **Extensiones → Apps Script**.
2. Pega [`cloud/AppScript/doPost.gs`](cloud/AppScript/doPost.gs).
3. **Implementar → Nueva implementación → Aplicación web** (acceso "cualquiera").
4. Copia la URL `/exec` en el nodo HTTP request de Node-RED.

> Verifica el canal de punta a punta con `python scripts/test-ecies.py`.

---

## 🤝 Contribuir

1. Crea una rama por frente: `frente-1-firmware`, `frente-2-seguridad`, `frente-3-backend`, `frente-4-integracion`.
2. Usa **Conventional Commits**:

   | Tipo | Uso |
   |------|-----|
   | `feat` | Nueva funcionalidad. |
   | `fix` | Corrección de errores. |
   | `docs` | Documentación. |
   | `chore` | Tareas de mantenimiento, CI, dependencias. |

   Ejemplo: `feat(firmware): add ECIES ephemeral keygen`
3. Abre un **Pull Request** hacia `main` con descripción y checklist de pruebas.

---

## 📄 Licencia

Distribuido bajo la **licencia MIT**. Consulta [`LICENSE`](LICENSE).

---

## 📚 Referencias

> Formato APA 7.ª edición.

1. Barker, E. (2020). *Recommendation for key management: Part 1 — General* (NIST SP 800-57 Part 1 Rev. 5). National Institute of Standards and Technology. https://doi.org/10.6028/NIST.SP.800-57pt1r5
2. Chen, L., Moody, D., Regenscheid, A., & Randall, K. (2023). *Recommendations for discrete logarithm-based cryptography: Elliptic curve domain parameters* (NIST SP 800-186). National Institute of Standards and Technology. https://doi.org/10.6028/NIST.SP.800-186
3. Dworkin, M. (2001). *Recommendation for block cipher modes of operation: Methods and techniques* (NIST SP 800-38A). National Institute of Standards and Technology. https://doi.org/10.6028/NIST.SP.800-38A
4. Martin, A., & Banks, A. (Eds.). (2019). *MQTT Version 5.0* (OASIS Standard). OASIS. https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html
5. Organización Mundial de la Salud. (2021). *WHO global air quality guidelines: Particulate matter (PM2.5 and PM10), ozone, nitrogen dioxide, sulfur dioxide and carbon monoxide*. World Health Organization. https://www.who.int/publications/i/item/9789240034228
6. Servicio Nacional de Meteorología e Hidrología del Perú. (2023). *Vigilancia de la calidad del aire en Lima Metropolitana*. SENAMHI. https://www.senamhi.gob.pe/
7. Shoup, V. (2001). *A proposal for an ISO standard for public key encryption (version 2.1)*. IACR ePrint Archive. https://eprint.iacr.org/2001/112
8. Espressif Systems. (2024). *ESP32-S3 Technical Reference Manual*. Espressif Systems. https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
