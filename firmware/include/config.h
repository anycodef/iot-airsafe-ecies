/**
 * @file    config.h
 * @brief   Configuración central del nodo AirSafe-ECIES (ESP32-S3).
 *
 * Ajusta credenciales, parámetros de red, tópicos MQTT, umbrales de sensado
 * y la clave pública del backend (generada con scripts/keygen.sh).
 *
 * @warning No commitees credenciales reales. Para secretos productivos usa
 *          un archivo secrets.h (ignorado por .gitignore).
 */
#ifndef CONFIG_H
#define CONFIG_H

// ─────────────────────────────────────────────────────────────────────────────
//  Red WiFi
// ─────────────────────────────────────────────────────────────────────────────
#define WIFI_SSID            "TU_SSID"
#define WIFI_PASS            "TU_PASSWORD"
#define WIFI_TIMEOUT_MS      15000

// ─────────────────────────────────────────────────────────────────────────────
//  Broker MQTT (Mosquitto en la Raspberry Pi)
// ─────────────────────────────────────────────────────────────────────────────
#define MQTT_BROKER_IP       "192.168.1.50"
#define MQTT_BROKER_PORT     1883
#define MQTT_CLIENT_ID       "airsafe-esp32s3"
#define MQTT_USER            "airsafe"
#define MQTT_PASS            "changeme"

#define TOPIC_TELEMETRIA     "airsafe/telemetria"
#define TOPIC_ALERTA         "airsafe/alerta"

// ─────────────────────────────────────────────────────────────────────────────
//  Periodicidad
// ─────────────────────────────────────────────────────────────────────────────
#define SAMPLE_INTERVAL_MS   30000UL      // 30 s entre lecturas de telemetría

// ─────────────────────────────────────────────────────────────────────────────
//  Pinout ESP32-S3 DevKitC-1
// ─────────────────────────────────────────────────────────────────────────────
// I2C (BME280 + OLED SSD1306)
#define PIN_I2C_SDA          8
#define PIN_I2C_SCL          9
#define OLED_I2C_ADDR        0x3C
#define BME280_I2C_ADDR      0x76

// ADC (sensores analógicos)
#define PIN_MQ2              4
#define PIN_MQ135            5
#define PIN_LDR              6

// GPIO digitales
#define PIN_PIR              7
#define PIN_BUZZER           10
#define PIN_LED_R            11
#define PIN_LED_G            12
#define PIN_LED_B            13

// Relé 4 canales
#define PIN_RELAY_1          14   // corte de gas / línea principal
#define PIN_RELAY_2          15
#define PIN_RELAY_3          16
#define PIN_RELAY_4          17

// Servo SG90 (electroválvula)
#define PIN_SERVO            18

// RFID RC522 (SPI)
#define PIN_RFID_SS          21
#define PIN_RFID_RST         47
#define PIN_RFID_SCK         12   // ajustar al bus SPI físico
#define PIN_RFID_MOSI        11
#define PIN_RFID_MISO        13

// ─────────────────────────────────────────────────────────────────────────────
//  Umbrales de alerta / calibración de sensores
// ─────────────────────────────────────────────────────────────────────────────
#define MQ2_GAS_THRESHOLD_PPM    2000.0f   // fuga de GLP/humo → alarma
#define MQ135_AQI_THRESHOLD_PPM  1000.0f   // CO2 eq alto → ventilar
#define LDR_DARK_THRESHOLD       1500      // lectura ADC bajo umbral = oscuridad

#define MQ2_RL_KOHM          5.0f          // resistencia de carga MQ-2
#define MQ2_R0_KOHM          9.8f          // R0 calibrado en aire limpio
#define MQ135_RL_KOHM        20.0f
#define MQ135_R0_KOHM        76.0f
#define ADC_MAX              4095.0f
#define ADC_VREF             3.3f

// ─────────────────────────────────────────────────────────────────────────────
//  Servo: ángulos de válvula
// ─────────────────────────────────────────────────────────────────────────────
#define SERVO_VALVE_OPEN_DEG     90
#define SERVO_VALVE_CLOSED_DEG   0

// ─────────────────────────────────────────────────────────────────────────────
//  Clave pública del backend — ECC P-256, punto SIN comprimir (65 bytes).
//  Formato: 0x04 || X(32) || Y(32). Generar con: ./scripts/keygen.sh
//  (Estos bytes son un PLACEHOLDER de ejemplo; reemplázalos por los tuyos.)
// ─────────────────────────────────────────────────────────────────────────────
static const unsigned char BACKEND_PUBKEY[65] = {
    0x04,
    0x3b, 0x9a, 0xca, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d
};

#endif  // CONFIG_H
