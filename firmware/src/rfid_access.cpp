/**
 * @file    rfid_access.cpp
 * @brief   Control de acceso RFID RC522 (SPI) para rearme/silencio autorizado.
 *
 *  Tras una alarma de fuga, el sistema permanece en estado seguro (válvula
 *  cerrada, buzzer activo). Solo una tarjeta RFID autorizada permite:
 *    - Silenciar la alarma.
 *    - Rearmar el nodo (reabrir válvula) una vez ventilado el ambiente.
 *
 *  Esto evita que cualquiera reanude el suministro de gas sin autorización.
 */
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include "config.h"
#include "actuators.h"

static MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);

// UIDs autorizados (4 bytes). Reemplazar por las tarjetas reales del equipo.
static const uint8_t AUTHORIZED_UIDS[][4] = {
    {0xDE, 0xAD, 0xBE, 0xEF},
    {0x12, 0x34, 0x56, 0x78},
};
static const size_t AUTHORIZED_COUNT = sizeof(AUTHORIZED_UIDS) / 4;

void rfid_init() {
    SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
    rfid.PCD_Init();
    Serial.println(F("[rfid] RC522 listo (rearme/silencio autorizado)"));
}

/** @brief Compara el UID leído contra la lista blanca. */
static bool is_authorized(const MFRC522::Uid &uid) {
    if (uid.size != 4) return false;
    for (size_t i = 0; i < AUTHORIZED_COUNT; ++i) {
        if (memcmp(uid.uidByte, AUTHORIZED_UIDS[i], 4) == 0) return true;
    }
    return false;
}

/**
 * @brief Sondea el lector. Si se presenta una tarjeta autorizada, ejecuta el
 *        rearme: silencia alarma, reabre válvula y vuelve a estado normal.
 * @return true si hubo un rearme autorizado en esta llamada.
 */
bool rfid_check_rearm() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return false;
    }

    bool ok = is_authorized(rfid.uid);
    if (ok) {
        Serial.println(F("[rfid] Tarjeta AUTORIZADA → rearmando nodo"));
        alarm_off();
        open_valve();
        deactivate_relay(1);     // restablece línea de gas
        set_status(GREEN);
        display_message("Acceso OK", "Nodo rearmado");
    } else {
        Serial.println(F("[rfid] Tarjeta NO autorizada"));
        display_message("Acceso", "DENEGADO");
    }

    rfid.PICC_HaltA();
    return ok;
}
