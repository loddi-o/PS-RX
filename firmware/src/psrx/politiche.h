//
// PS-RX - decisioni su flash, salvataggio e aggiornamento (logica pura, testata sul PC).
//
// Regola di stabilita': la flash si scrive solo senza controller collegati e senza un collegamento
// Bluetooth in corso. Una scrittura ferma il Pico per circa 50 ms (core 1 parcheggiato, interrupt
// spenti): con un controller in uso sarebbe un singhiozzo, durante un collegamento potrebbe farlo
// fallire.
//

#ifndef PSRX_POLITICHE_H
#define PSRX_POLITICHE_H

#include <cstdint>

bool flash_libera(int pad_connessi, bool setup_bt);

// Salvataggio differito delle impostazioni cambiate dall'app.
//   sporco:            ci sono modifiche solo in RAM
//   forzato:           l'utente ha chiesto "salva ora" (salta l'attesa)
//   ms_da_modifica:    tempo dall'ultima modifica
//   ms_da_fallimento:  tempo dall'ultima scrittura fallita (0xFFFFFFFF = mai)
bool deve_salvare(bool sporco, bool forzato, int pad_connessi, bool setup_bt, uint32_t ms_da_modifica,
                  uint32_t ms_da_fallimento);

#endif // PSRX_POLITICHE_H
