//
// Conta i click di un tasto letto a intervalli regolari (logica pura).
//

#include "conta_click.h"
#include "psrx_config.h"

void ContaClick::azzera() {
    premuto_ = false;
    annullata_ = false;
    click_ = 0;
}

uint8_t ContaClick::aggiorna(bool premuto, uint32_t ora_ms) {
    if (premuto && !premuto_) {
        premuto_ = true;
        t_pressione_ = ora_ms;
        return 0;
    }
    if (!premuto && premuto_) {
        premuto_ = false;
        t_rilascio_ = ora_ms;
        if (ora_ms - t_pressione_ > T_CLICK_MAX_MS) {
            annullata_ = true;          // pressione lunga: la serie non vale
        } else if (click_ < 9) {
            click_++;
        }
        return 0;
    }
    if (!premuto && (click_ > 0 || annullata_) && ora_ms - t_rilascio_ > T_BOOTSEL_SERIE_MS) {
        const uint8_t n = annullata_ ? 0 : click_;
        click_ = 0;
        annullata_ = false;
        return n;
    }
    return 0;
}
