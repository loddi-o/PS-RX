//
// PS-RX - finestra di risveglio del PC (vedi risveglio.h).
//

#include "risveglio.h"

DecisioneRisveglio FinestraRisveglio::aggiorna(uint32_t ora, int pad, bool pc_attivo, bool in_swap, uint8_t durata_s) {
    DecisioneRisveglio d{false, false};
    if (pad > 0 && pad_prima_ == 0 && durata_s > 0) {   // primo controller collegato
        attiva_ = true;
        t_inizio_ = ora;
        t_usb_ = ora - risveglio::T_USB_MS;              // primo segnale USB subito
        visto_non_attivo_ = false;
    }
    pad_prima_ = pad;
    if (!attiva_) return d;

    if (pad == 0 || ora - t_inizio_ >= static_cast<uint32_t>(durata_s) * 1000u) {
        attiva_ = false;   // controller spenti o tempo scaduto
        return d;
    }
    if (!in_swap) {
        if (!pc_attivo) {
            visto_non_attivo_ = true;
        } else if (visto_non_attivo_) {
            attiva_ = false;   // il PC si e' svegliato
            return d;
        }
    }
    d.attiva = true;
    if (ora - t_usb_ >= risveglio::T_USB_MS) {
        d.segnale_usb = true;
        t_usb_ = ora;
    }
    return d;
}
