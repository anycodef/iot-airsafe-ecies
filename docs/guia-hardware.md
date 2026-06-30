# Guía de hardware — AirSafe-ECIES

Pinout del ESP32-S3 DevKitC-1 y conexiones de sensores y actuadores.

> Los pines coinciden con `firmware/include/config.h`. Ajusta según tu cableado real
> y verifica que los GPIO elegidos no colisionen con funciones reservadas del módulo.

## Mapa de pines (ESP32-S3 DevKitC-1)

| Subsistema | Componente | Interfaz | Señal | GPIO |
|------------|------------|----------|-------|------|
| Ambiente | BME280 | I²C | SDA | 8 |
| | | | SCL | 9 |
| Pantalla | OLED SSD1306 | I²C | SDA / SCL | 8 / 9 (bus compartido) |
| Gas | MQ-2 | ADC | AOUT | 4 |
| Aire | MQ-135 | ADC | AOUT | 5 |
| Luz | LDR (divisor) | ADC | Vout | 6 |
| Presencia | PIR HC-SR501 | GPIO | OUT | 7 |
| Alarma | Buzzer activo | GPIO | + | 10 |
| Estado | LED RGB | GPIO | R / G / B | 11 / 12 / 13 |
| Corte | Relé 4ch | GPIO | IN1..IN4 | 14 / 15 / 16 / 17 |
| Válvula | Servo SG90 | PWM | señal | 18 |
| Acceso | RFID RC522 | SPI | SDA(SS) | 21 |
| | | | RST | 47 |
| | | | SCK | 12 |
| | | | MOSI | 11 |
| | | | MISO | 13 |

> ⚠ El RC522 (SPI) comparte numeración de GPIO con el LED RGB en este ejemplo. En una
> placa real **asigna pines distintos** o usa un bus SPI dedicado; ajusta `config.h`.

## Notas de conexión

### Alimentación
- **BME280, OLED, RC522:** 3.3 V (¡no 5 V en RC522!).
- **MQ-2 / MQ-135:** el *heater* requiere **5 V**; su AOUT puede superar 3.3 V → usa un
  **divisor de tensión** antes del ADC del ESP32-S3 para no dañar el pin.
- **Servo SG90 y relé:** aliméntalos con **5 V externos** y une las **masas (GND)** con el ESP32.

### I²C
- BME280 y OLED comparten SDA/SCL. Direcciones por defecto: BME280 `0x76`, OLED `0x3C`.

### Sensores de gas (MQ)
- Requieren **precalentamiento** (24–48 h la primera vez; ~1–2 min en cada arranque).
- Calibra `R0` en aire limpio y ajusta `MQ2_R0_KOHM` / `MQ135_R0_KOHM` en `config.h`.

### Actuadores de seguridad
- **Relé 1** corta la línea principal de gas; **servo** cierra la electroválvula.
- Estado inicial seguro: válvula **abierta**, relés **desactivados**, hasta que se detecte fuga.

## Diagrama de conexiones

El esquema de conexiones completo (Fritzing/diagrams.net) se encuentra en
[`diagramas/`](diagramas/). Genera/actualiza el PNG y enlázalo aquí.
