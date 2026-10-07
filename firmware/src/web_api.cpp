//
// web_api.cpp -- transport-agnostic config web UI (lwIP httpd).
//
// Nothing here knows or cares which netif delivered the request -- the WiFi
// transport (wifi_net.cpp) brings up lwIP + the netif and calls web_api_init().
// See web_api.h.
//

#include "web_api.h"

#ifdef ENABLE_WEBUI

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"
#include "lwip/pbuf.h"

#include "hardware/watchdog.h"

#include "bt.h"
#include "config.h"
#include "ota.h" // OTA-mode gating + /api/ota/* routes (ENABLE_OTA)
#include "usb.h" // usb_request_wake_kbd(): live-apply the wake-kbd toggle
#include "web_api.h"
#include "web_page.h"
#include "weblog.h" // /api/log diagnostic ring + runtime toggle
#ifdef ENABLE_WIFI_WOL
#include "wifi_net.h"   // AP-mode detection + scan/provision hooks
#include "web_portal.h" // onboarding captive-portal page
// Result of the most recent provision POST, reported by the synthetic
// /api/wifi_provision_result route. Set in apply_wifi_provision_post() (below
// fs_open_custom, which reads it), so forward-declare here.
static bool provision_ok;
static bool wifi_reset_ok;
#endif

//--------------------------------------------------------------------+
// WOL send hook: weak no-op default. The WiFi transport provides a strong
// definition (see wifi_net.cpp). With only the NCM transport linked, this
// no-op is used and /api/wol simply reports "no transport" -- the NCM link
// can't reach the home LAN, so there is nothing to send.
//--------------------------------------------------------------------+
extern "C" __attribute__((weak)) bool web_api_wol_send_impl(const uint8_t mac[6]) {
    (void) mac;
    return false;
}
bool web_api_wol_send(const uint8_t mac[6]) { return web_api_wol_send_impl(mac); }

// "Wake every stored target" hook: weak no-op default, strong override in
// wifi_net.cpp (wifi_wol_send_all). Used by the "Wake now" button.
extern "C" __attribute__((weak)) bool web_api_wol_send_all_impl(void) {
    return false;
}
bool web_api_wol_send_all(void) { return web_api_wol_send_all_impl(); }

//--------------------------------------------------------------------+
// ARP resolve hooks: weak no-op defaults, strong overrides in wifi_net.cpp.
// See web_api.h for rationale (non-blocking start/poll split).
//--------------------------------------------------------------------+
extern "C" __attribute__((weak)) void web_api_resolve_mac_start_impl(const uint8_t ip[4]) {
    (void) ip;
}
extern "C" __attribute__((weak)) int web_api_resolve_mac_poll_impl(uint8_t out_mac[6]) {
    (void) out_mac;
    return -1;
}
void web_api_resolve_mac_start(const uint8_t ip[4]) { web_api_resolve_mac_start_impl(ip); }
int web_api_resolve_mac_poll(uint8_t out_mac[6]) { return web_api_resolve_mac_poll_impl(out_mac); }

//--------------------------------------------------------------------+
// HTTP content: / (page) + JSON API -- via fs_open_custom
//--------------------------------------------------------------------+

// Build a complete response (headers + body) into a malloc'd buffer owned by
// the fs_file (freed in fs_close_custom). Used for the SMALL dynamic JSON
// responses (<=512 B). NOT used for the ~5 KB static page -- see
// make_static_page() below, which avoids copying the page into the heap.
static int make_file(struct fs_file *file, const char *status, const char *content_type,
                     const char *body, int body_len) {
    const int hdr_max = 160;
    char *buf = (char *) malloc(hdr_max + body_len);
    if (!buf) return 0;
    int hdr_len = snprintf(buf, hdr_max,
                           "HTTP/1.1 %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\n"
                           "Connection: close\r\nContent-Length: %d\r\n\r\n",
                           status, content_type, body_len);
    memcpy(buf + hdr_len, body, body_len);
    memset(file, 0, sizeof(*file));
    file->data = buf; // malloc'd; reclaimed in fs_close_custom via file->data
    file->len = (int) (hdr_len + body_len);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
}

// Serve the static config page WITHOUT ever copying its ~18.5 KB body into the
// heap. The old make_file() malloc'd 160 + ~18.5 KB PER GET; two clients loading
// the page at once summed to ~37 KB of transient heap against the WiFi+BT build's
// ~27 KB free-heap cushion -> OOM panic.
//
// Fix: stream the page from flash via fs_read_custom() in small (~MSS) chunks.
// With FS_FILE_FLAGS_CUSTOM and file->data == NULL, httpd skips its in-memory
// fast path entirely and pulls every byte through fs_read() -> fs_read_custom()
// into a per-connection ~536 B hs->buf (httpd.c http_check_eof). Peak heap per
// page load drops from ~18.5 KB to ~0.5 KB. The header is emitted by
// fs_read_custom() too (HEADER_INCLUDED, so httpd doesn't generate its own):
// file->index 0..PAGE_HDR_LEN-1 = header bytes, then WEB_PAGE[index-PAGE_HDR_LEN].
// Requires LWIP_HTTPD_DYNAMIC_FILE_READ=1 (set in lwipopts.h).
// The streamer can serve more than one flash page (the config page, and -- in
// the WiFi build -- the onboarding portal page). httpd serves one custom file at
// a time, so the active page is tracked module-statically: fs_open_custom()
// picks it, fs_read_custom() reads from it. `cur_page_*` are the body bytes;
// PAGE_HDR holds the matching response header (its Content-Length differs per
// page, so it's rebuilt whenever the active page changes).
static const char *cur_page_body = nullptr;
static int         cur_page_len  = 0;
static char PAGE_HDR[160];
static int  PAGE_HDR_LEN = 0;

