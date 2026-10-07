//
// PS-RX - LED del Pico (vedi led.h).
//

#include "led.h"

#include "config.h"
#include "log_psrx.h"
#include "pad.h"
#include "pc.h"
#include "pico/cyw43_arch.h"

namespace {

constexpr uint32_t PERIODO_LAMPEGGIO_MS = 250;
constexpr uint32_t DURATA_MAX_MS = 120000;

bool risveglio = false;
uint32_t t_risveglio = 0;
int pad_prima = 0;
bool scritto = false;
bool forza = true;

} // namespace

void led_task(uint32_t ora) {
    const int pad = pad_connessi();

    // Un controller in piu' mentre il PC non e' acceso: si sta cercando di svegliarlo.
    if (pad > pad_prima && pc_serve_wol()) {
        if (!risveglio) psrx_log("risveglio del PC: il LED lampeggia");
        risveglio = true;
        t_risveglio = ora;
    }
    pad_prima = pad;
    if (risveglio && (pad == 0 || !pc_serve_wol() || ora - t_risveglio > DURATA_MAX_MS)) {
        psrx_log("risveglio del PC: %s", pad == 0 ? "controller spenti" : !pc_serve_wol() ? "PC acceso" : "rinuncio");
        risveglio = false;
    }

    bool voluto;
    if (get_config().disable_pico_led) voluto = false;
    else if (risveglio) voluto = ((ora - t_risveglio) / PERIODO_LAMPEGGIO_MS) % 2 == 0;
    else voluto = pad > 0;

    if (voluto != scritto || forza) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, voluto);
        scritto = voluto;
        forza = false;
    }
}

void led_forza() {
    forza = true;
}

bool led_risveglio_in_corso() {
    return risveglio;
}
