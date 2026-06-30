/**
 * @file    actuators.h
 * @brief   Control de actuadores: relé, servo (válvula), buzzer, LED RGB y OLED.
 */
#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <Arduino.h>
#include "sensors.h"

/** @brief Colores semánticos para el LED RGB de estado. */
enum StatusColor {
    GREEN,   // operación normal
    YELLOW,  // advertencia (calidad de aire degradada)
    RED      // alarma (fuga de gas)
};

/** @brief Inicializa GPIO de relé/buzzer/LED, el servo (válvula abierta) y la OLED. */
bool actuators_init();

// --- Relé 4 canales ---
void activate_relay(uint8_t channel);     // channel: 1..4
void deactivate_relay(uint8_t channel);

// --- Electroválvula (servo SG90) ---
void open_valve();
void close_valve();

// --- Alarma sonora ---
void alarm_on();
void alarm_off();

// --- LED RGB de estado ---
void set_status(StatusColor color);

// --- Pantalla OLED ---
void display_readings(const SensorData &d);
void display_message(const String &line1, const String &line2);

#endif  // ACTUATORS_H