// Point the streamer at `body`/`len` and (re)build its response header.
static int make_static_page(struct fs_file *file, const char *body, int len) {
    cur_page_body = body;
    cur_page_len  = len;
    PAGE_HDR_LEN = snprintf(PAGE_HDR, sizeof(PAGE_HDR),
                            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                            "Cache-Control: no-store\r\nConnection: close\r\n"
                            "Content-Length: %d\r\n\r\n", len);
    memset(file, 0, sizeof(*file));
    file->data = NULL;                 // NULL + CUSTOM -> httpd streams via fs_read_custom
    file->len = PAGE_HDR_LEN + len;
    file->index = 0;                   // cursor: header bytes, then flash body
    file->flags = FS_FILE_FLAGS_CUSTOM | FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
}

// "AABBCCDDEEFF" (12 hex, no separators).
static void mac_to_hex(const uint8_t *a, char out[13]) {
    static const char h[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        out[i * 2]     = h[(a[i] >> 4) & 0xf];
        out[i * 2 + 1] = h[a[i] & 0xf];
    }
    out[12] = '\0';
}

static int json_config(char *out, size_t cap) {
    const Config_body &c = get_config();
    char wol_hex[13];
    mac_to_hex(c.wol_target_mac, wol_hex);
    char wol_hex2[13];
    mac_to_hex(c.wol_target_mac2, wol_hex2);
    return snprintf(out, cap,
                    "{\"version\":\"%s\","
                    "\"inactive_time\":%u,"
                    "\"disable_inactive_disconnect\":%u,"
                    "\"disable_pico_led\":%u,"
                    "\"polling_rate_mode\":%u,"
                    "\"audio_buffer_length\":%u,"
                    "\"controller_mode\":%u,"
                    "\"wol_target_mac\":\"%s\","
                    "\"wol_target_mac2\":\"%s\","
                    // hostname is sanitized to [a-z0-9-] in config_valid(), so it
                    // never needs JSON string escaping here.
                    "\"hostname\":\"%s\","
                    "\"wake_kbd_enabled\":%u,"
                    // Whether this firmware can enumerate the wake keyboard at
                    // all (needs the dynamic-descriptor machinery). The page
                    // hides the Wake section when false.
#ifdef ENABLE_WAKE_HID
                    "\"wake_kbd_capable\":true,"
#else
                    "\"wake_kbd_capable\":false,"
#endif
                    // Multi-controller support: capable = compiled slot count
                    // > 1 (the page hides the toggle otherwise); allowed = the
                    // runtime opt-in (config multi_enabled, default off).
#if MULTI_SLOT_COUNT > 1
                    "\"multi_capable\":true,"
#else
                    "\"multi_capable\":false,"
#endif
                    "\"multi_allowed\":%u,"
                    "\"multi_slots\":%u,"
                    "\"weblog_enabled\":%u,"
                    // The web UI only exists on the WiFi transport now, so a
                    // served page always has WOL + WiFi controls available.
                    "\"wol_capable\":true,"
                    // OTA from GitHub Releases (the page hides the Updates
                    // section when false: Pico W / custom no-OTA builds).
                    // ota_repo lets the page query api.github.com DIRECTLY
                    // (CORS-open) for "update available" display -- the
                    // browser does that TLS, the firmware never does any in
                    // normal mode. The dongle-side updater independently
                    // re-resolves the tag at install time; the browser check
                    // is cosmetic, never trusted.
#ifdef ENABLE_OTA
                    "\"ota_capable\":true,"
                    "\"ota_repo\":\"" OTA_REPO "\","
#else
                    "\"ota_capable\":false,"
#endif
                    "\"wifi_capable\":true}",
                    PICO_PROGRAM_VERSION_STRING,
                    c.inactive_time,
                    c.disable_inactive_disconnect,
                    c.disable_pico_led,
                    c.polling_rate_mode,
                    c.audio_buffer_length,
                    c.controller_mode,
                    wol_hex,
                    wol_hex2,
                    c.hostname,
                    c.wake_kbd_enabled,
                    (unsigned) (c.multi_enabled ? 1 : 0),
                    (unsigned) MULTI_SLOT_COUNT,
                    c.weblog_enabled);
}

//--------------------------------------------------------------------+
// Paired-controller (bond) management helpers
//--------------------------------------------------------------------+

// Parse exactly 12 hex chars into a[6]. Returns true on success.
static bool hex_to_addr(const char *s, uint8_t a[6]) {
    if (!s) return false;
    uint8_t bytes[6];
    auto nib = [](char x) -> int {
        if (x >= '0' && x <= '9') return x - '0';
        if (x >= 'a' && x <= 'f') return x - 'a' + 10;
        if (x >= 'A' && x <= 'F') return x - 'A' + 10;
        return -1;
    };
    for (int i = 0; i < 6; i++) {
        char c = s[i * 2], d = s[i * 2 + 1];
        if (!c || !d) return false;
        int hi = nib(c), lo = nib(d);
        if (hi < 0 || lo < 0) return false;
        bytes[i] = (uint8_t) ((hi << 4) | lo);
    }
    if (s[12] != '\0') return false; // trailing junk
    memcpy(a, bytes, 6);
    return true;
}

// Append a JSON string literal (with quotes) for `s`, escaping " and \.
static int json_str(char *out, size_t cap, const char *s) {
    size_t i = 0;
    if (cap < 3) return 0;
    out[i++] = '"';
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (c == '"' || c == '\\') {
            if (i + 2 >= cap - 1) break;
            out[i++] = '\\';
            out[i++] = c;
        } else if ((unsigned char) c < 0x20) {
            continue; // drop control chars
        } else {
            if (i + 1 >= cap - 1) break;
            out[i++] = c;
        }
    }
    out[i++] = '"';
    out[i] = '\0';
    return (int) i;
}

