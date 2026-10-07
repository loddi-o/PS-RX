//
// PS-RX - modalita' Xbox: driver di classe TinyUSB per le interfacce XInput (vedi xbox.h).
//

#include "xbox.h"

#include <cstring>

#include "bt.h"
#include "config.h"
#include "device/usbd_pvt.h"
#include "pico/time.h"
#include "slots.h"
#include "state_mgr.h"
#include "tusb.h"
#include "xinput.h"

extern uint8_t interrupt_in_data[][63];               // main.cpp: ultimo report di ogni posto
extern volatile uint16_t psrx_intervallo_us[];        // main.cpp: polling per posto (0 = nessun limite)
void state_push_slot_to_bt(uint8_t slot);             // main.cpp

volatile bool psrx_xbox_attivo = false;

namespace {

constexpr uint8_t POSTI = BT_MAX_SLOTS;
constexpr uint8_t EP_LEN = 32;

struct Interfaccia {
    bool aperta;
    uint8_t ep_in;
    uint8_t ep_out;
};

Interfaccia itf[POSTI];
CFG_TUSB_MEM_ALIGN uint8_t buf_in[POSTI][EP_LEN];
CFG_TUSB_MEM_ALIGN uint8_t buf_out[POSTI][EP_LEN];
uint8_t mandato[POSTI][XINPUT_REPORT_LEN];
bool forza[POSTI];                // primo report dopo l'apertura: parte anche se "uguale"
uint64_t t_invio[POSTI];
uint8_t vibrazione[POSTI][2];
bool click_back[POSTI];           // da xbox_task(): no se il touchpad del pad fa da mouse

void x_init() {
    memset(itf, 0, sizeof itf);
}

bool x_deinit() {
    return true;
}

void x_reset(uint8_t) {
    memset(itf, 0, sizeof itf);
    psrx_xbox_attivo = false;
}

uint16_t x_open(uint8_t rhport, tusb_desc_interface_t const *d, uint16_t max_len) {
    if (d->bInterfaceClass != TUSB_CLASS_VENDOR_SPECIFIC || d->bInterfaceSubClass != 0x5D ||
        d->bInterfaceProtocol != 0x01) {
        return 0;
    }
    const uint8_t posto = d->bInterfaceNumber;   // le interfacce XInput sono le prime, una per posto
    if (posto >= POSTI || d->bNumEndpoints != 2) return 0;
    uint16_t len = sizeof(tusb_desc_interface_t);
    uint8_t const *p = tu_desc_next(d);
    while (len < max_len && tu_desc_type(p) != TUSB_DESC_ENDPOINT) {   // salta il descrittore 0x21
        len += tu_desc_len(p);
        p = tu_desc_next(p);
    }
    if (len + 2 * sizeof(tusb_desc_endpoint_t) > max_len) return 0;
    uint8_t ep_out = 0, ep_in = 0;
    if (!usbd_open_edpt_pair(rhport, p, 2, TUSB_XFER_INTERRUPT, &ep_out, &ep_in)) return 0;
    len += 2 * sizeof(tusb_desc_endpoint_t);
    itf[posto] = {true, ep_in, ep_out};
    forza[posto] = true;
    vibrazione[posto][0] = vibrazione[posto][1] = 0;
    usbd_edpt_xfer(rhport, ep_out, buf_out[posto], EP_LEN, false);
    psrx_xbox_attivo = true;
    return len;
}

// Richieste di controllo verso le interfacce XInput: nessuna serve (xusb22 e xpad funzionano senza);
// lo stallo e' una risposta immediata.
bool x_control(uint8_t, uint8_t, tusb_control_request_t const *) {
    return false;
}

bool x_xfer(uint8_t rhport, uint8_t ep, xfer_result_t risultato, uint32_t n) {
    for (uint8_t posto = 0; posto < POSTI; posto++) {
        if (!itf[posto].aperta || ep != itf[posto].ep_out) continue;
        uint8_t forte, debole;
        if (risultato == XFER_RESULT_SUCCESS && xinput_vibrazione(buf_out[posto], n, &forte, &debole) &&
            (forte != vibrazione[posto][0] || debole != vibrazione[posto][1])) {
            vibrazione[posto][0] = forte;
            vibrazione[posto][1] = debole;
            BtStatus st{};
            bt_get_status(posto, &st);
            state_imposta_vibrazione(posto, forte, debole);
            if (st.connected) state_push_slot_to_bt(posto);
        }
        usbd_edpt_xfer(rhport, ep, buf_out[posto], EP_LEN, false);
        return true;
    }
    return true;   // report IN consegnato: niente da fare
}

const usbd_class_driver_t driver_xinput = {
    "PSRX-XINPUT", x_init, x_deinit, x_reset, x_open, x_control, x_xfer, nullptr, nullptr,
};

} // namespace

