/**
 * @file    sensors.h
 * @brief   Lectura de sensores: BME280 (I2C), MQ-2/MQ-135 (ADC), PIR/LDR (GPIO).
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

/**
 * @brief Snapshot de una lectura completa del entorno.
 */
struct SensorData {
    float temp;        // °C        (BME280)
    float hum;         // % HR      (BME280)
    float pres;        // hPa       (BME280)
    float mq2_ppm;     // ppm GLP/humo   (MQ-2)
    float mq135_ppm;   // ppm CO2 eq      (MQ-135)
    bool  pir;         // true = movimiento detectado
    bool  ldr_dark;    // true = ambiente oscuro
};

/** @brief Inicializa los sensores (I2C, ADC, GPIO). @return false si el BME280 no responde. */
bool sensors_init();

/** @brief Realiza una lectura completa y la devuelve en `out`. */
void sensors_read(SensorData &out);

/** @brief Evalúa si la concentración del MQ-2 supera el umbral de fuga de gas. */
bool is_gas_alert(const SensorData &d);

/** @brief Evalúa si la calidad del aire (MQ-135) está degradada. */
bool is_air_quality_alert(const SensorData &d);

#endif  // SENSORS_H
