//
// See weblog.h. A pico stdio driver whose out_chars records the stream two
// ways: the FIRST KB of output after enable is frozen forever (boot
// diagnostics -- [MEM], [Audio] heap, [FLASHBANK] -- must survive hours of
// HCI chatter), and a rolling ring keeps the most recent KB. /api/log
// (web_api.cpp) serves boot section + gap marker + recent tail. Registered
// ALONGSIDE the UART driver, so GP0 output is unaffected either way; the
// enable toggle only controls whether this mirror sees the stream.
//

#include "weblog.h"

#ifdef ENABLE_WIFI_WOL

#include <cstdint>

#include "pico/stdio.h"
#include "pico/stdio/driver.h"

namespace {
constexpr uint32_t BOOT_SIZE = 1024; // frozen: first bytes after enable
constexpr uint32_t RING_SIZE = 1024; // power of two; rolling recent tail

char     bootbuf[BOOT_SIZE];
uint32_t bootlen = 0;

char ring[RING_SIZE];
// Total chars ever written; ring index is pos & (RING_SIZE-1). Races with
// core1 printfs are tolerable for diagnostics (worst case: garbled chars).
// NEVER written to flash.
volatile uint32_t wpos = 0;

bool enabled_now = false;

void weblog_out_chars(const char *buf, int len) {
    const uint32_t p = wpos;
    for (int i = 0; i < len; i++) {
        if (bootlen < BOOT_SIZE) bootbuf[bootlen++] = buf[i];
        ring[(p + (uint32_t) i) & (RING_SIZE - 1)] = buf[i];
    }
    wpos = p + (uint32_t) len;
}
} // namespace

static stdio_driver_t weblog_driver = {};

void weblog_init() {
    weblog_driver.out_chars = weblog_out_chars;
    // Registered on demand by weblog_set_enabled(); nothing to do here beyond
    // wiring the callback (kept as an init hook so the call order in main()
    // documents where capture COULD begin).
}

void weblog_set_enabled(bool enabled) {
    if (enabled == enabled_now) return;
    enabled_now = enabled;
    stdio_set_driver_enabled(&weblog_driver, enabled);
}

bool weblog_enabled() { return enabled_now; }

int weblog_snapshot(char *out, int cap) {
    if (cap <= 0) return 0;
    int n = 0;
    const uint32_t end = wpos;

    // Frozen boot section.
    for (uint32_t i = 0; i < bootlen && n < cap - 1; i++) out[n++] = bootbuf[i];

    // Recent tail from the ring, skipping what the boot section already has.
    uint32_t start = end > RING_SIZE ? end - RING_SIZE : 0;
    if (start < bootlen) start = bootlen;
    if (start > bootlen) {
        static const char gap[] = "\n...[older log rolled off]...\n";
        for (const char *g = gap; *g && n < cap - 1; g++) out[n++] = *g;
    }
    for (uint32_t i = start; i < end && n < cap - 1; i++) {
        out[n++] = ring[i & (RING_SIZE - 1)];
    }
    out[n] = '\0';
    return n;
}

#endif // ENABLE_WIFI_WOL
