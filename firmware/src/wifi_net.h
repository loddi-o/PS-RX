//
// wifi_net.h -- onboard CYW43 Wi-Fi (STA mode) transport for the config web UI
// + Wake-on-LAN (ENABLE_WIFI_WOL). The only config transport.
//
// Needs NO extra hardware: the radio is the same CYW43 chip BTstack already
// drives, and its WiFi firmware is already linked into the image. The SDK's
// cyw43_arch owns the lwIP netif + DHCP for the WiFi interface (CYW43_LWIP=1),
// so the shared web_api/httpd layer is reused unchanged. BT and WiFi share one
// async_context (the poll context), both pumped by cyw43_arch_poll() in the
// main loop -- watch that radio sharing when changing anything audio-adjacent.
//

#ifndef DS5_BRIDGE_WIFI_NET_H
#define DS5_BRIDGE_WIFI_NET_H

#include <cstdint>

#ifdef ENABLE_WIFI_WOL
// Force the next wifi_net_init() into AP + captive-portal onboarding regardless
// of whether credentials are stored. The runtime BOOTSEL gesture carries its
// one-shot request across a reboot and calls this before mode selection. With
// no stored credentials, AP mode is entered automatically.
void wifi_net_request_ap_onboarding();

// Bring up the WiFi transport. Picks the mode automatically:
//   - provisioned (creds in flash) and AP not forced -> STA: join the home WLAN,
//     start DHCP client + mDNS + httpd (normal operation).
//   - unprovisioned, or AP forced via the call above -> AP + captive portal:
//     start SoftAP "DS5-Setup-XXXX", run a DHCP + DNS server, serve the
//     onboarding page so the user can pick a network and save credentials.
// Call once after cyw43_arch_init() and config_load() (main.cpp). lwIP itself is
// already initialised by cyw43_arch_init() (CYW43_LWIP=1) -- do NOT lwip_init().
void wifi_net_init();

// True once the device is running the AP onboarding portal (not STA). The web
// layer uses this to serve the onboarding page instead of the config page.
bool wifi_net_in_ap_mode();

// Call every main-loop iteration: pumps lwIP timers + the ARP-resolve poll +
// status logging. In STA mode it also samples the BOOTSEL triple-click onboarding
// gesture at 20 Hz, but only while no controller is connected. In AP mode it
// owns the continuous 2 Hz status LED. (RX and the netif are pumped by
// cyw43_arch_poll() already.)
void wifi_net_task();

// Send a Wake-on-LAN magic packet to `mac` (6 bytes) as a broadcast UDP datagram
// to 255.255.255.255:9. Returns true if queued. Also wired to the shared
// web_api_wol_send() hook (strong override in wifi_net.cpp).
bool wifi_wol_send(const uint8_t mac[6]);

// Kick off non-blocking ARP resolution of `ip` (4 bytes, local subnet).
// Start/poll split: must not block in the httpd POST callback, which is nested
// inside lwIP's own tcp_input(). Wired to web_api_resolve_*.
void wifi_resolve_mac_start(const uint8_t ip[4]);

// Poll the result: 0 = pending, 1 = resolved (out_mac filled), -1 = failed.
int wifi_resolve_mac_poll_result(uint8_t out_mac[6]);

// Onboarding portal hooks (AP mode), wired to the shared web layer.
//
// Kick off a WiFi scan for the portal's network dropdown. Non-blocking: the scan
// runs in the cyw43 poll context; wifi_net_task() collects results. No-op (and
// the result list stays as-is) when not in AP mode.
void wifi_scan_start();
// Write up to `max` scan results as a JSON array of {"ssid","rssi","secure"}
// into `out` (cap bytes). Returns bytes written. De-duplicated, RSSI-sorted.
int wifi_scan_json(char *out, int cap);
// Apply onboarding credentials from the portal: persist ssid/psk plus the
// CONFIG_WIFI_AUTH_WPA2/WPA3 selector to flash and schedule a reboot into STA
// mode. Returns true if accepted (non-empty ssid).
bool wifi_provision_apply(const char *ssid, const char *psk,
                          uint8_t auth_mode);

// Clear saved WiFi credentials, persist the unprovisioned state, and schedule a
// reboot. The next boot lands in AP + captive-portal onboarding.
bool wifi_reset_provisioning_apply();
#else
static inline void wifi_net_request_ap_onboarding() {}
static inline void wifi_net_init() {}
static inline bool wifi_net_in_ap_mode() { return false; }
static inline void wifi_net_task() {}
static inline bool wifi_wol_send(const uint8_t *) { return false; }
static inline void wifi_resolve_mac_start(const uint8_t *) {}
static inline int wifi_resolve_mac_poll_result(uint8_t *) { return -1; }
static inline void wifi_scan_start() {}
static inline int wifi_scan_json(char *, int) { return 0; }
static inline bool wifi_provision_apply(const char *, const char *, uint8_t) {
    return false;
}
static inline bool wifi_reset_provisioning_apply() { return false; }
#endif // ENABLE_WIFI_WOL

#endif // DS5_BRIDGE_WIFI_NET_H
