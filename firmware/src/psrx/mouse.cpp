//
// PS-RX - mouse del touchpad sull'USB (vedi mouse.h).
//

#include "mouse.h"

#include <cstring>

#include "bt.h"
#include "config.h"
#include "log_psrx.h"
#include "slots.h"
#include "trackpad.h"
#include "tusb.h"
#include "usb.h"

extern uint8_t interrupt_in_data[][63];

namespace {
TrackpadMouse trackpad[BT_MAX_SLOTS];
uint32_t ultimo_timestamp[BT_MAX_SLOTS] = {};
int32_t da_mandare_x = 0, da_mandare_y = 0, da_mandare_rotella = 0;
uint8_t tasti = 0, tasti_mandati = 0;
bool mouse_voluto = false;
bool primo = true;

int8_t taglia(int32_t &v) {
    const int32_t t = v > 127 ? 127 : v < -127 ? -127 : v;
    v -= t;
    return static_cast<int8_t>(t);
}
}

void mouse_task() {
    // L'interfaccia mouse c'e' se almeno un controller abbinato ha l'opzione attiva.
    bool voluto = false;
    for (const auto &p : get_config().psrx_pad) voluto = voluto || p.trackpad;
    if (voluto != mouse_voluto || primo) {
        primo = false;
        mouse_voluto = voluto;
        usb_request_mouse(voluto);
    }

    const uint8_t inst = usb_mouse_hid_instance();
    if (inst == 0xFF) return;

    uint8_t tasti_ora = 0;
    for (uint8_t posto = 0; posto < BT_MAX_SLOTS; posto++) {
        BtStatus st{};
        bt_get_status(posto, &st);
        if (!st.connected) continue;
        const PadImpostazioni *imp = config_pad(st.addr);
        if (!imp || !imp->trackpad) continue;
        const uint8_t *r = interrupt_in_data[posto];
        uint32_t ts;
        memcpy(&ts, &r[27], sizeof ts);
        if (ts == ultimo_timestamp[posto]) {
            if (r[9] & 0x02) tasti_ora |= tasti & 0x03;   // click tenuto fra un report e l'altro
            continue;
        }
        ultimo_timestamp[posto] = ts;
        const MovimentoMouse m = trackpad[posto].aggiorna(touchpad_da_report(r), imp->inverti_scorrimento);
        da_mandare_x += m.dx;
        da_mandare_y += m.dy;
        da_mandare_rotella += m.rotella;
        tasti_ora |= m.tasti;
    }
    tasti = tasti_ora;

    if (!da_mandare_x && !da_mandare_y && !da_mandare_rotella && tasti == tasti_mandati) return;
    if (!tud_hid_n_ready(inst)) return;
    const int8_t report[4] = {static_cast<int8_t>(tasti), taglia(da_mandare_x), taglia(da_mandare_y),
                              taglia(da_mandare_rotella)};
    if (tud_hid_n_report(inst, 0, report, sizeof report)) tasti_mandati = tasti;
}
