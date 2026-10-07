//
// web_api.h -- transport-agnostic config web UI (lwIP httpd).
//
// The config page + JSON API (/api/config, /api/bonds, /api/status, /api/wol)
// served over the onboard WiFi transport (ENABLE_WIFI_WOL, src/wifi_net.cpp).
// This file holds everything that does NOT depend on the netif: the
// fs_open_custom content, the JSON builders, and the POST handlers.
// httpd_init() is called from web_api_init(); the transport brings up lwIP +
// the netif first, then calls web_api_init(). (Historically this was shared
// with a USB CDC-NCM transport, since removed.)
//
// Guarded by ENABLE_WEBUI (defined by CMake when the transport is enabled).
//

#ifndef DS5_BRIDGE_WEB_API_H
#define DS5_BRIDGE_WEB_API_H

#include <cstdint>

#ifdef ENABLE_WEBUI

// Register httpd custom-file/POST handling. Call once, AFTER lwip_init() and the
// transport's netif is up.
void web_api_init();

// Wake-on-LAN send hook, PROVIDED BY THE TRANSPORT (weak default is a no-op).
// web_api calls this when the UI POSTs /api/wol (action=wake) or when another
// subsystem requests a wake. `mac` is the 6-byte target NIC address. Only the
// WiFi transport implements a real send (broadcast magic packet); the NCM
// transport's link can't reach the LAN, so it stays a no-op there.
// Returns true if a packet was actually emitted.
bool web_api_wol_send(const uint8_t mac[6]);

// Wake EVERY configured (non-zero) WOL target, PROVIDED BY THE TRANSPORT (weak
// default is a no-op). Used by the UI's "Wake now" button when no explicit MAC
// is given, so it fires all stored targets (PC + optional 2nd, e.g. a TV)
// through one code path. Returns true if at least one packet was emitted.
bool web_api_wol_send_all(void);

// ARP MAC-resolution hooks, PROVIDED BY THE TRANSPORT (weak defaults are
// no-ops). web_api calls _start() when the UI POSTs /api/resolve_mac with a
// target IP, so the user can fill in the WOL MAC field without hunting it
// down by hand (mirrors PC-wake-dongle's IP->MAC resolve button). Only the
// WiFi transport can actually ARP the local LAN.
//
// Both are non-blocking by design: the POST callback that calls _start() runs
// nested inside lwIP's own tcp_input()/timeout processing, so pumping the
// network stack again from in there (to wait for an ARP reply) would re-enter
// non-reentrant lwIP internals and corrupt the in-flight connection. _start()
// only kicks off the resolve; the transport's own main-loop task polls for the
// reply. web_api's GET /api/resolve_mac route calls _poll() to report it back.
void web_api_resolve_mac_start(const uint8_t ip[4]);

// Returns 0 = still pending, 1 = resolved (out_mac filled), -1 = failed.
int web_api_resolve_mac_poll(uint8_t out_mac[6]);

#else
static inline void web_api_init() {}
static inline bool web_api_wol_send(const uint8_t *) { return false; }
static inline bool web_api_wol_send_all(void) { return false; }
static inline void web_api_resolve_mac_start(const uint8_t *) {}
static inline int web_api_resolve_mac_poll(uint8_t *) { return -1; }
#endif // ENABLE_WEBUI

#endif // DS5_BRIDGE_WEB_API_H
