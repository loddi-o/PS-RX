//
// PS-RX - LED del Pico (vedi led.h).
//

#include "led.h"

#include "config.h"
#include "pad.h"
#include "pc.h"
#include "pico/cyw43_arch.h"

namespace {

constexpr uint32_t PERIODO_LAMPEGGIO_MS = 250;

bool risveglio_prima = false;
uint32_t t_risveglio = 0;
bool scritto = false;
bool forza = true;

} // namespace

void led_task(uint32_t ora) {
    const bool risveglio = pc_finestra_risveglio();
    if (risveglio && !risveglio_prima) t_risveglio = ora;   // il lampeggio parte acceso
    risveglio_prima = risveglio;

    bool voluto;
    if (get_config().disable_pico_led) voluto = false;
    else if (risveglio) voluto = ((ora - t_risveglio) / PERIODO_LAMPEGGIO_MS) % 2 == 0;
    else voluto = pad_connessi() > 0;

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
    return pc_finestra_risveglio();
}
