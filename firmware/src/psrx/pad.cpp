//
// PS-RX - funzioni verso DS5-Linux-Bridge (vedi pad.h).
//

#include "pad.h"

#include <cstring>

#include "bt.h"
#include "slots.h"

// Ultimo report di ogni posto (main.cpp), formato USB del DualSense senza Report ID.
extern uint8_t interrupt_in_data[][63];

namespace {
// Offset del timestamp del sensore nel report del DualSense (cambia a ogni report).
constexpr uint8_t OFFSET_TIMESTAMP = 27;

uint32_t ultimo_timestamp[BT_MAX_SLOTS] = {};
uint16_t conteggio[BT_MAX_SLOTS] = {};
uint16_t al_secondo[BT_MAX_SLOTS] = {};
uint32_t t_secondo = 0;
}

uint8_t pad_posti() { return BT_MAX_SLOTS; }
int pad_connessi() { return bt_connected_count(); }
bool pad_setup_bt() { return bt_setup_attivo(); }

void pad_avvia_abbinamento() { bt_start_pairing(); }
bool pad_finestra_abbinamento() { return bt_pairing_window_open(); }
void pad_spegni(uint8_t posto) {
    if (bt_slot_ds4(posto)) bt_disconnetti_posto(posto);   // il DualShock 4 si spegne da solo, scollegato
    else bt_slot_power_off(posto);
}
void pad_spegni_tutti() { bt_dualsense_power_off(); }

void pad_info(uint8_t posto, PadPerEventi &out) {
    out = PadPerEventi{};
    if (posto >= BT_MAX_SLOTS) return;
    BtStatus st{};
    bt_get_status(posto, &st);
    out.connesso = st.connected;
    if (!st.connected) return;
    out.modello = bt_slot_ds4(posto) ? MODELLO_DS4 : st.is_dse ? MODELLO_EDGE : MODELLO_DUALSENSE;
    out.batteria_valida = st.battery_valid;
    out.batteria = st.battery_pct;
    out.in_carica = st.charging;
    memcpy(out.mac, st.addr, sizeof out.mac);
}

int8_t pad_rssi(uint8_t posto) { return bt_rssi(posto); }

uint16_t pad_report_al_secondo(uint8_t posto) {
    return posto < BT_MAX_SLOTS ? al_secondo[posto] : 0;
}

void pad_task(uint32_t ora_ms) {
    for (uint8_t i = 0; i < BT_MAX_SLOTS; i++) {
        uint32_t t;
        memcpy(&t, &interrupt_in_data[i][OFFSET_TIMESTAMP], sizeof t);
        if (t != ultimo_timestamp[i]) {
            ultimo_timestamp[i] = t;
            conteggio[i]++;
        }
    }
    if (ora_ms - t_secondo >= 1000) {
        t_secondo = ora_ms;
        for (uint8_t i = 0; i < BT_MAX_SLOTS; i++) {
            al_secondo[i] = conteggio[i];
            conteggio[i] = 0;
        }
    }
}
