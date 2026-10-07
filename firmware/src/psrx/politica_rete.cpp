//
// PS-RX - quando tenere acceso il WiFi e quando mandare il Wake-on-LAN (vedi politica_rete.h).
//

#include "politica_rete.h"

DecisioneRete PoliticaRete::aggiorna(uint32_t ora, int pad, bool link_pronto, bool wol_attivo, bool ha_reti,
                                     bool finestra) {
    DecisioneRete d{false, false};

    if (!avviata_) {
        // All'avvio senza controller il WiFi parte subito (niente attesa di riaccensione).
        avviata_ = true;
        pad_prima_ = pad;
        t_senza_pad_ = ora - rete::T_RIACCENSIONE_MS;
    }
    if (pad == 0 && pad_prima_ > 0) t_senza_pad_ = ora;
    pad_prima_ = pad;

    const bool wol = finestra && wol_attivo && ha_reti && pad > 0;
    if (wol && !finestra_) {   // finestra appena aperta
        giro_fatto_ = false;
        nel_giro_ = 0;
    }
    finestra_ = wol;

    if (wol && link_pronto) {
        if (!giro_fatto_ || (nel_giro_ >= rete::N_PACCHETTI_WOL && ora - t_giro_ >= rete::T_TRA_GIRI_MS)) {
            giro_fatto_ = true;   // nuovo giro
            t_giro_ = ora;
            nel_giro_ = 0;
        }
        if (nel_giro_ < rete::N_PACCHETTI_WOL && (nel_giro_ == 0 || ora - t_ultimo_ >= rete::T_TRA_PACCHETTI_MS)) {
            d.invia_wol = true;
            nel_giro_++;
            inviati_++;
            t_ultimo_ = ora;
        }
    }

    if (!ha_reti) {
        acceso_ = false;
    } else if (pad == 0) {
        acceso_ = acceso_ || ora - t_senza_pad_ >= rete::T_RIACCENSIONE_MS;
    } else {
        acceso_ = wol;   // con un controller: acceso solo durante la finestra di risveglio
    }
    d.wifi_acceso = acceso_;
    return d;
}