// TinyUSB: driver dell'applicazione, provati prima di quelli interni (HID, audio, vendor). Il nostro
// prende solo le interfacce FF/5D/01, quindi in modalita' PlayStation non cambia nulla.
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *quanti) {
    *quanti = 1;
    return &driver_xinput;
}

void xbox_descrittore_interfaccia(uint8_t *dest, uint8_t posto) {
    const uint8_t ep_in = static_cast<uint8_t>(0x81 + posto);
    const uint8_t ep_out = static_cast<uint8_t>(0x01 + posto);
    const uint8_t blocco[XBOX_ITF_LEN] = {
        0x09, 0x04, posto, 0x00, 0x02, 0xFF, 0x5D, 0x01, 0x00,                // XInput, 2 endpoint
        0x11, 0x21, 0x00, 0x01, 0x01, 0x25, ep_in, 0x14, 0x00, 0x00, 0x00, 0x00, 0x13, ep_out,
        0x08, 0x00, 0x00,                                                      // descrittore 0x21 del 360
        0x07, 0x05, ep_in, 0x03, EP_LEN, 0x00, 0x01,                           // IN, 1 ms
        0x07, 0x05, ep_out, 0x03, EP_LEN, 0x00, 0x08,                          // OUT (vibrazione), 8 ms
    };
    memcpy(dest, blocco, sizeof blocco);
}

void xbox_invia() {
    if (!tud_ready()) return;
    const uint64_t ora = time_us_64();
    for (uint8_t posto = 0; posto < POSTI; posto++) {
        if (!itf[posto].aperta) continue;
        uint8_t r[XINPUT_REPORT_LEN];
        // interrupt_in_data la scrive solo questo core (on_bt_data in bt_pump): niente blocchi.
        xinput_da_dualsense(interrupt_in_data[posto], r, click_back[posto]);
        if (!forza[posto] && memcmp(r, mandato[posto], sizeof r) == 0) continue;
        const uint16_t intervallo = psrx_intervallo_us[posto];
        if (intervallo && ora - t_invio[posto] < intervallo) continue;
        const uint8_t ep = itf[posto].ep_in;
        if (!usbd_edpt_claim(0, ep)) continue;   // report precedente non ancora letto dal PC
        memcpy(buf_in[posto], r, sizeof r);
        if (usbd_edpt_xfer(0, ep, buf_in[posto], sizeof r, false)) {
            memcpy(mandato[posto], r, sizeof r);
            forza[posto] = false;
            t_invio[posto] = ora;
        } else {
            usbd_edpt_release(0, ep);
        }
    }
}

void xbox_task() {
    if (!psrx_xbox_attivo) return;
    for (uint8_t posto = 0; posto < POSTI; posto++) {
        BtStatus st{};
        bt_get_status(posto, &st);
        const PadImpostazioni *imp = st.connected ? config_pad(st.addr) : nullptr;
        click_back[posto] = !(imp && imp->trackpad);
    }
}
