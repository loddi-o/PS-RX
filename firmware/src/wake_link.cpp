//
// wake_link.cpp -- companion-Pico wake signal (ENABLE_WAKE_LINK, scaffolding).
// See wake_link.h for the design + wire protocol. This side is deliberately
// dumb: init the pin, pulse it HIGH for WAKE_LINK_PULSE_MS on request, drop it
// from the main loop. All wake policy (suspend gate, once-per-spell) lives in
// wake.cpp next to the WOL policy so the two stay in lockstep.
//

#include "wake_link.h"

#ifdef ENABLE_WAKE_LINK

#include "hardware/gpio.h"
#include "pico/time.h"

// Signal line to the companion Pico. GP2 is free on this board map (UART0 diag
// is GP0/GP1); override with -DWAKE_LINK_GPIO=<n> if it collides with a carrier
// board.
#ifndef WAKE_LINK_GPIO
#define WAKE_LINK_GPIO 2
#endif

// Long enough for the companion to sample it even from a slow poll loop; short
// enough that back-to-back suspend spells can't blur together.
#define WAKE_LINK_PULSE_MS 100

static bool pulse_active = false;
static absolute_time_t pulse_end;

void wake_link_init(void) {
    gpio_init(WAKE_LINK_GPIO);
    gpio_set_dir(WAKE_LINK_GPIO, GPIO_OUT);
    gpio_put(WAKE_LINK_GPIO, 0); // idle LOW
}

void wake_link_pulse(void) {
    gpio_put(WAKE_LINK_GPIO, 1);
    pulse_end = make_timeout_time_ms(WAKE_LINK_PULSE_MS);
    pulse_active = true;
}

void wake_link_task(void) {
    if (pulse_active && time_reached(pulse_end)) {
        gpio_put(WAKE_LINK_GPIO, 0);
        pulse_active = false;
    }
}

#endif // ENABLE_WAKE_LINK