static int json_bonds(char *out, size_t cap) {
    uint8_t list[CONFIG_MAX_BOND_NAMES][BT_ADDR_LEN];
    const int n = bt_bond_list(list, CONFIG_MAX_BOND_NAMES);

    // Legacy single-connected field (lowest connected slot) kept for older
    // clients; connected_all carries EVERY live pad so the bond list can mark
    // all of them (multi-slot).
    uint8_t conn[BT_ADDR_LEN];
    const bool have_conn = bt_connected_addr(conn);
    char conn_hex[13] = "";
    if (have_conn) mac_to_hex(conn, conn_hex);

    int w = snprintf(out, cap, "{\"connected\":\"%s\",\"connected_all\":[", conn_hex);
    int nconn = 0;
    for (uint8_t slot = 0; slot < BT_MAX_SLOTS && w < (int) cap; slot++) {
        BtStatus ss;
        bt_get_status(slot, &ss);
        if (!ss.connected) continue;
        char hex[13];
        mac_to_hex(ss.addr, hex);
        w += snprintf(out + w, cap - w, "%s\"%s\"", nconn ? "," : "", hex);
        nconn++;
    }
    if (w < (int) cap)
        w += snprintf(out + w, cap - w, "],\"max\":%d,\"bonds\":[", CONFIG_MAX_BOND_NAMES);
    for (int i = 0; i < n && w < (int) cap; i++) {
        char hex[13];
        mac_to_hex(list[i], hex);
        const char *nm = config_bond_name(list[i]);
        if (!nm) nm = "";
        w += snprintf(out + w, cap - w, "%s{\"addr\":\"%s\",\"name\":",
                      i ? "," : "", hex);
        if (w < (int) cap) w += json_str(out + w, cap - w, nm);
        if (w < (int) cap) w += snprintf(out + w, cap - w, "}");
    }
    if (w < (int) cap) w += snprintf(out + w, cap - w, "]}");
    return w;
}

// GET /api/status -- live controller health (read-only). The top-level fields
// describe the lowest connected slot (Decky-plugin compatibility: they mean
// exactly what they did on single-controller firmware); "slots" carries one
// entry per seat for the multi UI.
static int json_status(char *out, size_t cap) {
    const int lowest = bt_lowest_connected_slot();
    BtStatus s;
    bt_get_status(lowest < 0 ? 0 : (uint8_t) lowest, &s);
    int w = snprintf(out, cap,
                     "{\"connected\":%s,"
                     "\"model\":\"%s\","
                     "\"battery_valid\":%s,"
                     "\"battery_pct\":%u,"
                     "\"charging\":%s,"
                     "\"slots\":[",
                     s.connected ? "true" : "false",
                     s.is_dse ? "DSE" : "DS5",
                     s.battery_valid ? "true" : "false",
                     s.battery_pct,
                     s.charging ? "true" : "false");
    for (uint8_t slot = 0; slot < BT_MAX_SLOTS && w < (int) cap; slot++) {
        BtStatus ss;
        bt_get_status(slot, &ss);
        char hex[13] = "";
        if (ss.connected) mac_to_hex(ss.addr, hex);
        w += snprintf(out + w, cap - w,
                      "%s{\"connected\":%s,\"model\":\"%s\",\"addr\":\"%s\","
                      "\"battery_valid\":%s,\"battery_pct\":%u,\"charging\":%s}",
                      slot ? "," : "",
                      ss.connected ? "true" : "false",
                      ss.is_dse ? "DSE" : "DS5",
                      hex,
                      ss.battery_valid ? "true" : "false",
                      ss.battery_pct,
                      ss.charging ? "true" : "false");
    }
    if (w < (int) cap) w += snprintf(out + w, cap - w, "]}");
    return w;
}

// GET /api/resolve_mac -- reports the status of the most recent resolve
// kicked off by POST /api/resolve_mac. The POST only starts the ARP lookup
// (see web_api.h); the browser polls this GET afterward until it sees
// "pending":false. {"pending":true} while in flight, then either
// {"pending":false,"ok":true,"mac":"..."} or {"pending":false,"ok":false}.
static int json_resolve_mac(char *out, size_t cap) {
    uint8_t mac[6];
    const int r = web_api_resolve_mac_poll(mac);
    if (r == 0) return snprintf(out, cap, "{\"pending\":true}");
    if (r > 0) {
        char hex[13];
        mac_to_hex(mac, hex);
        return snprintf(out, cap, "{\"pending\":false,\"ok\":true,\"mac\":\"%s\"}", hex);
    }
    return snprintf(out, cap, "{\"pending\":false,\"ok\":false}");
}

#ifdef ENABLE_OTA
// GET /api/ota/status -- served in BOTH modes. During an update (OTA boot
// mode) it is the ONLY live route and carries the phase + byte progress;
// in normal mode it reports the persisted result of the last attempt so the
// page can show "updated to vX" / "checksum mismatch" after the reboot.
static int json_ota_status(char *out, size_t cap) {
    if (ota_mode_active()) {
        OtaStatus s;
        ota_get_status(&s);
        return snprintf(out, cap,
                        "{\"mode\":\"updating\",\"state\":\"%s\","
                        "\"bytes\":%lu,\"total\":%lu,\"tag\":\"%s\"}",
                        s.state, (unsigned long) s.bytes, (unsigned long) s.total,
                        s.tag);
    }
    return snprintf(out, cap,
                    "{\"mode\":\"idle\",\"last_result\":%lu,"
                    "\"last_result_str\":\"%s\",\"version\":\"%s\","
                    "\"asset\":\"%s\"}",
                    (unsigned long) ota_last_result(),
                    ota_result_str(ota_last_result()),
                    PICO_PROGRAM_VERSION_STRING, OTA_ASSET_NAME);
}
#endif

