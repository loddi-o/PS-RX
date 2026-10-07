//
// PS-RX - eventi per le notifiche dell'app (logica pura, testata sul PC).
//
// Il ciclo principale passa ogni ~20 ms lo stato dei controller; da li' nascono gli eventi:
// - COLLEGATO: appena la batteria e' nota (al massimo T_ATTESA_BATTERIA_MS dopo il collegamento),
//   con posto, modello, batteria e modalita' del ricevitore;
// - SCOLLEGATO;
// - BATTERIA: una volta al 20% e una al 10% (critica); si riarmano sopra il 40% o in carica.
// Gli eventi stanno in un anello in RAM; l'app li legge per numero (CMD_EVENTI) e non ne perde
// finche' non ne arrivano piu' di N_EVENTI fra due letture.
//

#ifndef PSRX_EVENTI_H
#define PSRX_EVENTI_H

#include <cstdint>

#include "protocollo.h"

struct PadPerEventi {
    bool connesso;
    uint8_t modello;          // ModelloPad
    bool batteria_valida;
    uint8_t batteria;         // 0..100
    bool in_carica;
    uint8_t mac[6];
};

class Eventi {
public:
    static constexpr uint8_t N_EVENTI = 16;

    void aggiorna(uint32_t ora, const PadPerEventi pad[PSRX_PAD_MAX], uint8_t modalita);

    // Copia in 'out' gli eventi con numero successivo a 'dopo' (al massimo 'max', i piu' vecchi
    // prima) e restituisce quanti sono.
    uint8_t leggi(uint16_t dopo, EventoPsrx *out, uint8_t max) const;
    uint16_t ultimo() const { return numero_; }

private:
    struct StatoPosto {
        bool connesso = false;
        bool annunciato = false;   // COLLEGATO gia' emesso
        uint32_t t_collegamento = 0;
        bool avvisato_basso = false;
        bool avvisato_critico = false;
    };

    void emetti(uint8_t tipo, uint8_t posto, const PadPerEventi &p, uint8_t flag, uint8_t modalita);

    StatoPosto posti_[PSRX_PAD_MAX];
    EventoPsrx anello_[N_EVENTI] = {};
    uint8_t quanti_ = 0;
    uint16_t numero_ = 0;
};

#endif // PSRX_EVENTI_H
