//
// Created by awalol on 2026/5/4.
//

#ifndef DS5_BRIDGE_CONFIG_H
#define DS5_BRIDGE_CONFIG_H

#include <cstdint>

// User-assignable nicknames for paired controllers, shown in the web UI.
// Keyed by Bluetooth address; capacity mirrors btstack's NVM_NUM_LINK_KEYS (4).
#define CONFIG_MAX_BOND_NAMES 4
#define CONFIG_BOND_ADDR_LEN  6
#define CONFIG_BOND_NAME_LEN  16 // 15 chars + NUL

//--------------------------------------------------------------------+
// Config_body layout is APPEND-ONLY. To stay compatible with configs
// already written to flash by older firmware, obey these rules:
//   * Only ever add new fields at the END of Config_body.
//   * Never reorder, resize, remove, or repurpose an existing field.
//   * When you add a field, give it a sane default in config_valid().
// Reads are migrated by size: newer firmware keeps every field an older
// blob contained and default-initializes the newly-appended tail (see
// config_load()). The offsetof() static_asserts in config.cpp pin the
// original field layout so an accidental mid-struct insertion fails the
// build instead of silently corrupting persisted config.
//
// CONFIG_VERSION is a *layout* number, NOT a reset trigger. Bump it only on
// a genuinely incompatible change (which append-only should make rare). To
// intentionally wipe settings, call config_factory_reset() -- do not abuse
// the version for that.
//--------------------------------------------------------------------+

// mDNS / network hostname (the "<name>.local" the dongle advertises). User-set
// so two dongles on one LAN don't both claim ds5.local. Max 10 chars + NUL;
// validated to a DNS label (lowercase a-z, 0-9, hyphen; no leading/trailing
// hyphen) in config_valid(). See CONFIG_HOSTNAME_DEFAULT.
#define CONFIG_HOSTNAME_LEN     11
#define CONFIG_HOSTNAME_DEFAULT "ds5"

// Home-WLAN credentials for the WiFi-WOL transport's STA join (ENABLE_WIFI_WOL).
// Filled by the onboarding captive portal and persisted to flash, replacing the
// old gitignored wifi_secrets.h. SSID is 32 octets max (802.11) + NUL; a
// WPA2/WPA3 Personal passphrase is stored as 8..63 chars + NUL.
// wifi_provisioned gates STA vs AP mode:
// 0 == no usable creds yet -> come up in AP + captive portal so the user can
// onboard; 1 == creds present -> join the home WLAN. Present in EVERY build so
// the flash layout/Config_body size is identical across transports (only the
// WiFi build reads them), same convention as the WOL fields above.
#define CONFIG_WIFI_SSID_LEN    33  // 32 chars + NUL
#define CONFIG_WIFI_PSK_LEN     64  // 63 chars + NUL
#define CONFIG_WIFI_AUTH_WPA2   0
#define CONFIG_WIFI_AUTH_WPA3   1

struct __attribute__((packed)) BondName {
    uint8_t addr[CONFIG_BOND_ADDR_LEN]; // all-zero == empty slot
    char    name[CONFIG_BOND_NAME_LEN]; // NUL-terminated; "" == unnamed
};