extern "C" int fs_open_custom(struct fs_file *file, const char *name) {
#ifdef ENABLE_OTA
    // OTA boot mode: BT/audio/USB state does not exist, so every route that
    // touches it must be unreachable. Serve ONLY the progress endpoint (the
    // config page that triggered the update keeps polling it); everything
    // else -- including "/" -- answers 503 until the device resets out of
    // OTA mode (~1 min).
    if (ota_mode_active()) {
        if (strcmp(name, "/api/ota/status") == 0) {
            static char obody[192];
            const int len = json_ota_status(obody, sizeof(obody));
            return make_file(file, "200 OK", "application/json", obody, len);
        }
        static const char busy[] = "firmware update in progress";
        return make_file(file, "503 Service Unavailable", "text/plain", busy,
                         sizeof(busy) - 1);
    }
#endif
#ifdef ENABLE_WIFI_WOL
    // Onboarding mode: serve the captive portal for essentially every GET.
    if (wifi_net_in_ap_mode()) {
        // W3: in AP mode the network is OPEN and BT is never initialized, so
        // only the onboarding routes may reach the shared handlers. Everything
        // else under /api/ (config, bonds -- forgetall really erases the TLV,
        // status, resolve_mac, pairing) is 404'd so a nearby actor who joins
        // DS5-Setup-XXXX can't rewrite config or wipe bonds. The portal page
        // itself only ever calls wifi_scan + wifi_provision(_result).
        const bool is_portal_api =
            (strcmp(name, "/api/wifi_scan") == 0) ||
            (strcmp(name, "/api/wifi_provision") == 0) ||
            (strcmp(name, "/api/wifi_provision_result") == 0);
        const bool other_api = (strncmp(name, "/api/", 5) == 0);
        if (other_api && !is_portal_api) {
            // A non-onboarding API GET in AP mode: reject.
            static const char nf[] = "not found";
            return make_file(file, "404 Not Found", "text/plain", nf, sizeof(nf) - 1);
        }
        if (!is_portal_api) {
            // Serve the portal page itself (200) for the root, the OS captive-probe
            // URLs (Windows /connecttest.txt + /index.shtml; Apple
            // /hotspot-detect.html; Android /generate_204; ...), AND any other GET.
            // We deliberately do NOT 302-redirect: redirecting probe URLs to
            // http://192.168.4.1/ on the SAME host made the captive mini-browser
            // re-request /index.shtml in a tight loop and never render. Returning
            // the page body directly for every path breaks that loop and makes the
            // "Sign in to network" sheet show the form immediately.
            return make_static_page(file, PORTAL_PAGE, (int)(sizeof(PORTAL_PAGE) - 1));
        }
    }
#endif
    if (strcmp(name, "/") == 0 || strcmp(name, "/index.html") == 0) {
        // Streamed from flash (no per-request body malloc). See make_static_page.
        return make_static_page(file, WEB_PAGE, (int)(sizeof(WEB_PAGE) - 1));
    }
    // Shared JSON scratch: make_file() copies the body into its own malloc'd
    // buffer before returning, and httpd serves one custom file at a time, so a
    // single static buffer is safe for all JSON routes (saves BSS -> heap).
    // 1024: /api/status now carries a per-slot array (~120 B x 4 slots on top
    // of the legacy fields), and /api/config grew the multi/weblog flags.
    static char body[1024];
#ifdef ENABLE_WIFI_WOL
    if (strcmp(name, "/api/wifi_scan") == 0) {
        // A scan is kicked off on first hit and on explicit ?start=1 (the portal's
        // rescan button); subsequent polls just report the latest list + whether a
        // scan is still running. (httpd strips the query string before matching,
        // so we can't see ?start here -- starting on every poll is harmless: the
        // scan-start is a no-op while one is already active.)
        wifi_scan_start();
        const int len = wifi_scan_json(body, sizeof(body));
        return make_file(file, "200 OK", "application/json", body, len);
    }
    if (strcmp(name, "/api/wifi_provision_result") == 0) {
        // Synthetic reply for the provision POST (see httpd_post_finished). Reports
        // whether the creds were accepted; the device reboots into STA shortly after.
        const int len = snprintf(body, sizeof(body), "{\"ok\":%s}",
                                 provision_ok ? "true" : "false");
        return make_file(file, "200 OK", "application/json", body, len);
    }
    if (strcmp(name, "/api/wifi_reset_result") == 0) {
        // Synthetic reply for POST /api/wifi_reset. If ok, the device has cleared
        // saved WLAN creds and will reboot into AP onboarding shortly.
        const int len = snprintf(body, sizeof(body), "{\"ok\":%s}",
                                 wifi_reset_ok ? "true" : "false");
        return make_file(file, "200 OK", "application/json", body, len);
    }
#endif
    if (strcmp(name, "/api/config") == 0) {
        const int len = json_config(body, sizeof(body));
        return make_file(file, "200 OK", "application/json", body, len);
    }
    if (strcmp(name, "/api/bonds") == 0) {
        const int len = json_bonds(body, sizeof(body));
        return make_file(file, "200 OK", "application/json", body, len);
    }
    if (strcmp(name, "/api/status") == 0) {
        const int len = json_status(body, sizeof(body));
        return make_file(file, "200 OK", "application/json", body, len);
    }
    if (strcmp(name, "/api/resolve_mac") == 0) {
        const int len = json_resolve_mac(body, sizeof(body));
        return make_file(file, "200 OK", "application/json", body, len);
    }
#ifdef ENABLE_OTA
    if (strcmp(name, "/api/ota/status") == 0) {
        const int len = json_ota_status(body, sizeof(body));
        return make_file(file, "200 OK", "application/json", body, len);
    }
    // Synthetic reply for POST /api/ota/start: the request was accepted and
    // the device reboots into OTA mode ~1.2 s after this response flushes.
    if (strcmp(name, "/api/ota/start_result") == 0) {
        static const char ok[] = "{\"ok\":true}";
        return make_file(file, "200 OK", "application/json", ok, sizeof(ok) - 1);
    }
#endif
    if (strcmp(name, "/api/log") == 0) {
        // Diagnostic log (RAM-only stdio mirror; see weblog.h). text/plain so
        // it renders directly in a browser tab for copy-paste bug reports.
        if (!weblog_enabled()) {
            static const char off[] =
                "diagnostic log disabled (enable it in Settings, reproduce, then reload)";
            return make_file(file, "200 OK", "text/plain; charset=utf-8", off, sizeof(off) - 1);
        }
        // Big enough for boot section (1K) + gap marker + ring (1K). Static:
        // make_file copies it, and httpd serves one custom file at a time.
        static char logbuf[2112];
        const int len = weblog_snapshot(logbuf, sizeof(logbuf));
        return make_file(file, "200 OK", "text/plain; charset=utf-8", logbuf, len);
    }
    // POST /api/config redirects here when config_save() failed to reach flash.
    // Returning a non-2xx status makes the page's `r.ok` check false so it shows
    // an error instead of "Saved ✓" for a change that never persisted.
    if (strcmp(name, "/api/save-failed") == 0) {
        static const char sf[] = "config save failed: flash not written";
        return make_file(file, "500 Internal Server Error", "text/plain", sf, sizeof(sf) - 1);
    }
    if (strcmp(name, "/404.html") == 0) {
        static const char nf[] = "not found";
        return make_file(file, "404 Not Found", "text/plain", nf, sizeof(nf) - 1);
    }
    return 0;
}

