//
// PS-RX - eventi per le notifiche dell'app (vedi eventi.h).
//

#include "eventi.h"

#include <cstring>

#include "psrx_config.h"

void Eventi::emetti(uint8_t tipo, uint8_t posto, const PadPerEventi &p, uint8_t flag, uint8_t modalita) {
    EventoPsrx e{};
    e.numero = ++numero_;
    if (e.numero == 0) e.numero = ++numero_;   // 0 vuol dire "nessun evento letto"
    e.tipo = tipo;
    e.posto = posto;
    e.modello = p.modello;
    e.batteria = p.batteria_valida ? p.batteria : 0xFF;
    e.flag = flag;
    e.modalita = modalita;
    memcpy(e.mac, p.mac, sizeof e.mac);
    anello_[e.numero % N_EVENTI] = e;
    if (quanti_ < N_EVENTI) quanti_++;
}

void Eventi::aggiorna(uint32_t ora, const PadPerEventi pad[PSRX_PAD_MAX], uint8_t modalita) {
    for (uint8_t i = 0; i < PSRX_PAD_MAX; i++) {
        StatoPosto &s = posti_[i];
        const PadPerEventi &p = pad[i];

        if (p.connesso && !s.connesso) {
            s = StatoPosto{};
            s.connesso = true;
            s.t_collegamento = ora;
        } else if (!p.connesso && s.connesso) {
            if (s.annunciato) emetti(EVENTO_SCOLLEGATO, i, p, 0, modalita);
            s = StatoPosto{};
            continue;
        }
        if (!s.connesso) continue;

        if (!s.annunciato) {
            if (!p.batteria_valida && ora - s.t_collegamento < T_ATTESA_BATTERIA_MS) continue;
            emetti(EVENTO_COLLEGATO, i, p, 0, modalita);
            s.annunciato = true;
            // Il livello al collegamento e' gia' nella notifica: niente avviso doppio subito dopo.
            if (p.batteria_valida && !p.in_carica) {
                s.avvisato_basso = p.batteria <= SOGLIA_BATTERIA_BASSA;
                s.avvisato_critico = p.batteria <= SOGLIA_BATTERIA_CRITICA;
            }
            continue;
        }

        if (!p.batteria_valida) continue;
        if (p.in_carica || p.batteria >= BATTERIA_RIARMO) {
            s.avvisato_basso = s.avvisato_critico = false;
            continue;
        }
        if (p.batteria <= SOGLIA_BATTERIA_CRITICA && !s.avvisato_critico) {
            emetti(EVENTO_BATTERIA, i, p, 1, modalita);
            s.avvisato_critico = s.avvisato_basso = true;
        } else if (p.batteria <= SOGLIA_BATTERIA_BASSA && !s.avvisato_basso) {
            emetti(EVENTO_BATTERIA, i, p, 0, modalita);
            s.avvisato_basso = true;
        }
    }
}

uint8_t Eventi::leggi(uint16_t dopo, EventoPsrx *out, uint8_t max) const {
    uint8_t n = 0;
    // Dal piu' vecchio ancora nell'anello al piu' recente.
    for (uint8_t k = quanti_; k > 0 && n < max; k--) {
        const uint16_t numero = static_cast<uint16_t>(numero_ - (k - 1));
        const EventoPsrx &e = anello_[numero % N_EVENTI];
        if (e.numero != numero) continue;   // posto saltato dal passaggio per lo zero
        if (static_cast<int16_t>(numero - dopo) <= 0) continue;
        out[n++] = e;
    }
    return n;
}
