//
// bootsel_button.h -- safely read the Pico BOOTSEL button at runtime.
//
// The board has no spare user button, so BOOTSEL doubles as the WiFi onboarding
// trigger. The WiFi task samples it only while no controller is connected.
//
// Reading BOOTSEL means briefly taking over the QSPI CS pad (it is shared with
// the flash chip-select) and sampling it as a GPIO input. That requires PAUSING
// execute-in-place: the low-level callback MUST live in RAM and run with
// interrupts disabled, because while XIP is paused any code/data fetch from
// flash would fault. If core 1 exists, flash_safe_execute() parks it first.
//
// This follows TinyUSB's rp2040-family BOOTSEL reader. RP2350 exposes QSPI CS
// at SIO_GPIO_HI_IN_QSPI_CSN_BITS (bit 27), not QSPI pad index 1; confusing
// those two caused the old early-boot reader to report a false press.
//

#ifndef DS5_BRIDGE_BOOTSEL_BUTTON_H
#define DS5_BRIDGE_BOOTSEL_BUTTON_H

#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#if PICO_RP2350
#include "hardware/regs/sio.h"
#endif
#include "pico/flash.h"
#include "pico/multicore.h"

static void __no_inline_not_in_flash_func(bootsel_button_read_cb)(void *param) {
    bool *pressed = static_cast<bool *>(param);
    const uint CS_PIN_INDEX = 1; // QSPI SS is the flash chip-select pad

    // Float chip select and sample its pull state. The button shorts it low.
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    for (int i = 0; i < 1000; ++i) {
        __nop();
    }

#if PICO_RP2350
    *pressed = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);
#else
    *pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
#endif

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
}

// True while BOOTSEL is physically pressed. Call from core 0 only. If core 1
// is running, use the same multicore-safe coordination as flash writes; on a
// no-audio build core 1 never launches, so IRQ exclusion alone is sufficient.
static bool bootsel_button_pressed(void) {
    bool pressed = false;
    if (multicore_lockout_victim_is_initialized(1)) {
        const int rc = flash_safe_execute(bootsel_button_read_cb, &pressed, 100);
        return rc == PICO_OK && pressed;
    }

    const uint32_t flags = save_and_disable_interrupts();
    bootsel_button_read_cb(&pressed);
    restore_interrupts(flags);
    return pressed;
}

#endif // DS5_BRIDGE_BOOTSEL_BUTTON_H