extern "C" void fs_close_custom(struct fs_file *file) {
    // JSON responses (make_file) own a malloc'd file->data; free it. The static
    // page (make_static_page) sets data=NULL and streams from flash, so there's
    // nothing to free for it.
    if (file && file->data) {
        free(const_cast<char *>(file->data));
        file->data = NULL;
    }
}

// Streams the static config page when httpd pulls bytes (data==NULL + CUSTOM).
// file->index 0..PAGE_HDR_LEN-1 -> the response header; beyond that ->
// WEB_PAGE[index - PAGE_HDR_LEN]. Copies up to `count` (~MSS) per call into the
// per-connection buffer; never allocates the whole page. Only the page uses this
// path -- JSON/404 responses carry their bytes in file->data (FS_READ_EOF here).
extern "C" int fs_read_custom(struct fs_file *file, char *buffer, int count) {
    if (!file || file->data != NULL) {
        return FS_READ_EOF; // in-memory response (JSON/404): nothing to stream
    }
    int remaining = file->len - file->index;
    if (remaining <= 0) return FS_READ_EOF;
    int n = (count < remaining) ? count : remaining;

    int written = 0;
    // Header portion first.
    if (file->index < PAGE_HDR_LEN) {
        int hdr_avail = PAGE_HDR_LEN - file->index;
        int hdr_n = (n < hdr_avail) ? n : hdr_avail;
        memcpy(buffer, PAGE_HDR + file->index, hdr_n);
        written += hdr_n;
    }
    // Then the flash body (may continue within the same call once header drains).
    if (written < n) {
        int body_pos = (file->index + written) - PAGE_HDR_LEN; // >= 0 here
        int body_n = n - written;
        memcpy(buffer + written, cur_page_body + body_pos, body_n);
        written += body_n;
    }
    file->index += written;
    return written;
}

//--------------------------------------------------------------------+
// POST handling: /api/config, /api/bonds, /api/controllers, /api/wol
//--------------------------------------------------------------------+

#define POST_BUFSIZE 512
static char post_buf[POST_BUFSIZE];
static u16_t post_pos;
static void *post_conn;
// Declared Content-Length of the in-flight POST (-1 if the client didn't send
// one). Used in httpd_post_finished to distinguish a complete body from an
// aborted connection so a partial body isn't applied + flash-saved (W4).
static int post_content_len;
static bool last_save_ok = true; // result of the most recent config_save()

