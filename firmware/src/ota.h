//
// ota.h -- firmware updates over WiFi from GitHub Releases (ENABLE_OTA).
//
// DESIGN (why a dedicated boot mode): a TLS handshake wants ~20 KB of live
// heap plus temporaries, and the normal runtime only has ~27 KB free once
// BTstack + Opus are up -- while flash writes in normal mode additionally
// need core1 (audio) parked for every sector. Instead of fighting both, the
// update runs in a stripped boot mode modeled on AP onboarding: the web UI
// sets a request flag in a watchdog scratch register and reboots; early
// main() sees the flag and diverts into ota_mode_main() BEFORE BT/audio/USB
// ever start. Result: tens of KB of heap free for mbedTLS, zero radio
// contention with audio, and (core1 never launched) direct flash writes with
// IRQs off -- the exact pre-core1 path flash_safety.cpp already blesses.
//
// FLASH LAYOUT (double-buffer, no A/B boot selector):
//   [0, 2 MB)               running image (XIP)
//   [2 MB, -4 sectors)      OTA staging (downloaded image lands here)
//   [-4 sectors)            config store        (see config.cpp)
//   [-3, -1 sectors)        BTstack TLV bank
//   [-1 sector)             bootrom-erased on UF2 download
// The download NEVER touches the running image: any failure (WiFi, TLS, HTTP,
// checksum) just reboots back into the old firmware with a result code. Only
// after the staged image's SHA-256 matches the release manifest does a
// RAM-resident stub copy staging over [0, size) and reset. That final copy
// (~15 s) is the one power-loss-vulnerable window; recovery is the always-
// available USB BOOTSEL reflash. (True self-healing would need a separate
// never-overwritten bootloader partition -- deliberately out of scope.)
//
// VERSION DISCOVERY (no GitHub API, no tokens, no JSON): GET
// https://github.com/<OTA_REPO>/releases/latest answers 302 with
// Location: .../releases/tag/<tag>. The tag is compared to
// PICO_PROGRAM_VERSION_STRING; on mismatch the stable-named release assets
// <asset>.sha256 and <asset> are fetched via releases/download/<tag>/...
// (which 302s to objects.githubusercontent.com). TLS trust = pinned CA roots
// (ota_certs.h).
//
#ifndef DS5_BRIDGE_OTA_H
#define DS5_BRIDGE_OTA_H

#include <cstdint>

// Result of the most recent OTA attempt, persisted across the reboot back to
// normal firmware in a watchdog scratch register (cleared by power loss --
// acceptable: the UI then just shows "no recent update").
enum OtaResult : uint32_t {
    OTA_RESULT_NONE = 0,        // no OTA attempted since power-on
    OTA_RESULT_OK = 1,          // staged + verified + applied (set just before the copy)
    OTA_RESULT_ALREADY_CURRENT = 2,
    OTA_RESULT_NO_WIFI_CREDS = 3,
    OTA_RESULT_WIFI_JOIN_FAILED = 4,
    OTA_RESULT_TLS_INIT_FAILED = 5,
    OTA_RESULT_CHECK_FAILED = 6,   // couldn't resolve the latest release tag
    OTA_RESULT_SHA_FETCH_FAILED = 7,
    OTA_RESULT_DOWNLOAD_FAILED = 8,
    OTA_RESULT_TOO_BIG = 9,        // asset exceeds the staging window
    OTA_RESULT_SHA_MISMATCH = 10,  // staged image failed verification
    OTA_RESULT_TIMEOUT = 11,
    OTA_RESULT_OOM = 12,
};

#ifdef ENABLE_OTA

// --- normal-mode API (web UI) ----------------------------------------------

// Arm the OTA request flag and schedule a reboot ~1.2 s out (so the HTTP
// response reaches the browser first). `force` reinstalls even when the
// latest tag matches the running version. `beta` switches the release
// channel: stable resolves via the releases/latest redirect (which GitHub
// defines as the newest NON-prerelease), beta via the releases.atom feed
// (whose first entry is the newest release INCLUDING prereleases).
void ota_request_and_reboot(bool force, bool beta);

// Read + clear the persisted result of the last OTA attempt. Call once early
// in a NORMAL boot; thereafter ota_last_result()/ota_result_str() serve it.
void ota_boot_capture_result();
uint32_t ota_last_result();
const char *ota_result_str(uint32_t result);

// --- boot-mode API (main.cpp) -----------------------------------------------

// True when the scratch register carries a pending OTA request (checked once,
// right after config_load()).
bool ota_boot_pending();

// The whole OTA boot mode: STA join, discovery, download, verify, apply.
// Never returns -- every path ends in a reset (into the new firmware on
// success, back into this one otherwise).
[[noreturn]] void ota_mode_main();

// --- shared with web_api.cpp --------------------------------------------------

// True while ota_mode_main() is running; web_api uses it to serve ONLY
// /api/ota/status and reject everything else (BT/audio state doesn't exist).
bool ota_mode_active();

// Live progress snapshot for /api/ota/status (safe in both modes).
struct OtaStatus {
    const char *state;   // short machine-readable phase name
    uint32_t bytes;      // downloaded so far (bin phase)
    uint32_t total;      // Content-Length of the bin (0 until known)
    char tag[48];        // latest release tag once discovered ("" before)
};
void ota_get_status(OtaStatus *out);

#else // !ENABLE_OTA -- inert stubs so callers need no #ifdef forests

inline void ota_boot_capture_result() {}
inline uint32_t ota_last_result() { return OTA_RESULT_NONE; }
inline bool ota_boot_pending() { return false; }
inline bool ota_mode_active() { return false; }

#endif // ENABLE_OTA

#endif // DS5_BRIDGE_OTA_H
