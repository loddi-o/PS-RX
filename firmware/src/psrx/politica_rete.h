//
// PS-RX - quando tenere acceso il WiFi e quando mandare il Wake-on-LAN (logica pura, testata sul PC).
//
// - Nessun controller collegato: WiFi acceso (connesso a una rete salvata), pronto per il WoL.
//   Dopo che l'ultimo controller si scollega si aspetta T_RIACCENSIONE_MS, cosi' una riconnessione
//   rapida non fa ripartire il WiFi per niente.
// - Si collega il primo controller: se il WoL e' configurato si apre una finestra di al massimo
//   T_FINESTRA_WOL_MS. Appena il WiFi e' connesso (indirizzo IP preso) partono N_PACCHETTI_WOL
//   pacchetti magici a T_TRA_PACCHETTI_MS di distanza, poi il WiFi si spegne. Senza WoL configurato
//   il WiFi si spegne subito.
// - Con almeno un controller collegato, fuori dalla finestra: WiFi spento (radio tutta al Bluetooth).
//

#ifndef PSRX_POLITICA_RETE_H
#define PSRX_POLITICA_RETE_H

#include <cstdint>

namespace rete {
constexpr uint32_t T_RIACCENSIONE_MS = 2000;
constexpr uint32_t T_FINESTRA_WOL_MS = 30000;
constexpr uint32_t T_TRA_PACCHETTI_MS = 300;
constexpr uint8_t N_PACCHETTI_WOL = 3;
}

struct DecisioneRete {
    bool wifi_acceso;   // la radio WiFi deve essere accesa (e connessa a una rete salvata)
    bool invia_wol;     // manda adesso un giro di pacchetti magici
};

class PoliticaRete {
public:
    // ora:            millisecondi dall'avvio
    // pad:            controller collegati
    // link_pronto:    WiFi connesso con indirizzo IP
    // wol_attivo:     Wake-on-LAN configurato (almeno un MAC) e non disattivato
    // ha_reti:        almeno una rete salvata
    DecisioneRete aggiorna(uint32_t ora, int pad, bool link_pronto, bool wol_attivo, bool ha_reti);

    bool finestra_wol() const { return in_finestra_; }
    uint8_t pacchetti_inviati() const { return inviati_; }

private:
    bool avviata_ = false;
    int pad_prima_ = 0;
    bool in_finestra_ = false;
    uint32_t t_finestra_ = 0;
    uint32_t t_ultimo_ = 0;
    uint8_t inviati_ = 0;
    uint32_t t_senza_pad_ = 0;     // da quando non ci sono controller
    bool acceso_ = false;
};

#endif // PSRX_POLITICA_RETE_H