// Which endpoint the in-flight POST targets.
enum PostTarget {
    POST_CONFIG,
    POST_BONDS,
    POST_CONTROLLERS,
    POST_WOL,
    POST_RESOLVE_MAC,
    POST_WIFI_PROVISION,
    POST_WIFI_RESET,
    POST_OTA_START
};
static PostTarget post_target;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// In-place URL-decode (%XX and '+' -> space) of a form field value.
static void url_decode(char *s) {
    char *w = s;
    auto nib = [](char x) -> int {
        if (x >= '0' && x <= '9') return x - '0';
        if (x >= 'a' && x <= 'f') return x - 'a' + 10;
        if (x >= 'A' && x <= 'F') return x - 'A' + 10;
        return -1;
    };
    for (char *r = s; *r; r++) {
        if (*r == '+') {
            *w++ = ' ';
        } else if (*r == '%' && r[1] && r[2]) {
            int hi = nib(r[1]), lo = nib(r[2]);
            if (hi >= 0 && lo >= 0) { *w++ = (char) ((hi << 4) | lo); r += 2; }
            else *w++ = *r;
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

static void apply_post(char *body) {
    // Deliberate settings wipe. Resets Config to defaults and persists; leaves
    // BT bonds untouched (forgetting controllers is the /api/bonds "forgetall"
    // action). This is the sanctioned reset path -- CONFIG_VERSION is layout
    // metadata, not a reset knob.
    if (strstr(body, "factory_reset=1")) {
        watchdog_update();
        last_save_ok = config_factory_reset();
        printf("[WEB] factory reset via web UI: %s\n", last_save_ok ? "OK" : "FAILED");
        // Defaults zero the wake-keyboard toggle; drop the kbd from the
        // enumeration too (no-op if it was already off).
        usb_request_wake_kbd(get_config().wake_kbd_enabled != 0);
        return;
    }

    Config_body c = get_config(); // start from current, overwrite parsed fields

    for (char *tok = strtok(body, "&"); tok; tok = strtok(nullptr, "&")) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq++ = 0;
        const int val = atoi(eq);
        if (strcmp(tok, "inactive_time") == 0) {
            c.inactive_time = (uint8_t) clampi(val, 5, 60);
        } else if (strcmp(tok, "disable_inactive_disconnect") == 0) {
            c.disable_inactive_disconnect = val ? 1 : 0;
        } else if (strcmp(tok, "disable_pico_led") == 0) {
            c.disable_pico_led = val ? 1 : 0;
        } else if (strcmp(tok, "polling_rate_mode") == 0) {
            c.polling_rate_mode = (uint8_t) clampi(val, 0, 2);
        } else if (strcmp(tok, "audio_buffer_length") == 0) {
            c.audio_buffer_length = (uint8_t) clampi(val, 16, 128);
        } else if (strcmp(tok, "controller_mode") == 0) {
            c.controller_mode = (uint8_t) clampi(val, 0, 2);
        } else if (strcmp(tok, "wol_target_mac") == 0) {
            // 12 hex chars, no separators (the page strips ':'/'-' client-side).
            // Reject anything malformed so a bad POST can't store a junk MAC; an
            // all-zero MAC is the canonical "unset" and is allowed (clears it).
            uint8_t mac[6];
            if (hex_to_addr(eq, mac)) memcpy(c.wol_target_mac, mac, 6);
        } else if (strcmp(tok, "wol_target_mac2") == 0) {
            // Second WOL target (e.g. a TV). Same parse/unset convention as #1.
            uint8_t mac[6];
            if (hex_to_addr(eq, mac)) memcpy(c.wol_target_mac2, mac, 6);
        } else if (strcmp(tok, "wake_kbd_enabled") == 0) {
            c.wake_kbd_enabled = val ? 1 : 0;
        } else if (strcmp(tok, "multi_allowed") == 0) {
            c.multi_enabled = val ? 1 : 0;
        } else if (strcmp(tok, "weblog_enabled") == 0) {
            c.weblog_enabled = val ? 1 : 0;
        } else if (strcmp(tok, "hostname") == 0) {
            // mDNS / netif hostname. url_decode then copy raw; set_config() ->
            // config_valid() -> sanitize_hostname() folds case and strips any
            // non-DNS-label chars (and re-defaults if empty), so we don't filter
            // here. Takes effect on next boot (the netif/mDNS name is set at init).
            url_decode(eq);
            strncpy(c.hostname, eq, CONFIG_HOSTNAME_LEN - 1);
            c.hostname[CONFIG_HOSTNAME_LEN - 1] = '\0';
        }
    }

    set_config(c); // validates + stores in RAM
    // The sector erase blocks with interrupts off; feed the watchdog first.
    watchdog_update();
    // config_save() can fail (core1 won't park -> flash write skipped). If it
    // does, RAM holds the new values but flash does not, so the change would
    // silently vanish on the next boot. Record the result so the response can
    // tell the user instead of falsely reporting success.
    last_save_ok = config_save();
    printf("[WEB] config save via web UI: %s\n", last_save_ok ? "OK" : "FAILED");

    // Live-apply the wake-keyboard toggle: request the descriptor bounce with
    // the validated in-RAM value (mirrors how the other settings apply from
    // RAM even if the flash save failed). A no-op when the enumerated state
    // already matches -- i.e. for every POST that didn't change the toggle.
    usb_request_wake_kbd(get_config().wake_kbd_enabled != 0);

#ifdef ENABLE_WIFI_WOL
    // Live-apply the diagnostic-log toggle (same RAM-value pattern). Turning
    // multi_allowed OFF is deliberately admissions-only: connected pads stay
    // up (a settings save must never kill live gameplay); the connection
    // filter enforces capacity 1 for NEW connections from now on.
    weblog_set_enabled(get_config().weblog_enabled != 0);
#endif
}

static void apply_bonds_post(char *body) {
    char action[16] = "";
    char addr_hex[16] = "";
    char name[CONFIG_BOND_NAME_LEN] = "";
    last_save_ok = true; // actions that don't persist (e.g. pair) leave this true

    for (char *tok = strtok(body, "&"); tok; tok = strtok(nullptr, "&")) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq++ = 0;
        if (strcmp(tok, "action") == 0) {
            strncpy(action, eq, sizeof(action) - 1);
        } else if (strcmp(tok, "addr") == 0) {
            strncpy(addr_hex, eq, sizeof(addr_hex) - 1);
        } else if (strcmp(tok, "name") == 0) {
            url_decode(eq);
            strncpy(name, eq, sizeof(name) - 1);
        }
    }

    if (strcmp(action, "pair") == 0) {
        // Can fail: every bond seat used, or (multi) all slots connected.
        // Surface the rejection through the existing save-failed path so the
        // page shows an error instead of a pairing window that never opened.
        last_save_ok = bt_start_pairing();
        printf("[WEB] start pairing (open inquiry) via web UI: %s\n",
               last_save_ok ? "OK" : "REJECTED");
        return;
    }

    if (strcmp(action, "forgetall") == 0) {
        bt_bond_forget_all();
        Config_body c = get_config();
        memset(c.bond_names, 0, sizeof(c.bond_names));
        set_config(c);
        watchdog_update();
        last_save_ok = config_save();
        printf("[WEB] forget all bonds via web UI: %s\n", last_save_ok ? "OK" : "FAILED");
        return;
    }

    uint8_t addr[6];
    if (!hex_to_addr(addr_hex, addr)) {
        printf("[WEB] bonds POST: bad addr '%s'\n", addr_hex);
        return;
    }

    if (strcmp(action, "forget") == 0) {
        bt_bond_forget(addr);
        config_clear_bond_name(addr);
        watchdog_update();
        last_save_ok = config_save();
        printf("[WEB] forget bond via web UI: %s\n", last_save_ok ? "OK" : "FAILED");
    } else if (strcmp(action, "rename") == 0) {
        config_set_bond_name(addr, name);
        watchdog_update();
        last_save_ok = config_save();
        printf("[WEB] rename bond via web UI: %s\n", last_save_ok ? "OK" : "FAILED");
    }
}

// POST /api/controllers -- action=poweroffall. Sends the existing DualSense
// power-off feature report to every live slot. Bonds are kept, so each pad can
// reconnect normally with its PS button.
static bool apply_controllers_post(char *body) {
    if (strcmp(body, "action=poweroffall") != 0) {
        printf("[WEB] controllers POST: unknown action\n");
        return false;
    }

    bt_dualsense_power_off();
    printf("[WEB] power off all connected controllers via web UI\n");
    return true;
}

// POST /api/wol -- action=wake[&mac=AABBCCDDEEFF]. With an explicit mac, wakes
// exactly that target. With no mac (the page's "Wake now" button), fires EVERY
// stored target (wol_target_mac + wol_target_mac2) via web_api_wol_send_all().
// Both routes are no-ops unless the WiFi transport is linked.
static void apply_wol_post(char *body) {
    char action[16] = "";
    uint8_t mac[6];
    bool have_mac = false;

    for (char *tok = strtok(body, "&"); tok; tok = strtok(nullptr, "&")) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq++ = 0;
        if (strcmp(tok, "action") == 0) {
            strncpy(action, eq, sizeof(action) - 1);
        } else if (strcmp(tok, "mac") == 0) {
            if (hex_to_addr(eq, mac)) have_mac = true;
        }
    }

    if (strcmp(action, "wake") != 0) return;

    if (have_mac) {
        const bool sent = web_api_wol_send(mac);
        printf("[WEB] WOL %s via web UI\n", sent ? "sent" : "send failed (no transport)");
        return;
    }

    // No explicit MAC: wake every configured target. send_all() already skips
    // unset (all-zero) targets and returns false if none were configured.
    const bool sent = web_api_wol_send_all();
    if (!sent) printf("[WEB] WOL requested but no target MAC configured (or no transport)\n");
    else printf("[WEB] WOL sent to all stored targets via web UI\n");
}

