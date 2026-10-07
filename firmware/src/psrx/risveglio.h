//
// PS-RX - finestra di risveglio del PC (logica pura, testata sul PC).
//
// Quando si collega il primo controller (da 0 controller collegati), per "durata" secondi (impostazione
// dell'app, predefinita 20 s, 0 = disattivata) il ricevitore prova a svegliare il PC in tutti i modi:
//   - USB: un segnale di risveglio (remote wakeup) ogni T_USB_MS (se il bus e' attivo non succede nulla);
//   - Wake-on-LAN: lo manda politica_rete.h finche' la finestra e' aperta;
//   - il LED lampeggia.
// La finestra si chiude prima se il PC si sveglia davvero: il bus USB, visto non attivo durante la finestra,
// torna attivo (mai contando i ricollegamenti USB voluti, il cambio di forma). Il gamepad funziona normalmente
// per tutto il tempo.
//

#ifndef PSRX_RISVEGLIO_H
#define PSRX_RISVEGLIO_H

#include <cstdint>

namespace risveglio {
constexpr uint32_t T_USB_MS = 2000;
constexpr uint8_t DURATA_PREDEFINITA_S = 20;
}

struct DecisioneRisveglio {
    bool attiva;      // finestra aperta: WoL e lampeggio
    bool segnale_usb; // manda adesso un segnale di risveglio USB
};

class FinestraRisveglio {
public:
    // pad:        controller collegati
    // pc_attivo:  bus USB attivo adesso (configurato e non sospeso)
    // in_swap:    ricollegamento USB voluto in corso (cambio di forma): lo stato del bus non conta
    // durata_s:   durata della finestra (0 = funzione disattivata)
    DecisioneRisveglio aggiorna(uint32_t ora, int pad, bool pc_attivo, bool in_swap, uint8_t durata_s);
    bool attiva() const { return attiva_; }

private:
    int pad_prima_ = 0;
    bool attiva_ = false;
    uint32_t t_inizio_ = 0;
    uint32_t t_usb_ = 0;
    bool visto_non_attivo_ = false;
};

#endif // PSRX_RISVEGLIO_H
