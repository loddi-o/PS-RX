// Strong override of pico_flash's weak get_flash_safety_helper().
//
// Why (GitHub issue #2, empty paired-controllers list): BTstack persists link
// keys through the SDK's btstack_flash_bank, which wraps every flash mutation
// in flash_safe_execute() and IGNORES the return code (asserts are compiled
// out in Release). The very first mutation -- formatting the TLV bank on a
// unit that never had one -- happens inside cyw43_arch_init(), BEFORE
// audio_init() launches core1. The SDK's default helper sees "core1 is not a
// registered lockout victim" and refuses with PICO_ERROR_NOT_PERMITTED, so the
// format silently never happens: the bank keeps whatever garbage the sector
// held, every later link-key store programs bytes over unerased flash, and the
// paired list reads back empty forever. (Units that once booted a build with
// PICO_FLASH_ASSUME_CORE1_SAFE=1 -- upstream, pre-2e07f72 -- already carry a
// valid bank header in flash, which is why the bug only bites fresh units.)
//
// This helper keeps the default's safe behaviour (park core1 via multicore
// lockout when it is a registered victim) and adds the one case the default
// refuses: when core1 has NEVER been launched there is no second core touching
// XIP, so the erase/program can run directly with IRQs off -- exactly what
// config_save() already does privately for AP-onboarding mode. Covers every
// flash_safe_execute() caller: BTstack's TLV bank (link keys + forget
// blacklist) and config_save().

#include "flash_safety.h"

#include <cstdio>
#include <cstring>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/btstack_flash_bank.h" // PICO_FLASH_BANK_STORAGE_OFFSET/TOTAL_SIZE
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/time.h"

// Set (pre-launch) by audio_init() via flash_safety_note_core1_launch(); never
// cleared. While false, core1 provably isn't executing, so a lockout-less
// flash op can't race it.
static volatile bool core1_launched = false;

void flash_safety_note_core1_launch() { core1_launched = true; }

// Per-core state between enter and exit, mirroring the SDK default helper.
static uint32_t irq_state[NUM_CORES];
static bool locked_out[NUM_CORES];

static bool helper_core_init_deinit(bool init) {
    if (!init) return false; // deinit unsupported, like the SDK default
    multicore_lockout_victim_init();
    return true;
}

static int helper_enter_safe_zone_timeout_ms(uint32_t timeout_ms) {
    const uint core = get_core_num();
    const uint other = core ^ 1u;

    bool do_lockout;
    if (multicore_lockout_victim_is_initialized(other)) {
        do_lockout = true;
    } else if (other == 1u && !core1_launched) {
        // Core1 was never started: single-core, direct write is safe. This is
        // the pre-core1 boot window (BTstack TLV bank format inside
        // cyw43_arch_init) and AP-onboarding mode (core1 skipped entirely).
        printf("[FLASH] safe_execute: core1 not launched -> direct write (IRQs off)\n");
        do_lockout = false;
    } else {
        // The other core is (or may be) running but hasn't registered as a
        // lockout victim. audio_init() waits for core1's registration before
        // returning, so this only happens on a code path that broke that
        // contract. Give the registration a moment, then refuse like the SDK
        // default rather than risk an unparked write under live XIP.
        const uint32_t wait_ms = timeout_ms < 100u ? timeout_ms : 100u;
        const absolute_time_t until = make_timeout_time_ms(wait_ms);
        while (!multicore_lockout_victim_is_initialized(other) && !time_reached(until)) {
            tight_loop_contents();
        }
        if (!multicore_lockout_victim_is_initialized(other)) {
            printf("[FLASH] safe_execute REFUSED: core%u running without victim init\n", other);
            return PICO_ERROR_NOT_PERMITTED;
        }
        do_lockout = true;
    }

    if (do_lockout) {
        // Nudge a __wfe()-parked core awake so it can honour the lockout
        // request (same workaround as config_save's retry loop).
        __sev();
        if (!multicore_lockout_start_timeout_us(timeout_ms * 1000ull)) {
            printf("[FLASH] safe_execute: core%u lockout timed out\n", other);
            return PICO_ERROR_TIMEOUT;
        }
    }
    locked_out[core] = do_lockout;
    irq_state[core] = save_and_disable_interrupts();
    return PICO_OK;
}

static int helper_exit_safe_zone_timeout_ms(uint32_t timeout_ms) {
    const uint core = get_core_num();
    restore_interrupts_from_disabled(irq_state[core]);
    if (locked_out[core]) {
        locked_out[core] = false;
        if (!multicore_lockout_end_timeout_us(timeout_ms * 1000ull)) {
            return PICO_ERROR_TIMEOUT;
        }
    }
    return PICO_OK;
}

static flash_safety_helper_t app_flash_safety_helper = {
    .core_init_deinit = helper_core_init_deinit,
    .enter_safe_zone_timeout_ms = helper_enter_safe_zone_timeout_ms,
    .exit_safe_zone_timeout_ms = helper_exit_safe_zone_timeout_ms,
};

// pico/flash.h declares this extern "C"; the strong definition displaces the
// weak SDK default for every flash_safe_execute() caller in the image.
flash_safety_helper_t *get_flash_safety_helper(void) {
    return &app_flash_safety_helper;
}

void flash_safety_log_btstack_bank(const char *when) {
    // Mirrors btstack_tlv_flash_bank_get_latest_bank(): each 4 KB bank starts
    // with the 7-byte magic "BTstack" + 1 epoch byte. A unit hit by issue #2
    // shows both banks INVALID here on every boot; after the fix the format
    // runs during cyw43_arch_init() and the post-init log shows bank0 valid.
    const uint32_t bank_size = PICO_FLASH_BANK_TOTAL_SIZE / 2u;
    for (uint32_t b = 0; b < 2; b++) {
        const uint8_t *hdr = reinterpret_cast<const uint8_t *>(
            XIP_BASE + PICO_FLASH_BANK_STORAGE_OFFSET + b * bank_size);
        const bool valid = memcmp(hdr, "BTstack", 7) == 0;
        printf("[TLV] %s: bank%u @0x%08x %s hdr=%02x%02x%02x%02x%02x%02x%02x epoch=%u\n",
               when, (unsigned) b,
               (unsigned) (PICO_FLASH_BANK_STORAGE_OFFSET + b * bank_size),
               valid ? "valid" : "INVALID",
               hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6], hdr[7]);
    }
}