struct __attribute__((packed)) Config_body {
    uint8_t config_version; // Config Version
    float speaker_volume; // reserved (WebHID-era speaker volume; unread -- UAC volume is authoritative). [-100,0] clamp kept to sanitize old blobs.
    uint8_t inactive_time; // [5,60] min
    uint8_t disable_inactive_disconnect; // bool: 0 disable,1 enable
    uint8_t disable_pico_led; // bool
    uint8_t polling_rate_mode; // 0: 250Hz, 1: 500Hz, 2: real-time
    uint8_t audio_buffer_length; // [16,128]
    uint8_t controller_mode; // 0: DS5, 1: DSE, 2: Auto
    // Reserved: config-page address selector + custom IP from the retired
    // USB-NCM web transport (the config page is served over WiFi now). Blobs
    // written by NCM-era firmware carry them at these offsets, so they stay
    // (append-only rule: never remove or repurpose). Do not reuse.
    uint8_t webconfig_subnet;       // reserved (NCM-era; unread)
    uint8_t webconfig_custom_ip[4]; // reserved (NCM-era; unread)
    BondName bond_names[CONFIG_MAX_BOND_NAMES]; // nicknames for paired controllers
    // --- append new fields BELOW this line only (see append-only note above) ---
    // Wake-on-LAN target (ENABLE_WIFI_WOL builds). wol_target_mac is the NIC of
    // the PC to wake; all-zero == unset (no MAC configured -> WOL is a no-op).
    // Present in EVERY build so the flash layout/Config_body size is identical
    // across transports -- only the WiFi build reads it.
    uint8_t wol_target_mac[6];
    // Static addressing knobs from the retired W5500 Ethernet transport. The
    // WiFi transport is DHCP-only and never reads them, but blobs written by
    // W5500-era firmware carry them at these offsets, so they stay (append-only
    // rule: never remove or repurpose). Do not reuse for anything else.
    uint8_t wol_use_static_ip;   // reserved (W5500-era; unread)
    uint8_t wol_static_ip[4];    // reserved (W5500-era; unread)
    uint8_t wol_static_netmask[4]; // reserved (W5500-era; unread, was RAM-only)
    // Network hostname advertised over mDNS as "<hostname>.local" (and set as the
    // netif hostname). Defaults to CONFIG_HOSTNAME_DEFAULT. User-editable in the
    // web UI so multiple dongles on one LAN don't collide on ds5.local.
    // config_valid() sanitizes it to a valid DNS label and re-defaults if empty.
    char hostname[CONFIG_HOSTNAME_LEN];
    // Home-WLAN credentials (WiFi-WOL onboarding). wifi_provisioned: 0 = no creds
    // -> AP + captive portal; 1 = creds set -> STA join. wifi_ssid is NOT
    // necessarily a DNS label, so it is NUL-terminated and length-bounded but not
    // otherwise sanitized; the password is stored verbatim. config_valid() forces
    // termination and clears wifi_provisioned if the SSID is empty. NEVER emitted
    // back to the web UI in cleartext (the portal only ever writes them).
    uint8_t wifi_provisioned;            // bool: 0 = onboard via AP, 1 = STA creds set
    char    wifi_ssid[CONFIG_WIFI_SSID_LEN];
    char    wifi_psk[CONFIG_WIFI_PSK_LEN];
    // USB wake keyboard (runtime web-UI toggle, replaces the old
    // WAKE_VIA_USB_KBD compile option). 1 = enumerate a boot keyboard alongside
    // the gamepad/dummy HID so an F15 keystroke can wake the host from S3;
    // 0 (default) = pure-DualSense USB face (anticheat-safe). Applied live via
    // a descriptor-variant bounce (usb_request_wake_kbd). Only meaningful in
    // ENABLE_WAKE_HID builds; stored in every build (append-only layout rule).
    uint8_t wake_kbd_enabled;            // bool
    // Second Wake-on-LAN target (ENABLE_WIFI_WOL builds), e.g. a TV alongside the
    // PC in wol_target_mac. all-zero == unset (skipped). A "Wake" fires a magic
    // packet to every configured (non-zero) target. Same unset convention and
    // every-build-storage rationale as wol_target_mac above.
    uint8_t wol_target_mac2[6];
    // Multi-controller opt-in (v11). 0 (default) = single-controller only,
    // exactly the pre-multi behavior: a 2nd pad is declined at connect and
    // the descriptor never leaves MINIMAL/FULL -- existing users see nothing
    // new until they enable it. 1 = up to MULTI_SLOT_COUNT concurrent pads.
    // The append-only migration zero-fills new tail fields, which lands on
    // the OFF default by construction. Exposed to the web UI as
    // "multi_allowed". Only meaningful when MULTI_SLOT_COUNT > 1; stored in
    // every build (append-only layout rule).
    uint8_t multi_enabled;               // bool; 0 (default) = single-controller
    // Diagnostic web log (v11). 1 = mirror printf into the RAM ring served at
    // /api/log (weblog.cpp); 0 (default) = mirror disabled, /api/log answers
    // "disabled". RAM-only -- log lines never touch flash. Persisted so a
    // user can enable it, reproduce an issue across reboots (boot logs
    // captured), and copy the log from the browser.
    uint8_t weblog_enabled;              // bool
    // Home-WLAN authentication selected during onboarding. This lives at the
    // append-only tail rather than beside wifi_psk. Zero is deliberately WPA2
    // so migrated configs preserve the pre-WPA3 connection behavior. WPA3
    // means Personal/SAE; open networks ignore this field.
    uint8_t wifi_auth_mode;               // CONFIG_WIFI_AUTH_WPA2/WPA3
};

struct __attribute__((packed)) Config {
    uint32_t magic;
    uint16_t version;   // layout version (see append-only note); NOT a reset trigger
    uint32_t crc32;     // crc32 of the first `size` bytes of body; set/verified on save
    uint16_t size;      // number of valid body bytes written == sizeof(Config_body) at save time
    Config_body body;
};

void config_default();
void config_load();
bool config_save();
// Reset every setting to defaults and persist. This is the deliberate wipe
// path (e.g. a web-UI "factory reset"); bumping CONFIG_VERSION is not.
bool config_factory_reset();
const Config_body& get_config();
void config_valid();
void set_config(const Config_body &new_config);

// Bond-name table (nicknames keyed by Bluetooth address). These mutate the
// in-RAM config; the caller persists with config_save() when ready.

// Look up the nickname for `addr` (CONFIG_BOND_ADDR_LEN bytes). Returns the
// stored name (may be "") if a slot matches, or nullptr if none does.
const char *config_bond_name(const uint8_t *addr);

// Assign `name` to `addr`, reusing an existing slot for that address or the
// first empty slot. An empty/blank name clears the slot. Returns false if
// there was no slot free for a new address. Does NOT call config_save().
bool config_set_bond_name(const uint8_t *addr, const char *name);

// Clear the name slot for `addr` (e.g. when its bond is forgotten).
void config_clear_bond_name(const uint8_t *addr);

// Store home-WLAN credentials from the onboarding portal and mark the device
// provisioned (wifi_provisioned=1) so the next boot joins as STA instead of
// opening the captive portal. `ssid`/`psk` are copied length-bounded and
// NUL-terminated; `auth_mode` selects WPA2 or WPA3 Personal. An empty `ssid`
// clears provisioning instead. Mutates the in-RAM config only; the caller
// persists with config_save().
void config_set_wifi_creds(const char *ssid, const char *psk,
                           uint8_t auth_mode);

extern bool is_dse;

#endif //DS5_BRIDGE_CONFIG_H
