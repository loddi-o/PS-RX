//
// PS-RX - decisioni su flash, salvataggio e aggiornamento (vedi politiche.h).
//

#include "politiche.h"

#include "psrx_config.h"

bool flash_libera(int pad_connessi, bool setup_bt) {
    return pad_connessi == 0 && !setup_bt;
}

bool deve_salvare(bool sporco, bool forzato, int pad_connessi, bool setup_bt, uint32_t ms_da_modifica,
                  uint32_t ms_da_fallimento) {
    if (!sporco && !forzato) return false;
    if (!flash_libera(pad_connessi, setup_bt)) return false;
    if (ms_da_fallimento < T_SALVATAGGIO_RIPROVA_MS) return false;
    return forzato || ms_da_modifica >= T_SALVATAGGIO_ATTESA_MS;
}
