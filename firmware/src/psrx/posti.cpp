//
// PS-RX - colore secondo il posto e combinazione di spegnimento (vedi posti.h).
//

#include "posti.h"

#include "bt.h"
#include "combo.h"
#include "config.h"
#include "log_psrx.h"
#include "slots.h"
#include "state_mgr.h"

extern uint8_t interrupt_in_data[][63];
extern volatile uint16_t psrx_intervallo_us[];   // main.cpp: frequenza verso il PC per posto
extern volatile bool psrx_posti_ridotti;
void state_push_slot_to_bt(uint8_t slot);   // main.cpp

namespace {
constexpr uint32_t T_LETTURA_MS = 10;
// Colori dei posti come la PS5: blu, rosso, verde, rosa.
constexpr uint8_t COLORI[4][3] = {{0, 0, 255}, {255, 0, 0}, {0, 255, 0}, {255, 0, 128}};
// Impostazione PAD_POLLING -> intervallo minimo fra due report verso il PC (0 = appena arriva).
constexpr uint16_t INTERVALLO_US[4] = {0, 2000, 4000, 8000};

ComboSpegni combo[BT_MAX_SLOTS];
bool colore_dato[BT_MAX_SLOTS] = {};
bool led_prima = false;
uint32_t t_lettura = 0;
}

void posti_task(uint32_t ora) {
    if (ora - t_lettura < T_LETTURA_MS) return;
    t_lettura = ora;

    const bool led = get_config().psrx_led_posto != 0;
    psrx_led_posto = led;
    if (led != led_prima) {
        led_prima = led;
        for (auto &c : colore_dato) c = false;
    }

    bool ridotti = false;
    for (uint8_t posto = 0; posto < BT_MAX_SLOTS; posto++) {
        BtStatus st{};
        bt_get_status(posto, &st);
        if (!st.connected) {
            combo[posto].azzera();
            colore_dato[posto] = false;
            psrx_intervallo_us[posto] = 0;
            continue;
        }
        const PadImpostazioni *imp = config_pad(st.addr);
        const uint16_t intervallo = INTERVALLO_US[imp ? imp->polling & 3 : 0];
        psrx_intervallo_us[posto] = intervallo;
        ridotti = ridotti || intervallo != 0;
        if (led && !colore_dato[posto]) {
            const uint8_t *c = COLORI[posto % 4];
            state_imposta_colore(posto, c[0], c[1], c[2]);
            state_push_slot_to_bt(posto);
            colore_dato[posto] = true;
        }
        if (combo[posto].aggiorna(interrupt_in_data[posto][8])) {
            psrx_log("combinazione di spegnimento sul controller del posto %u", posto + 1);
            if (bt_slot_ds4(posto)) bt_disconnetti_posto(posto);   // il DualShock 4 si spegne da solo
            else bt_slot_power_off(posto);
        }
    }
    psrx_posti_ridotti = ridotti;
}
