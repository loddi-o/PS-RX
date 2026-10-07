//
// PS-RX - tasto BOOTSEL del Pico, a firmware in funzione (vedi bootsel_gesti.h).
//

#include "bootsel_gesti.h"

#include "aggiornamento.h"
#include "hardware/gpio.h" // GPIO_OVERRIDE_* per bootsel_button.h
#include "bootsel_button.h"
#include "conta_click.h"
#include "log_psrx.h"
#include "pad.h"
#include "psrx_config.h"

#include "pico/time.h"

static ContaClick conta;
static uint32_t t_lettura = 0;

void bootsel_gesti_task() {
    const uint32_t ora = to_ms_since_boot(get_absolute_time());
    if (ora - t_lettura < T_BOOTSEL_LETTURA_MS) return;
    t_lettura = ora;
    if (pad_connessi() > 0 || pad_setup_bt()) {
        conta.azzera();
        return;
    }
    const uint8_t click = conta.aggiorna(bootsel_button_pressed(), ora);
    if (click == 1) {
        psrx_log("BOOTSEL: abbinamento, finestra di 30 s (Create/Share + PS sul controller)");
        pad_avvia_abbinamento();
    } else if (click == 3) {
        aggiornamento_entra_in_bootsel("BOOTSEL premuto 3 volte");
    } else if (click > 0) {
        psrx_log("BOOTSEL: %u click, nessuna azione", click);
    }
}
