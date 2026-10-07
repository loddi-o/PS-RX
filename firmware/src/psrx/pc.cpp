//
// PS-RX - stato del PC dal bus USB (vedi pc.h).
//

#include "pc.h"

#include "bt.h"
#include "config.h"
#include "log_psrx.h"
#include "pad.h"
#include "tusb.h"
#include "usb.h"
#include "wake.h"

namespace {

constexpr uint32_t PERIODO_MS = 100;
// Lo stato nuovo deve durare tanto prima di agire. Sospeso: 10 s come DS5-Linux-Bridge, perche' a PC acceso
// Windows puo' sospendere per poco un dispositivo USB inattivo. Spento: 3 s (una sospensione selettiva non
// toglie la configurazione).
constexpr uint32_t CONFERMA_SOSPESO_MS = 10000;
constexpr uint32_t CONFERMA_MS = 3000;

StatoPc confermato = PC_SCONOSCIUTO;
StatoPc candidato = PC_SCONOSCIUTO;
uint32_t t_candidato = 0;
uint32_t t_controllo = 0;

const char *nome(StatoPc s) {
    switch (s) {
        case PC_ACCESO:  return "acceso";
        case PC_SOSPESO: return "sospeso";
        case PC_SPENTO:  return "spento";
        default:         return "sconosciuto";
    }
}

StatoPc leggi() {
    if (!tud_mounted()) return PC_SPENTO;
    return usb_host_suspended() ? PC_SOSPESO : PC_ACCESO;
}

} // namespace

void pc_task(uint32_t ora) {
    if (ora - t_controllo < PERIODO_MS) return;
    t_controllo = ora;
    if (usb_variant_swap_in_progress()) {   // ricollegamento voluto: non e' il PC che cambia stato
        t_candidato = ora;
        return;
    }
    const StatoPc s = leggi();
    if (s != candidato) {
        candidato = s;
        t_candidato = ora;
        return;
    }
    const uint32_t attesa = candidato == PC_SOSPESO ? CONFERMA_SOSPESO_MS : CONFERMA_MS;
    if (candidato == confermato || ora - t_candidato < attesa) return;
    const StatoPc prima = confermato;
    confermato = candidato;
    psrx_log("PC %s", nome(confermato));
    if (prima == PC_ACCESO && confermato != PC_ACCESO && !get_config().psrx_pad_accesi_con_pc_spento &&
        pad_connessi() > 0) {
        psrx_log("PC %s: spengo i controller", nome(confermato));
        for (uint8_t k = 0; k < BT_MAX_SLOTS; k++) {
            BtStatus st{};
            bt_get_status(k, &st);
            if (st.connected) pad_spegni(k);
        }
    }
}

StatoPc pc_stato() {
    return confermato;
}

bool pc_serve_wol() {
    // Acceso (confermato): inutile, anche durante un ricollegamento USB voluto. Appena svegliato (stato
    // immediato acceso, per esempio dal risveglio via USB): inutile, la finestra del WoL si chiude subito.
    // Spento, sospeso, o stato non ancora noto: WoL (dopo pc_ritardo_wol_ms se si puo' svegliare via USB).
    if (confermato == PC_ACCESO) return false;
    return usb_variant_swap_in_progress() || leggi() != PC_ACCESO;
}

uint32_t pc_ritardo_wol_ms() {
    return wake_usb_possibile() ? 3000 : 0;
}
