/**
 * @file    main.cpp
 * @brief   Lazo principal del nodo AirSafe-ECIES.
 *
 *  Ciclo: sensar → (fail-safe local) → construir JSON → cifrar ECIES → publicar MQTT.
 *
 *  Garantías de diseño:
 *    - FAIL-SAFE: la detección de fuga y el corte de gas ocurren ANTES de tocar la
 *      red; si no hay WiFi/MQTT/nube, la protección local sigue operando.
 *    - CONFIDENCIALIDAD: la telemetría se publica SIEMPRE cifrada con ECIES.
 *    - PFS: cada mensaje usa una clave efímera distinta (ver crypto.cpp).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "config.h"
#include "crypto.h"
#include "sensors.h"
#include "actuators.h"

// Declaradas en rfid_access.cpp.
void rfid_init();
bool rfid_check_rearm();

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static unsigned long last_sample = 0;
static bool          gas_latched = false;   // alarma de gas enclavada hasta rearme RFID

// ─────────────────────────────────────────────────────────────────────────────
//  Conectividad
// ─────────────────────────────────────────────────────────────────────────────
static void wifi_connect() {
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(250);
    }
    Serial.println(WiFi.status() == WL_CONNECTED
                       ? "[wifi] conectado"
                       : "[wifi] sin conexion (sigue lógica local)");
}

/** Reintenta MQTT sin bloquear la lógica local de seguridad. */
static bool mqtt_ensure() {
    if (mqtt.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    mqtt.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        Serial.println("[mqtt] conectado");
        return true;
    }
    Serial.printf("[mqtt] fallo rc=%d\n", mqtt.state());
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Serialización + cifrado + publicación
// ─────────────────────────────────────────────────────────────────────────────
static String build_json(const SensorData &d, bool gas_alert) {
    JsonDocument doc;
    doc["ts"]        = millis();
    doc["temp"]      = round(d.temp * 10) / 10.0;
    doc["hum"]       = round(d.hum * 10) / 10.0;
    doc["pres"]      = round(d.pres * 10) / 10.0;
    doc["mq2_ppm"]   = round(d.mq2_ppm);
    doc["mq135_ppm"] = round(d.mq135_ppm);
    doc["pir"]       = d.pir;
    doc["ldr_dark"]  = d.ldr_dark;
    doc["gas_alert"] = gas_alert;
    String out;
    serializeJson(doc, out);
    return out;
}

/** Cifra el JSON con ECIES y lo publica en `topic`. */
static bool publish_encrypted(const char *topic, const String &json) {
    if (!mqtt_ensure()) return false;

    EciesPayload pl;
    if (!ecies_encrypt(BACKEND_PUBKEY, json, pl)) {
        Serial.println("[ecies] cifrado fallo");
        return false;
    }

    // Envoltura JSON con los 3 campos del esquema ECIES (todos Base64).
    JsonDocument env;
    env["epk"] = pl.ephemeral_pubkey_b64;   // clave pública efímera
    env["iv"]  = pl.iv_b64;                  // IV del CTR
    env["ct"]  = pl.ciphertext_b64;          // ciphertext
    String payload;
    serializeJson(env, payload);

    bool ok = mqtt.publish(topic, payload.c_str());
    Serial.printf("[mqtt] publish %s (%u B) → %s\n", topic, payload.length(), ok ? "ok" : "err");
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fail-safe: actuación local inmediata ante fuga de gas.
// ─────────────────────────────────────────────────────────────────────────────
static void handle_gas_alert(const SensorData &d) {
    gas_latched = true;
    set_status(RED);
    alarm_on();
    close_valve();          // corta el suministro
    activate_relay(1);      // corta la línea eléctrica de gas
    display_message("FUGA DE GAS", "Valvula CERRADA");

    // Publicación de alerta inmediata (best-effort; no bloquea la protección).
    String json = build_json(d, true);
    publish_encrypted(TOPIC_ALERTA, json);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setup / Loop
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== AirSafe-ECIES — nodo ESP32-S3 ===");

    crypto_init();          // RNG para uECC (PFS)
    sensors_init();
    actuators_init();
    rfid_init();

    wifi_connect();
    mqtt.setBufferSize(1024);   // payload ECIES + JSON supera el default de 256 B
    mqtt_ensure();

    display_message("AirSafe-ECIES", "Operativo");
}

void loop() {
    if (mqtt.connected()) mqtt.loop();

    // Rearme/silencio con tarjeta RFID autorizada.
    if (gas_latched && rfid_check_rearm()) {
        gas_latched = false;
    }

    unsigned long now = millis();
    if (now - last_sample < SAMPLE_INTERVAL_MS && last_sample != 0) {
        return;
    }
    last_sample = now;

    // 1) Sensar.
    SensorData d;
    sensors_read(d);

    // 2) Fail-safe local PRIMERO (no depende de la red).
    bool gas = is_gas_alert(d);
    if (gas) {
        handle_gas_alert(d);
    } else if (!gas_latched) {
        // Sin enclavamiento activo → estado normal/advertencia.
        if (is_air_quality_alert(d)) {
            set_status(YELLOW);
        } else {
            set_status(GREEN);
            alarm_off();
        }
        display_readings(d);
    }

    // 3) Telemetría cifrada periódica.
    String json = build_json(d, gas);
    publish_encrypted(TOPIC_TELEMETRIA, json);

    // 4) Reconexión perezosa (la lógica local ya corrió pase lo que pase).
    if (WiFi.status() != WL_CONNECTED) wifi_connect();
}
