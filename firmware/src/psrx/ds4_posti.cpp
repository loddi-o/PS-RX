//
// PS-RX - DualShock 4 nei posti del ricevitore (vedi ds4_posti.h).
//

#include "ds4_posti.h"

#include <cstring>
#include <vector>

#include "bt.h"
#include "config.h"
#include "ds4.h"
#include "log_psrx.h"
#include "protocollo.h"
#include "salvataggio.h"
#include "slots.h"
#include "wake.h"

// main.cpp: ultimo report di ogni posto (formato USB del DualSense) e coda verso l'USB.
extern uint8_t interrupt_in_data[][63];
void realtime_hid_queue_push(uint8_t slot, const uint8_t *report);

namespace {
enum Fase : uint8_t { LIBERO, IN_ATTESA, DUALSHOCK4, DUALSENSE };

StatoDs4 traduzione[BT_MAX_SLOTS];
Fase fase[BT_MAX_SLOTS] = {};
uint32_t t_collegamento[BT_MAX_SLOTS] = {};
bool calibrazione_fatta[BT_MAX_SLOTS] = {};

constexpr uint32_t T_RICONOSCIMENTO_MS = 3000;   // senza 0xA3 entro 3 s: e' un DualSense

void ricorda_modello(const uint8_t *mac, uint8_t modello) {
    const PadImpostazioni *p = config_pad(mac);
    if (p && p->modello == modello) return;
    PadImpostazioni *q = config_pad_scrivibile(mac);
    if (!q) return;
    q->modello = modello;
    salvataggio_segna_modifica();
}

void diventa_ds4(uint8_t posto, const uint8_t *mac) {
    fase[posto] = DUALSHOCK4;
    calibrazione_fatta[posto] = false;
    // Report che l'host legge all'avvio del DualSense (senza, hid-playstation non si aggancia).
    uint8_t f09[DS5_LUNGHEZZA_FEATURE_09];
    uint8_t f20[DS5_LUNGHEZZA_FEATURE_20];
    ds4_feature_09(mac, f09);
    ds4_feature_20(f20);
    bt_feature_slot_imposta(posto, 0x09, f09, sizeof f09);
    bt_feature_slot_imposta(posto, 0x20, f20, sizeof f20);
    bt_slot_segna_ds4(posto);
    ricorda_modello(mac, MODELLO_DS4);
    psrx_log("DualShock 4 nel posto %u: si presenta all'host come DualSense", posto + 1);
}
} // namespace

void __not_in_flash_func(psrx_ds4_ingresso)(uint8_t posto, const uint8_t *dati, uint16_t lunghezza) {
    if (posto >= BT_MAX_SLOTS) return;
    uint8_t report[DS5_LUNGHEZZA_REPORT];
    if (!ds4_in_dualsense(dati, lunghezza, interrupt_in_data[posto], traduzione[posto], report)) return;
    wake_on_bt_input(report, DS5_LUNGHEZZA_REPORT);
    realtime_hid_queue_push(posto, report);
}

void ds4_posti_task(uint32_t ora) {
    for (uint8_t posto = 0; posto < BT_MAX_SLOTS; posto++) {
        BtStatus st{};
        bt_get_status(posto, &st);
        if (!st.connected) {
            fase[posto] = LIBERO;
            continue;
        }
        if (fase[posto] == LIBERO) {
            fase[posto] = IN_ATTESA;
            t_collegamento[posto] = ora;
            traduzione[posto] = StatoDs4{};
            const PadImpostazioni *p = config_pad(st.addr);
            if (p && p->modello == MODELLO_DS4) diventa_ds4(posto, st.addr);   // gia' visto: subito
        }
        if (fase[posto] == IN_ATTESA) {
            std::vector<uint8_t> v;
            if (bt_feature_slot(posto, DS4_FEATURE_FIRMWARE, v)) {
                diventa_ds4(posto, st.addr);
            } else if (ora - t_collegamento[posto] >= T_RICONOSCIMENTO_MS) {
                fase[posto] = DUALSENSE;
                ricorda_modello(st.addr, st.is_dse ? MODELLO_EDGE : MODELLO_DUALSENSE);
            }
        }
        if (fase[posto] == DUALSHOCK4 && !calibrazione_fatta[posto]) {
            std::vector<uint8_t> v;
            if (bt_feature_slot(posto, 0x05, v)) {
                ds4_calibrazione_in_dualsense(v.data(), static_cast<uint16_t>(v.size()));
                bt_feature_slot_imposta(posto, 0x05, v.data(), static_cast<uint16_t>(v.size()));
                calibrazione_fatta[posto] = true;
            }
        }
    }
}
