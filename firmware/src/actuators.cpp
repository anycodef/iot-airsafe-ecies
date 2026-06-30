/**
 * @file    actuators.cpp
 * @brief   Control de actuadores: relé 4ch, servo SG90, buzzer, LED RGB y OLED.
 */
#include "actuators.h"
#include "config.h"

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_SSD1306.h>

#define OLED_W 128
#define OLED_H 64

static Servo valve_servo;
static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static bool oled_ok = false;

static const uint8_t RELAY_PINS[4] = {
    PIN_RELAY_1, PIN_RELAY_2, PIN_RELAY_3, PIN_RELAY_4
};

bool actuators_init() {
    // Relés (lógica activa en LOW en la mayoría de módulos → arrancan desactivados).
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH);
    }

    // Buzzer + LED RGB.
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    set_status(GREEN);

    // Servo de la electroválvula (arranca ABIERTA: estado seguro de suministro).
    valve_servo.attach(PIN_SERVO);
    open_valve();

    // OLED.
    oled_ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    if (oled_ok) {
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
        display_message("AirSafe-ECIES", "Iniciando...");
    }
    return oled_ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Relé 4 canales (channel: 1..4). Módulos comunes: LOW = activo.
// ─────────────────────────────────────────────────────────────────────────────
void activate_relay(uint8_t channel) {
    if (channel < 1 || channel > 4) return;
    digitalWrite(RELAY_PINS[channel - 1], LOW);
}

void deactivate_relay(uint8_t channel) {
    if (channel < 1 || channel > 4) return;
    digitalWrite(RELAY_PINS[channel - 1], HIGH);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Electroválvula con servo SG90.
// ─────────────────────────────────────────────────────────────────────────────
void open_valve()  { valve_servo.write(SERVO_VALVE_OPEN_DEG); }
void close_valve() { valve_servo.write(SERVO_VALVE_CLOSED_DEG); }

// ─────────────────────────────────────────────────────────────────────────────
//  Buzzer.
// ─────────────────────────────────────────────────────────────────────────────
void alarm_on()  { digitalWrite(PIN_BUZZER, HIGH); }
void alarm_off() { digitalWrite(PIN_BUZZER, LOW); }

// ─────────────────────────────────────────────────────────────────────────────
//  LED RGB de estado.
// ─────────────────────────────────────────────────────────────────────────────
void set_status(StatusColor color) {
    bool r = (color == RED)    || (color == YELLOW);
    bool g = (color == GREEN)  || (color == YELLOW);
    bool b = false;
    digitalWrite(PIN_LED_R, r ? HIGH : LOW);
    digitalWrite(PIN_LED_G, g ? HIGH : LOW);
    digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

// ─────────────────────────────────────────────────────────────────────────────
//  OLED SSD1306.
// ─────────────────────────────────────────────────────────────────────────────
void display_readings(const SensorData &d) {
    if (!oled_ok) return;
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.printf("T:%.1fC H:%.0f%%\n", d.temp, d.hum);
    oled.printf("P:%.0f hPa\n", d.pres);
    oled.printf("MQ2 :%.0f ppm\n", d.mq2_ppm);
    oled.printf("MQ135:%.0f ppm\n", d.mq135_ppm);
    oled.printf("PIR:%s LDR:%s", d.pir ? "1" : "0", d.ldr_dark ? "dark" : "ok");
    oled.display();
}

void display_message(const String &line1, const String &line2) {
    if (!oled_ok) return;
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.setTextSize(1);
    oled.println(line1);
    oled.println();
    oled.println(line2);
    oled.display();
}
