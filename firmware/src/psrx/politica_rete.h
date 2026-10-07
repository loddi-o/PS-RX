//
// PS-RX - quando tenere acceso il WiFi e quando mandare il Wake-on-LAN (logica pura, testata sul PC).
//
// - Nessun controller collegato: WiFi acceso (connesso a una rete salvata), pronto per il WoL.
//   Dopo che l'ultimo controller si scollega si aspetta T_RIACCENSIONE_MS, cosi' una riconnessione
//   rapida non fa ripartire il WiFi per niente.
// - Finestra di risveglio aperta (risveglio.h: primo controller collegato, durata dall'app): se il WoL e'
//   configurato il WiFi resta acceso e, appena connesso, manda giri di N_PACCHETTI_WOL pacchetti magici
//   (a T_TRA_PACCHETTI_MS) ogni T_TRA_GIRI_MS, finche' la finestra resta aperta.
// - Con almeno un controller collegato, fuori dalla finestra: WiFi spento (radio tutta al Bluetooth).
//

#ifndef PSRX_POLITICA_RETE_H
#define PSRX_POLITICA_RETE_H

#include <cstdint>

namespace rete {
constexpr uint32_t T_RIACCENSIONE_MS = 2000;
constexpr uint32_t T_TRA_PACCHETTI_MS = 300;
constexpr uint32_t T_TRA_GIRI_MS = 5000;
constexpr uint8_t N_PACCHETTI_WOL = 3;
}

struct DecisioneRete {
    bool wifi_acceso;   // la radio WiFi deve essere accesa (e connessa a una rete salvata)
    bool invia_wol;     // manda adesso un pacchetto magico (a tutte le destinazioni)
};

class PoliticaRete {
public:
    // ora:            millisecondi dall'avvio
    // pad:            controller collegati
    // link_pronto:    WiFi connesso con indirizzo IP
    // wol_attivo:     Wake-on-LAN configurato (almeno un MAC) e non disattivato
    // ha_reti:        almeno una rete salvata
    // finestra:       finestra di risveglio aperta (risveglio.h)
    DecisioneRete aggiorna(uint32_t ora, int pad, bool link_pronto, bool wol_attivo, bool ha_reti, bool finestra);

    bool finestra_wol() const { return finestra_; }
    uint16_t pacchetti_inviati() const { return inviati_; }

private:
    bool avviata_ = false;
    int pad_prima_ = 0;
    bool finestra_ = false;
    uint32_t t_giro_ = 0;          // inizio dell'ultimo giro di pacchetti
    uint32_t t_ultimo_ = 0;
    uint8_t nel_giro_ = 0;         // pacchetti gia' mandati nel giro in corso
    bool giro_fatto_ = false;      // almeno un giro iniziato in questa finestra
    uint16_t inviati_ = 0;
    uint32_t t_senza_pad_ = 0;     // da quando non ci sono controller
    bool acceso_ = false;
};

#endif // PSRX_POLITICA_RETE_H