// POST /api/resolve_mac -- ip=A.B.C.D. Kicks off an ARP lookup for that address
// (the WiFi transport's resolve-start hook) so the UI can auto-fill the WOL
// target MAC instead of the user hunting it down by hand. Non-blocking: this
// only starts the lookup. The browser polls GET /api/resolve_mac afterward for
// the result (json_resolve_mac() above) once the transport's main-loop task
// has had a chance to drive the ARP query/cache-check/timeout -- see
// wifi_net.cpp for why this can't just block here.
static void apply_resolve_mac_post(char *body) {
    unsigned a = 0, b = 0, cc = 0, d = 0;
    char *eq = strchr(body, '=');
    const bool parsed = eq &&
             sscanf(eq + 1, "%u.%u.%u.%u", &a, &b, &cc, &d) == 4 &&
             a <= 255 && b <= 255 && cc <= 255 && d <= 255;
    if (!parsed) {
        printf("[WEB] resolve_mac POST: bad ip\n");
        return;
    }
    const uint8_t ip[4] = {(uint8_t) a, (uint8_t) b, (uint8_t) cc, (uint8_t) d};
    web_api_resolve_mac_start(ip);
    printf("[WEB] resolving %u.%u.%u.%u...\n", a, b, cc, d);
}

#ifdef ENABLE_WIFI_WOL
// POST /api/wifi_provision -- ssid=...&psk=...&auth=wpa2|wpa3 from the
// onboarding portal. Saves the home-WLAN credentials/security choice and
// schedules a reboot into STA mode (wifi_net.cpp owns the persist + deferred
// reset). Omitting auth remains WPA2 for compatibility with older clients.
// The JSON reply is sent before the reboot fires so the phone sees success.
static void apply_wifi_provision_post(char *body) {
    char ssid[CONFIG_WIFI_SSID_LEN] = "";
    char psk[CONFIG_WIFI_PSK_LEN] = "";
    uint8_t auth_mode = CONFIG_WIFI_AUTH_WPA2;
    bool auth_valid = true;
    bool too_long = false;
    for (char *tok = strtok(body, "&"); tok; tok = strtok(nullptr, "&")) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq++ = 0;
        if (strcmp(tok, "ssid") == 0) {
            url_decode(eq);
            if (strlen(eq) >= sizeof(ssid)) {
                too_long = true;
            } else {
                strcpy(ssid, eq);
            }
        } else if (strcmp(tok, "psk") == 0) {
            url_decode(eq);
            if (strlen(eq) >= sizeof(psk)) {
                too_long = true;
            } else {
                strcpy(psk, eq);
            }
        } else if (strcmp(tok, "auth") == 0) {
            url_decode(eq);
            if (strcmp(eq, "wpa2") == 0) {
                auth_mode = CONFIG_WIFI_AUTH_WPA2;
            } else if (strcmp(eq, "wpa3") == 0) {
                auth_mode = CONFIG_WIFI_AUTH_WPA3;
            } else {
                auth_valid = false;
            }
        }
    }
    provision_ok = !too_long && auth_valid &&
                   wifi_provision_apply(ssid, psk, auth_mode);
    printf("[WEB] wifi provision %s\n", provision_ok ? "accepted" : "rejected");
}

