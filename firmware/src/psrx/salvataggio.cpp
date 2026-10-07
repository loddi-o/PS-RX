//
// PS-RX - salvataggio differito delle impostazioni cambiate dall'app.
//

#include "salvataggio.h"

#include "config.h"
#include "log_psrx.h"
#include "pad.h"
#include "politiche.h"
#include "psrx_config.h"

#include "pico/time.h"

static bool sporco = false;
static bool forzato = false;
static uint32_t t_modifica = 0;
static bool fallito = false;
static uint32_t t_fallimento = 0;

static uint32_t ora_ms() {
    return to_ms_since_boot(get_absolute_time());
}

void salvataggio_segna_modifica() {
    sporco = true;
    t_modifica = ora_ms();
}

void salvataggio_forza() {
    forzato = true;
}

bool salvataggio_in_sospeso() {
    return sporco;
}

void salvataggio_task() {
    if (!sporco && !forzato) return;
    const uint32_t ora = ora_ms();
    const uint32_t da_fallimento = fallito ? ora - t_fallimento : 0xFFFFFFFFu;
    if (!deve_salvare(sporco, forzato, pad_connessi(), pad_setup_bt(), ora - t_modifica, da_fallimento)) return;
    forzato = false;
    if (config_save()) {
        sporco = false;
        fallito = false;
        psrx_log("impostazioni salvate in flash");
    } else {
        fallito = true;
        t_fallimento = ora;
        psrx_log("salvataggio delle impostazioni FALLITO: riprovo fra %lu s",
                 static_cast<unsigned long>(T_SALVATAGGIO_RIPROVA_MS / 1000));
    }
}
