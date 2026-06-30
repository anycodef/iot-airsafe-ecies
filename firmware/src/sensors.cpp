/**
 * @file    sensors.cpp
 * @brief   Drivers de sensores del nodo AirSafe-ECIES.
 *
 *  - BME280  (I2C)  → temperatura, humedad, presión.
 *  - MQ-2    (ADC)  → GLP / humo / propano (ppm).
 *  - MQ-135  (ADC)  → CO2 equivalente / calidad de aire (ppm).
 *  - PIR     (GPIO) → presencia.
 *  - LDR     (ADC)  → luz ambiente (umbral oscuridad).
 *
 *  Conversión de gas: curva log-log Rs/R0 simplificada (datasheet MQ).
 *      ppm = a * (Rs/R0)^b
 *  Los coeficientes (a, b) son aproximaciones de la curva del fabricante.
 */
#include "sensors.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_BME280.h>

static Adafruit_BME280 bme;
static bool bme_ok = false;

// Coeficientes de la curva de aproximación ppm = a*(Rs/R0)^b.
static const float MQ2_A   = 574.25f,  MQ2_B   = -2.222f;   // ~GLP
static const float MQ135_A = 116.60f,  MQ135_B = -2.769f;   // ~CO2

bool sensors_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // ADC: 12 bits, rango completo 0–3.3 V.
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    pinMode(PIN_PIR, INPUT);

    bme_ok = bme.begin(BME280_I2C_ADDR, &Wire);
    if (!bme_ok) {
        Serial.println(F("[sensors] BME280 no detectado en I2C"));
    }
    return bme_ok;
}

/** Convierte una lectura ADC del MQ en concentración (ppm) vía curva Rs/R0. */
static float mq_to_ppm(int adc, float rl_kohm, float r0_kohm, float a, float b) {
    if (adc <= 0) adc = 1;                       // evita división por cero
    float vout = (adc / ADC_MAX) * ADC_VREF;     // voltaje en el divisor
    if (vout <= 0.0f) vout = 0.001f;
    float rs    = (ADC_VREF - vout) / vout * rl_kohm;   // resistencia del sensor
    float ratio = rs / r0_kohm;                          // Rs/R0
    float ppm   = a * powf(ratio, b);
    return (ppm < 0) ? 0 : ppm;
}

void sensors_read(SensorData &out) {
    // --- BME280 (I2C) ---
    if (bme_ok) {
        out.temp = bme.readTemperature();
        out.hum  = bme.readHumidity();
        out.pres = bme.readPressure() / 100.0f;   // Pa → hPa
    } else {
        out.temp = out.hum = out.pres = NAN;
    }

    // --- MQ-2 / MQ-135 (ADC) ---
    out.mq2_ppm   = mq_to_ppm(analogRead(PIN_MQ2),   MQ2_RL_KOHM,   MQ2_R0_KOHM,   MQ2_A,   MQ2_B);
    out.mq135_ppm = mq_to_ppm(analogRead(PIN_MQ135), MQ135_RL_KOHM, MQ135_R0_KOHM, MQ135_A, MQ135_B);

    // --- PIR (GPIO digital) ---
    out.pir = digitalRead(PIN_PIR) == HIGH;

    // --- LDR (ADC, umbral de oscuridad) ---
    out.ldr_dark = analogRead(PIN_LDR) < LDR_DARK_THRESHOLD;
}

bool is_gas_alert(const SensorData &d) {
    return d.mq2_ppm > MQ2_GAS_THRESHOLD_PPM;
}

bool is_air_quality_alert(const SensorData &d) {
    return d.mq135_ppm > MQ135_AQI_THRESHOLD_PPM;
}