// POST /api/wifi_reset -- from the normal config page after the device is on
// the home WLAN. Clears stored WiFi credentials and schedules a reboot; the next
// boot is unprovisioned, so it starts the AP captive portal.
static void apply_wifi_reset_post(void) {
    wifi_reset_ok = wifi_reset_provisioning_apply();
    printf("[WEB] wifi reset %s\n", wifi_reset_ok ? "accepted" : "rejected");
}
#endif

extern "C" err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                                  u16_t http_request_len, int content_len, char *response_uri,
                                  u16_t response_uri_len, u8_t *post_auto_wnd) {
    (void) http_request;
    (void) http_request_len;
    (void) response_uri;
    (void) response_uri_len;
    (void) post_auto_wnd;
#ifdef ENABLE_OTA
    // OTA boot mode is read-only: /api/ota/status is the only route alive.
    if (ota_mode_active()) return ERR_VAL;
#endif
    PostTarget t;
    if (strcmp(uri, "/api/config") == 0)      t = POST_CONFIG;
    else if (strcmp(uri, "/api/bonds") == 0)  t = POST_BONDS;
    else if (strcmp(uri, "/api/controllers") == 0) t = POST_CONTROLLERS;
    else if (strcmp(uri, "/api/wol") == 0)    t = POST_WOL;
    else if (strcmp(uri, "/api/resolve_mac") == 0) t = POST_RESOLVE_MAC;
#ifdef ENABLE_WIFI_WOL
    else if (strcmp(uri, "/api/wifi_provision") == 0) t = POST_WIFI_PROVISION;
    else if (strcmp(uri, "/api/wifi_reset") == 0) t = POST_WIFI_RESET;
#endif
#ifdef ENABLE_OTA
    else if (strcmp(uri, "/api/ota/start") == 0) t = POST_OTA_START;
#endif
    else return ERR_VAL;
#ifdef ENABLE_WIFI_WOL
    // W3: in AP onboarding mode reject every POST except the provision flow.
    // The open AP + uninitialized BT means POST /api/bonds action=forgetall
    // (gap_delete_all_link_keys erases the TLV even with BT down) or
    // POST /api/config (rewrites+persists settings) must not be reachable.
    if (wifi_net_in_ap_mode() && t != POST_WIFI_PROVISION && t != POST_WIFI_RESET) {
        return ERR_VAL;
    }
#endif
    if (content_len >= POST_BUFSIZE) return ERR_VAL;
    if (post_conn) return ERR_USE; // one POST at a time
    post_conn = connection;
    post_pos = 0;
    // Terminate now: a body-less POST never runs httpd_post_receive_data, and
    // the finished handler must not strstr() a previous request's leftovers.
    post_buf[0] = 0;
    post_target = t;
    post_content_len = content_len; // -1 when the client sent no Content-Length
    return ERR_OK;
}

extern "C" err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    if (connection == post_conn && p) {
        const u16_t space = POST_BUFSIZE - 1 - post_pos;
        const u16_t take = p->tot_len < space ? p->tot_len : space;
        post_pos += pbuf_copy_partial(p, post_buf + post_pos, take, 0);
        post_buf[post_pos] = 0;
    }
    if (p) pbuf_free(p);
    return ERR_OK;
}

extern "C" void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    if (connection != post_conn) return;
    post_conn = nullptr;

    // lwIP httpd also calls this when a POST connection dies mid-body. If the
    // client declared a Content-Length and we didn't receive all of it, the
    // body is partial -- applying it can persist a silently-wrong value and
    // burn a flash erase/program cycle for a request the client never
    // finished. Reject without applying. (content_len == -1 means the client
    // sent no length, so we can't tell; fall through to the old behavior.)
    if (post_content_len >= 0 && post_pos != (u16_t)post_content_len) {
        snprintf(response_uri, response_uri_len, "/api/save-failed");
        return;
    }

    switch (post_target) {
        case POST_BONDS:
            apply_bonds_post(post_buf);
            snprintf(response_uri, response_uri_len,
                     last_save_ok ? "/api/bonds" : "/api/save-failed");
            break;
        case POST_CONTROLLERS:
            snprintf(response_uri, response_uri_len,
                     apply_controllers_post(post_buf) ? "/api/status" : "/404.html");
            break;
        case POST_WOL:
            apply_wol_post(post_buf);
            snprintf(response_uri, response_uri_len, "/api/config");
            break;
        case POST_RESOLVE_MAC:
            apply_resolve_mac_post(post_buf);
            snprintf(response_uri, response_uri_len, "/api/resolve_mac");
            break;
#ifdef ENABLE_WIFI_WOL
        case POST_WIFI_PROVISION:
            apply_wifi_provision_post(post_buf);
            // The reply is served from the synthetic route below, which reports
            // provision_ok set just above. (The reboot is deferred ~1.2s by
            // wifi_net.cpp so this response reaches the phone first.)
            snprintf(response_uri, response_uri_len, "/api/wifi_provision_result");
            break;
        case POST_WIFI_RESET:
            apply_wifi_reset_post();
            snprintf(response_uri, response_uri_len, "/api/wifi_reset_result");
            break;
#endif
#ifdef ENABLE_OTA
        case POST_OTA_START:
            // body: "force=1" reinstalls even when already on the latest tag;
            // "beta=1" resolves the newest release INCLUDING prereleases.
            // The version check itself happens IN OTA mode (the normal runtime
            // has no heap for TLS) -- this just arms the request + reboots.
            ota_request_and_reboot(strstr(post_buf, "force=1") != nullptr,
                                   strstr(post_buf, "beta=1") != nullptr);
            snprintf(response_uri, response_uri_len, "/api/ota/start_result");
            break;
#endif
        case POST_CONFIG:
        default:
            apply_post(post_buf);
            snprintf(response_uri, response_uri_len,
                     last_save_ok ? "/api/config" : "/api/save-failed");
            break;
    }
}

void web_api_init() {
    httpd_init();
}

#endif // ENABLE_WEBUI
