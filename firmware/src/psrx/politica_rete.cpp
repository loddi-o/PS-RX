//
// PS-RX - quando tenere acceso il WiFi e quando mandare il Wake-on-LAN (vedi politica_rete.h).
//

#include "politica_rete.h"

DecisioneRete PoliticaRete::aggiorna(uint32_t ora, int pad, bool link_pronto, bool wol_attivo, bool ha_reti,
                                     uint32_t ritardo_ms) {
    DecisioneRete d{false, false};

    if (!avviata_) {
        // All'avvio senza controller il WiFi parte subito (niente attesa di riaccensione).
        avviata_ = true;
        pad_prima_ = pad;
        t_senza_pad_ = ora - rete::T_RIACCENSIONE_MS;
    }

    if (pad > 0 && pad_prima_ == 0) {
        // Il primo controller si e' appena collegato.
        if (wol_attivo && ha_reti) {
            in_finestra_ = true;
            t_finestra_ = ora;
            inviati_ = 0;
            ritardo_ = ritardo_ms;
        }
    }
    if (pad == 0 && pad_prima_ > 0) {
        t_senza_pad_ = ora;
        in_finestra_ = false;   // un controller si e' collegato e scollegato prima del WoL: inutile
    }
    pad_prima_ = pad;

    if (in_finestra_) {
        if (ora - t_finestra_ >= rete::T_FINESTRA_WOL_MS || !wol_attivo) {
            in_finestra_ = false;   // tempo scaduto, o il WoL non serve piu' (il PC si e' svegliato)
        } else if (ora - t_finestra_ < ritardo_) {
            // si aspetta il risveglio via USB prima del WoL di riserva
        } else if (link_pronto && (inviati_ == 0 || ora - t_ultimo_ >= rete::T_TRA_PACCHETTI_MS)) {
            d.invia_wol = true;
            inviati_++;
            t_ultimo_ = ora;
            if (inviati_ >= rete::N_PACCHETTI_WOL) in_finestra_ = false;
        }
    }

    if (!ha_reti) {
        acceso_ = false;
    } else if (pad == 0) {
        acceso_ = acceso_ || ora - t_senza_pad_ >= rete::T_RIACCENSIONE_MS;
    } else {
        // Con un controller collegato resta acceso solo per la finestra del WoL; l'ultimo pacchetto
        // parte in questo giro, la radio si spegne al prossimo.
        acceso_ = in_finestra_ || d.invia_wol;
    }
    d.wifi_acceso = acceso_;
    return d;
}
