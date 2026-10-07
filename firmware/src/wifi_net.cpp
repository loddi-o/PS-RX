//
// wifi_net.cpp -- onboard CYW43 Wi-Fi (STA) transport: config web UI + WOL.
//
// The single CYW43 radio carries BT (Opus audio, latency-critical) AND WiFi
// (config/WOL TCP) at once -- radio contention with audio is the thing to watch
// when touching this file. (This started as a spike to answer exactly that
// question; an external W5500 Ethernet transport existed first and was retired
// once WiFi proved out without the duplex-audio stutter.)
//
// STRUCTURE:
//   - No manual netif. The SDK's cyw43_arch (CYW43_LWIP=1) creates and owns the
//     lwIP netif for the WiFi interface, runs its RX into lwIP, and is pumped by
//     cyw43_arch_poll() already in the main loop. We just join the WLAN and
//     start DHCP.
//   - lwIP is initialised by cyw43_arch_init() (lwip_nosys_init -> lwip_init),
//     so we MUST NOT call lwip_init() here, or lwIP double-inits.
//   - The ARP resolve must be a start/poll split serviced from the main loop,
//     never blocked inside the httpd POST callback (see the resolve section).
//

#include "wifi_net.h"

#ifdef ENABLE_WIFI_WOL

#include <cstdio>
#include <cstring>

#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "pico/unique_id.h"

#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/udp.h"
#include "lwip/apps/mdns.h"

#include "dhcpserver.h"
#include "dnsserver.h"

#include "bootsel_button.h"
#include "bt.h"
#include "config.h"
#include "web_api.h"

// Home-WLAN credentials now live in flash (Config_body.wifi_ssid/psk), filled by
// the onboarding captive portal -- the gitignored wifi_secrets.h spike shortcut
// is gone. With no creds saved (wifi_provisioned == 0) the device comes up in AP
// mode and serves the portal; with creds it joins as STA. See wifi_net_init().

//--------------------------------------------------------------------+
// Wake-on-LAN: 102-byte magic packet broadcast as UDP to 255.255.255.255:9.
// lwIP frames ethernet/IP/UDP; the cyw43 netif carries it out the radio.
//--------------------------------------------------------------------+

bool wifi_wol_send(const uint8_t mac[6]) {
    uint8_t magic[102];
    memset(magic, 0xFF, 6);
    for (int i = 0; i < 16; i++) memcpy(magic + 6 + i * 6, mac, 6);

    struct udp_pcb *pcb = udp_new();
    if (!pcb) return false;
    ip_set_option(pcb, SOF_BROADCAST);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(magic), PBUF_RAM);
    if (!p) { udp_remove(pcb); return false; }
    memcpy(p->payload, magic, sizeof(magic));

    ip_addr_t bcast;
    IP4_ADDR(&bcast, 255, 255, 255, 255);
    const err_t e = udp_sendto(pcb, p, &bcast, 9);

    pbuf_free(p);
    udp_remove(pcb);
    return e == ERR_OK;
}

// Strong override of the shared web_api WOL hook (weak no-op in web_api.cpp).
extern "C" bool web_api_wol_send_impl(const uint8_t mac[6]) {
    return wifi_wol_send(mac);
}

static bool mac_is_zero(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) if (mac[i]) return false;
    return true;
}

// Fire a magic packet at every configured (non-zero) WOL target -- currently
// wol_target_mac (the PC) and wol_target_mac2 (e.g. a TV). Shared by the wake
// companion (wake_emit_wol) and the web UI's "Wake now" so both wake ALL stored
// targets. Returns true if at least one packet was sent. Skips silently in AP
// onboarding mode (no LAN uplink -- the caller usually gates this too).
bool wifi_wol_send_all(void) {
    if (wifi_net_in_ap_mode()) return false;
    const Config_body &c = get_config();
    const uint8_t *targets[2] = { c.wol_target_mac, c.wol_target_mac2 };
    bool any = false;
    for (const uint8_t *mac : targets) {
        if (mac_is_zero(mac)) continue;
        const bool sent = wifi_wol_send(mac);
        printf("[wifi] WOL %02X:%02X:%02X:%02X:%02X:%02X %s\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
               sent ? "sent" : "send failed");
        any = any || sent;
    }
    return any;
}

// Strong override of the shared web_api "wake all stored targets" hook (weak
// no-op in web_api.cpp): the manual "Wake now" button with no explicit MAC.
extern "C" bool web_api_wol_send_all_impl(void) {
    return wifi_wol_send_all();
}

//--------------------------------------------------------------------+
// Non-blocking ARP resolve (IP -> MAC) for the web UI's "Find MAC". The POST
// callback only records the target; wifi_net_task() drives the cache-check/
// query/timeout on the main loop. The split is mandatory: the POST callback
// runs nested inside lwIP's own tcp_input()/timeout processing, so pumping the
// stack from in there to wait for the ARP reply would re-enter non-reentrant
// lwIP internals and corrupt the in-flight connection.
//--------------------------------------------------------------------+

#define ARP_RESOLVE_BUDGET_MS 600

enum class ResolveState { IDLE, PENDING, DONE_OK, DONE_FAIL };
static ResolveState resolve_state = ResolveState::IDLE;
static ip4_addr_t resolve_target;
static uint8_t resolve_mac_out[6];
static bool resolve_queried = false;
static absolute_time_t resolve_deadline;

void wifi_resolve_mac_start(const uint8_t ip[4]) {
    IP4_ADDR(&resolve_target, ip[0], ip[1], ip[2], ip[3]);
    resolve_queried = false;
    resolve_deadline = make_timeout_time_ms(ARP_RESOLVE_BUDGET_MS);
    resolve_state = ResolveState::PENDING;
}

static void wifi_resolve_poll(void) {
    if (resolve_state != ResolveState::PENDING) return;
    struct netif *nif = netif_default;
    if (!nif) { resolve_state = ResolveState::DONE_FAIL; return; }
    struct eth_addr *eth = nullptr;
    const ip4_addr_t *found = nullptr;
    if (etharp_find_addr(nif, &resolve_target, &eth, &found) >= 0 && eth) {
        memcpy(resolve_mac_out, eth->addr, 6);
        resolve_state = ResolveState::DONE_OK;
        return;
    }
    if (!resolve_queried) {
        etharp_query(nif, &resolve_target, nullptr);
        resolve_queried = true;
    }
    if (time_reached(resolve_deadline)) {
        resolve_state = ResolveState::DONE_FAIL;
    }
}

int wifi_resolve_mac_poll_result(uint8_t out_mac[6]) {
    switch (resolve_state) {
        case ResolveState::DONE_OK:
            memcpy(out_mac, resolve_mac_out, 6);
            return 1;
        case ResolveState::PENDING:
            return 0;
        default:
            return -1;
    }
}

extern "C" void web_api_resolve_mac_start_impl(const uint8_t ip[4]) {
    wifi_resolve_mac_start(ip);
}
extern "C" int web_api_resolve_mac_poll_impl(uint8_t out_mac[6]) {
    return wifi_resolve_mac_poll_result(out_mac);
}

// Wake companion hook, called from wake.cpp's request_host_wake() whenever a
// genuine host wake is warranted. wake.cpp owns WHEN (and the once-per-spell
// rate-limit); this just emits to the configured target. Returns true if a
// packet was sent.
extern "C" bool wake_emit_wol(void) {
    // No LAN in onboarding mode -- the SoftAP carries only the local portal, so
    // a magic packet has nowhere to go. (wifi_wol_send_all() re-checks this too.)
    if (wifi_net_in_ap_mode()) return false;
    // Wake every configured target (PC + optional 2nd, e.g. a TV).
    return wifi_wol_send_all();
}

//--------------------------------------------------------------------+
// Mode + state
//--------------------------------------------------------------------+

// Two mutually-exclusive runtime modes (never both -- a device is either
// onboarding or operating):
//   STA: provisioned, joined the home WLAN, config page + WOL (normal use).
//   AP : unprovisioned (or BOOTSEL-forced), SoftAP + captive portal (onboarding).
static bool in_ap_mode = false;
static bool force_ap = false;          // set by wifi_net_request_ap_onboarding()
static bool wifi_mdns_added = false;   // STA: mDNS netif registered once

// STA retry state. The first join is kicked off immediately by wifi_sta_init();
// each completed failure advances through this table, capped at one minute.
static constexpr uint32_t STA_RETRY_DELAYS_MS[] = {
    5'000, 15'000, 30'000, 60'000
};
static constexpr uint32_t STA_CONTROLLER_RETRY_MIN_MS = 60'000;
static constexpr uint32_t STA_JOIN_STATUS_GRACE_MS = 30'000;
static constexpr uint32_t STA_BADAUTH_RETRY_MS = 5'000;
static constexpr unsigned STA_BADAUTH_LIMIT = 3;
static unsigned sta_retry_stage = 0;
static unsigned sta_badauth_failures = 0;
static bool sta_retry_scheduled = false;
static bool sta_retry_for_badauth = false;
static bool sta_failure_latched = false;
static bool sta_join_kickoff_pending = false;
static absolute_time_t sta_retry_at = {0};
static absolute_time_t sta_last_attempt = {0};
static absolute_time_t sta_join_status_deadline = {0};
static bool sta_attempt_recorded = false;

//--------------------------------------------------------------------+
// Explicit BOOTSEL -> one-shot AP boot
//--------------------------------------------------------------------+
// scratch[0] carries the request across watchdog_reboot(). OTA owns scratch
// [2..3], while scratch[4..7] belong to the SDK reboot-vector protocol.
// Consuming and clearing the magic before AP startup makes this a one-shot:
// if the user does not save new credentials, a later reboot retries the old
// STA network rather than becoming permanently stuck in onboarding.
static constexpr uint32_t WIFI_AP_BOOT_MAGIC = 0x44533541u; // ASCII "DS5A"
static constexpr uint32_t BOOTSEL_POLL_MS = 50;
static constexpr int BOOTSEL_DEBOUNCE_SAMPLES = 2;          // 2 x 50 ms = 100 ms
static constexpr int BOOTSEL_REQUIRED_CLICKS = 3;
static constexpr uint32_t BOOTSEL_SEQUENCE_TIMEOUT_MS = 4'000;

static void wifi_capture_ap_boot_request() {
    if (watchdog_hw->scratch[0] != WIFI_AP_BOOT_MAGIC) return;
    watchdog_hw->scratch[0] = 0;
    wifi_net_request_ap_onboarding();
    printf("[wifi] BOOTSEL request consumed -> AP onboarding for this boot\n");
}

// Polling QSPI CS can disturb XIP on the other core, so the safe reader parks
// core 1 and this gesture is disabled whenever any controller has an ACL link.
// Thus normal gameplay/audio never pays for a BOOTSEL sample. To re-onboard,
// power every controller off and click BOOTSEL three times. Each edge must be
// stable for 100 ms and all three completed clicks must fit within four seconds.
// Rebooting only after the third debounced RELEASE avoids the ROM USB flashing
// mode on both RP2040 and RP2350.
static void wifi_bootsel_onboarding_task() {
    static absolute_time_t next_poll = {0};
    static bool stable_pressed = false;
    static bool candidate_pressed = false;
    static int candidate_samples = 0;
    static int completed_clicks = 0;
    static absolute_time_t sequence_deadline = {0};
    if (!time_reached(next_poll)) return;
    next_poll = make_timeout_time_ms(BOOTSEL_POLL_MS);

    if (bt_connected_count() != 0) {
        stable_pressed = false;
        candidate_pressed = false;
        candidate_samples = 0;
        completed_clicks = 0;
        return;
    }

    const bool pressed = bootsel_button_pressed();
    if (completed_clicks != 0 && time_reached(sequence_deadline)) {
        completed_clicks = 0;
        printf("[wifi] BOOTSEL click sequence timed out\n");
    }

    if (pressed == stable_pressed) {
        candidate_samples = 0;
        return;
    }

    if (pressed != candidate_pressed) {
        candidate_pressed = pressed;
        candidate_samples = 1;
        return;
    }
    if (++candidate_samples < BOOTSEL_DEBOUNCE_SAMPLES) return;

    stable_pressed = pressed;
    candidate_samples = 0;
    if (stable_pressed) return; // count only complete press-release cycles

    if (completed_clicks == 0) {
        sequence_deadline = make_timeout_time_ms(BOOTSEL_SEQUENCE_TIMEOUT_MS);
    }
    completed_clicks++;
    printf("[wifi] BOOTSEL click %d/%d\n",
           completed_clicks, BOOTSEL_REQUIRED_CLICKS);
    if (completed_clicks < BOOTSEL_REQUIRED_CLICKS) return;

    watchdog_hw->scratch[0] = WIFI_AP_BOOT_MAGIC;
    printf("[wifi] BOOTSEL triple-click -> rebooting to one-shot AP onboarding\n");
    watchdog_update();
    sleep_ms(20); // allow the UART message to drain
    watchdog_reboot(0, 0, 0);
}

//--------------------------------------------------------------------+
// SoftAP status LED -- continuous 2 Hz, 50% duty
//--------------------------------------------------------------------+

static constexpr uint32_t AP_LED_TOGGLE_MS = 250;
static absolute_time_t ap_led_next_toggle = {0};
static bool ap_led_state = false;

static void wifi_ap_led_task() {
    if (!time_reached(ap_led_next_toggle)) return;
    ap_led_state = !ap_led_state;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, ap_led_state);
    ap_led_next_toggle = make_timeout_time_ms(AP_LED_TOGGLE_MS);
}

// AP-mode IP plan: network 10.55.55.104/29, dongle (gateway) at 10.55.55.105,
// DHCP hands clients .106-.110 (see dhcpserver.h DHCPS_BASE_IP/DHCPS_MAX_IP,
// tuned to this /29). The /29 caps the pool at 5 client slots so nobody can
// cram a crowd of stations onto the single radio and starve BT/audio. The DNS
// server answers every lookup with .105 so the captive-portal sheet pops on the
// phone.
#define AP_GW_A 10
#define AP_GW_B 55
#define AP_GW_C 55
#define AP_GW_D 105

static dhcp_server_t ap_dhcp;
static dns_server_t  ap_dns;

bool wifi_net_in_ap_mode() { return in_ap_mode; }
void wifi_net_request_ap_onboarding() { force_ap = true; }

//--------------------------------------------------------------------+
// STA: join the home WLAN (non-blocking)
//--------------------------------------------------------------------+

// Kick off a join WITHOUT blocking. The earlier blocking
// cyw43_arch_wifi_connect_timeout_ms stalled main() for up to ~20s when the
// 2.4GHz join was flaky (observed ~13s) -- that boot-time stall starved BT/USB
// bring-up and destabilized DualSense enumeration (DS5 linked over BT but the
// host never enumerated it). The async variant returns immediately; the join
// progresses in the cyw43 poll context (pumped by cyw43_arch_poll() in the main
// loop) and wifi_net_task() watches cyw43_tcpip_link_status to detect success,
// failure, and when to retry. So WiFi can never block the audio/USB path.
static int wifi_start_join(void) {
    const Config_body &c = get_config();
    // The SDK scanner only distinguishes open from RSN-secured networks; it
    // cannot tell WPA2 from WPA3. Onboarding therefore persists the user's
    // explicit choice. Keep WPA2_MIXED for legacy WPA2-AES/WPA2-mixed routers
    // and use SAE only when WPA3 was selected. An empty PSK is an open network.
    const bool open = c.wifi_psk[0] == '\0';
    const bool wpa3 = !open && c.wifi_auth_mode == CONFIG_WIFI_AUTH_WPA3;
    const uint32_t auth = open ? CYW43_AUTH_OPEN
                              : wpa3 ? CYW43_AUTH_WPA3_SAE_AES_PSK
                                     : CYW43_AUTH_WPA2_MIXED_PSK;
    printf("[wifi] connecting to SSID \"%s\" using %s (async)...\n",
           c.wifi_ssid, open ? "OPEN" : wpa3 ? "WPA3-SAE" : "WPA2");
    sta_last_attempt = get_absolute_time();
    sta_attempt_recorded = true;
    const int rc = cyw43_arch_wifi_connect_async(
        c.wifi_ssid, c.wifi_psk[0] ? c.wifi_psk : NULL, auth);
    if (rc) printf("[wifi] connect kickoff failed (rc=%d); will retry\n", rc);
    sta_join_kickoff_pending = rc == 0;
    sta_join_status_deadline = make_timeout_time_ms(STA_JOIN_STATUS_GRACE_MS);
    return rc;
}

static void wifi_sta_init(void) {
    // STA mode on. (cyw43_arch_init() already ran in main(); it brought up the
    // driver, BT, and lwIP together -- see cyw43_arch_poll.c. We do NOT
    // lwip_init() here.)
    cyw43_arch_enable_sta_mode();

    // Network hostname advertised as "<hostname>.local". User-set in config so
    // multiple dongles on one LAN don't collide on ds5.local. config_valid()
    // has already sanitized it to a valid DNS label (and defaulted it if empty),
    // so it's safe to use verbatim.
    const char *hostname = get_config().hostname;
#if LWIP_NETIF_HOSTNAME
    if (netif_default) netif_set_hostname(netif_default, hostname);
#endif

#if LWIP_MDNS_RESPONDER
    // The mDNS responder can be initialised now; the per-netif registration that
    // actually advertises "<hostname>.local" is deferred to wifi_net_task() once
    // the link is up (adding a netif before it has a link/IP is pointless).
    mdns_resp_init();
#endif

    // Kick off the join asynchronously and return immediately -- do NOT block
    // boot on it (see wifi_start_join). main() proceeds straight to BT/audio/USB
    // init; wifi_net_task() drives the join to completion and retries on failure.
    wifi_start_join();

    printf("[wifi] STA transport starting (http://%s.local/ once a lease lands)\n", hostname);
}

//--------------------------------------------------------------------+
// AP: SoftAP + captive portal for onboarding
//--------------------------------------------------------------------+

// "DS5-Setup-XXXX" where XXXX is the last 2 bytes of the board unique id, so
// multiple dongles being set up in the same room have distinct AP names.
static char ap_ssid[20];
static void build_ap_ssid(void) {
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    snprintf(ap_ssid, sizeof(ap_ssid), "DS5-Setup-%02X%02X",
             uid.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 2],
             uid.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 1]);
}

static void wifi_ap_init(void) {
    in_ap_mode = true;
    ap_led_state = false;
    ap_led_next_toggle = get_absolute_time();
    build_ap_ssid();

    // Open AP (no password): a captive-portal setup network is conventionally
    // open so the user can join without yet another secret, and the only thing
    // it carries is the local provisioning page (no WOL, no LAN access).
    cyw43_arch_enable_ap_mode(ap_ssid, NULL, CYW43_AUTH_OPEN);

    // Give the AP netif a fixed address. Address it explicitly via
    // cyw43_state.netif[CYW43_ITF_AP] rather than netif_default: the SDK sets
    // netif_default per-netif and in a mixed setup it may point at the STA netif,
    // so relying on it here is fragile (this is also what the pico-examples AP
    // demo does).
    ip4_addr_t gw, mask;
    IP4_ADDR(&gw, AP_GW_A, AP_GW_B, AP_GW_C, AP_GW_D);
    // /29 (255.255.255.248): 8 addresses .104-.111, usable hosts .105-.110.
    // Gateway/dongle at .105, DHCP pool .106-.110 (see dhcpserver.h). Small on
    // purpose -- 5 client slots max on the single BT/WiFi radio.
    IP4_ADDR(&mask, 255, 255, 255, 248);
    struct netif *apn = &cyw43_state.netif[CYW43_ITF_AP];
    netif_set_addr(apn, &gw, &mask, &gw);
    // Force broadcast routing out the AP netif. The DHCP server replies to
    // 255.255.255.255 (the client has no IP yet); lwIP routes a global broadcast
    // via netif_default. With both the STA netif (created by cyw43_arch_init even
    // though we never joined) and the AP netif present, netif_default can be the
    // wrong (STA, link-down) interface -> the ACK never reaches the client and it
    // loops REQUEST forever. Pinning default to the AP netif fixes egress for the
    // DHCP + DNS replies.
    netif_set_default(apn);

    // DHCP + DNS servers so a phone gets a lease and every lookup resolves to us
    // (captive-portal detection -> the OS pops the "Sign in" sheet).
    dhcp_server_init(&ap_dhcp, &gw, &mask);
    dns_server_init(&ap_dns, &gw);

    printf("[wifi] AP onboarding: join \"%s\" then browse to http://%u.%u.%u.%u/\n",
           ap_ssid, AP_GW_A, AP_GW_B, AP_GW_C, AP_GW_D);
}

//--------------------------------------------------------------------+
// WiFi scan (AP mode, for the portal's network dropdown)
//--------------------------------------------------------------------+

#define SCAN_MAX 16
struct ScanEntry {
    char ssid[33];
    int16_t rssi;
    uint8_t secure;
};
static ScanEntry scan_list[SCAN_MAX];
static int scan_count = 0;
static bool scan_in_progress = false;
static bool scan_ever_started = false;   // has a scan ever been kicked off?
static absolute_time_t scan_min_next = {0}; // earliest a NEW scan may start (rate-limit)

// Driver callback (cyw43 poll context). De-dup by SSID, keep the strongest RSSI.
static int scan_result_cb(void *env, const cyw43_ev_scan_result_t *r) {
    (void) env;
    if (!r || r->ssid_len == 0 || r->ssid_len > 32) return 0; // skip hidden/garbage
    char ssid[33];
    memcpy(ssid, r->ssid, r->ssid_len);
    ssid[r->ssid_len] = '\0';

    for (int i = 0; i < scan_count; i++) {
        if (strcmp(scan_list[i].ssid, ssid) == 0) {
            if (r->rssi > scan_list[i].rssi) scan_list[i].rssi = r->rssi;
            return 0; // already have it
        }
    }
    if (scan_count < SCAN_MAX) {
        strcpy(scan_list[scan_count].ssid, ssid);
        scan_list[scan_count].rssi = r->rssi;
        scan_list[scan_count].secure = (r->auth_mode != 0) ? 1 : 0;
        scan_count++;
    }
    return 0;
}

// Kick off a scan, but only when appropriate. The portal polls GET
// /api/wifi_scan repeatedly to refresh the dropdown while a scan runs; calling
// this on every poll previously RESTARTED the scan each time -> "scanning" never
// went false -> the page polled forever and the list flickered. Guards:
//   - only one scan at a time (scan_in_progress / cyw43_wifi_scan_active);
//   - a NEW scan may only begin after scan_min_next (rate-limit ~8s), so a busy
//     poll loop can't re-trigger back-to-back scans;
//   - results ACCUMULATE across scans (no scan_count reset) so the dropdown is
//     stable and only grows; the dedup in scan_result_cb keeps it clean.
// The very first call auto-starts (portal just loaded); later calls are the
// rate-limited refresh / the rescan button.
void wifi_scan_start(void) {
    if (!in_ap_mode) return;            // scan only matters during onboarding
    if (scan_in_progress) return;
    if (cyw43_wifi_scan_active(&cyw43_state)) return;
    if (scan_ever_started && !time_reached(scan_min_next)) return; // rate-limit
    cyw43_wifi_scan_options_t opts = {0};
    if (cyw43_wifi_scan(&cyw43_state, &opts, NULL, scan_result_cb) == 0) {
        scan_in_progress = true;
        scan_ever_started = true;
        scan_min_next = make_timeout_time_ms(8000);
        printf("[wifi] scan started\n");
    }
}

// JSON array of networks for the portal, strongest first.
int wifi_scan_json(char *out, int cap) {
    // Insertion sort by RSSI desc (tiny list).
    for (int i = 1; i < scan_count; i++) {
        ScanEntry e = scan_list[i];
        int j = i - 1;
        while (j >= 0 && scan_list[j].rssi < e.rssi) {
            scan_list[j + 1] = scan_list[j];
            j--;
        }
        scan_list[j + 1] = e;
    }
    int w = snprintf(out, cap, "{\"scanning\":%s,\"nets\":[",
                     scan_in_progress ? "true" : "false");
    for (int i = 0; i < scan_count; i++) {
        // Bound this entry's worst-case serialized length so we never emit a
        // half-written network: SSID up to 2x (every byte escaped) + the fixed
        // {"ssid":""..."rssi":-nnn,"secure":n} scaffolding + the "," separator,
        // and leave room for the closing "]}". If it wouldn't fit, stop here --
        // the list is sorted strongest-first, so we keep the networks the user
        // most likely wants and still close the JSON cleanly. (Prevents the
        // truncated-mid-string malformed JSON that broke the portal dropdown in
        // dense RF; see CODE_REVIEW_2 W2.)
        int need = 1 /*,*/ + 10 /*{"ssid":"*/ + 2 * (int)strlen(scan_list[i].ssid) +
                   1 /*"*/ + 34 /*,"rssi":-nnn,"secure":n}*/ + 2 /*]}*/;
        if (w + need > cap) break;
        w += snprintf(out + w, cap - w, "%s{\"ssid\":\"", i ? "," : "");
        for (const char *p = scan_list[i].ssid; *p; p++) {
            if (*p == '"' || *p == '\\') out[w++] = '\\';
            out[w++] = *p;
        }
        w += snprintf(out + w, cap - w, "\",\"rssi\":%d,\"secure\":%u}",
                      scan_list[i].rssi, scan_list[i].secure);
    }
    w += snprintf(out + w, cap - w, "]}");
    return w;
}

//--------------------------------------------------------------------+
// Provisioning: save creds + reboot into STA
//--------------------------------------------------------------------+

static bool reboot_pending = false;
static absolute_time_t reboot_at;

static void wifi_schedule_reboot(uint32_t delay_ms) {
    reboot_at = make_timeout_time_ms(delay_ms);
    reboot_pending = true;
}

bool wifi_provision_apply(const char *ssid, const char *psk,
                          uint8_t auth_mode) {
    if (!ssid) ssid = "";
    if (!psk) psk = "";
    const size_t ssid_len = strlen(ssid);
    const size_t psk_len = strlen(psk);
    if (ssid_len == 0 || ssid_len >= CONFIG_WIFI_SSID_LEN ||
        psk_len >= CONFIG_WIFI_PSK_LEN ||
        (psk_len > 0 && psk_len < 8) ||
        auth_mode > CONFIG_WIFI_AUTH_WPA3) {
        printf("[wifi] provisioning rejected "
               "(ssid=%u bytes, psk=%u bytes, auth=%u)\n",
               (unsigned) ssid_len, (unsigned) psk_len, auth_mode);
        return false;
    }
    config_set_wifi_creds(ssid, psk, auth_mode);
    watchdog_update();        // sector erase blocks with interrupts off
    if (!config_save()) return false;
    printf("[wifi] provisioned SSID \"%s\" using %s; "
           "rebooting into STA mode\n",
           ssid, psk_len == 0 ? "OPEN"
                             : auth_mode == CONFIG_WIFI_AUTH_WPA3
                                   ? "WPA3-SAE" : "WPA2");
    // Defer the reboot a beat so the HTTP "saved" response can flush to the
    // phone before the watchdog resets us. wifi_net_task() fires it.
    wifi_schedule_reboot(1200);
    return true;
}

bool wifi_reset_provisioning_apply() {
    config_set_wifi_creds("", "", CONFIG_WIFI_AUTH_WPA2);
    watchdog_update();
    if (!config_save()) return false;
    printf("[wifi] WiFi credentials cleared; rebooting to AP onboarding\n");
    wifi_schedule_reboot(1200);
    return true;
}

//--------------------------------------------------------------------+
// Init / service
//--------------------------------------------------------------------+

void wifi_net_init() {
    wifi_capture_ap_boot_request();
    const bool provisioned = get_config().wifi_provisioned;
    if (force_ap || !provisioned) {
        wifi_ap_init();       // onboarding
    } else {
        wifi_sta_init();      // normal operation
    }
    web_api_init();           // httpd serves portal (AP) or config page (STA)
}

void wifi_net_task() {
    // lwIP timers + the ARP-resolve state machine. RX + the netif are pumped by
    // cyw43_arch_poll() in the main loop, so we don't poll RX here.
    sys_check_timeouts();

    // Provisioning/reset reboots are deferred so the HTTP response can flush
    // before the watchdog reset. This must work from AP onboarding and from the
    // normal STA config page.
    if (reboot_pending && time_reached(reboot_at)) {
        watchdog_reboot(0, 0, 0);
        return;
    }

    if (in_ap_mode) {
        // Dedicated onboarding mode has no BT/battery LED owner. This pattern
        // intentionally overrides the normal "disable onboard LED" setting.
        wifi_ap_led_task();

        // Track scan completion so the portal can stop polling.
        if (scan_in_progress && !cyw43_wifi_scan_active(&cyw43_state)) {
            scan_in_progress = false;
            printf("[wifi] scan done (%d networks)\n", scan_count);
        }
        return; // no STA link tracking / WOL while onboarding
    }

    wifi_resolve_poll();
    wifi_bootsel_onboarding_task();

    // ~1s status poll: log link state transitions + the DHCP lease once it
    // lands, and retry the join if we never associated (or dropped).
    static absolute_time_t next = {0};
    if (!time_reached(next)) return;
    next = make_timeout_time_ms(1000);

    const int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    static int prev_link = -2;
    if (link != prev_link) {
        const char *s = (link == CYW43_LINK_UP) ? "UP"
                      : (link == CYW43_LINK_JOIN) ? "JOINING"
                      : (link == CYW43_LINK_NOIP) ? "NO-IP"
                      : (link == CYW43_LINK_FAIL) ? "FAIL"
                      : (link == CYW43_LINK_NONET) ? "NO-NET"
                      : (link == CYW43_LINK_BADAUTH) ? "BADAUTH" : "DOWN";
        printf("[wifi] link %s\n", s);
        prev_link = link;
    }

    // STA join supervision. Ordinary failures NEVER clear stored credentials
    // or switch to AP automatically: the home network may simply be absent
    // after moving the host or during an outage, and AP mode intentionally
    // disables Bluetooth/audio. The exception is three consecutive completed
    // authentication failures. Those credentials cannot currently join, so
    // clear them and return to onboarding rather than retrying forever.

    static bool reported_ip = false;
    if (link == CYW43_LINK_UP) {
        sta_retry_stage = 0;
        sta_badauth_failures = 0;
        sta_retry_scheduled = false;
        sta_retry_for_badauth = false;
        sta_failure_latched = false;
        sta_join_kickoff_pending = false;
    }

    if (link == CYW43_LINK_UP && netif_default &&
        !ip4_addr_isany_val(*netif_ip4_addr(netif_default))) {
#if LWIP_MDNS_RESPONDER
        // Advertise "<hostname>.local" now that we have a link + IP. Done once.
        if (!wifi_mdns_added) {
            mdns_resp_add_netif(netif_default, get_config().hostname);
            wifi_mdns_added = true;
        }
#endif
        if (!reported_ip) {
            printf("[wifi] IP %s -- http://%s/ (or http://%s.local/)\n",
                   ip4addr_ntoa(netif_ip4_addr(netif_default)),
                   ip4addr_ntoa(netif_ip4_addr(netif_default)),
                   get_config().hostname);
            reported_ip = true;
        }
    } else if (link == CYW43_LINK_JOIN || link == CYW43_LINK_NOIP) {
        // Association/DHCP is progressing; discard a stale failure deadline.
        sta_retry_scheduled = false;
        sta_retry_for_badauth = false;
        sta_failure_latched = false;
        sta_join_kickoff_pending = false;
    } else if (link == CYW43_LINK_DOWN || link == CYW43_LINK_FAIL ||
               link == CYW43_LINK_NONET || link == CYW43_LINK_BADAUTH) {
        reported_ip = false;

        // A successful async kickoff can take several polls before link status
        // moves away from its previous failure value. Do not launch duplicate
        // joins during that opaque window; recover if the driver never advances.
        // BADAUTH is itself proof that authentication completed, even if the
        // one-second poll missed an intermediate JOIN state.
        if (link == CYW43_LINK_BADAUTH && !sta_failure_latched) {
            sta_join_kickoff_pending = false;
        }
        if (sta_join_kickoff_pending) {
            if (!time_reached(sta_join_status_deadline)) return;
            sta_join_kickoff_pending = false;
            sta_failure_latched = false;
            printf("[wifi] join status did not advance; applying retry backoff\n");
        }

        if (!sta_failure_latched) {
            sta_failure_latched = true;
            if (link == CYW43_LINK_BADAUTH) {
                sta_badauth_failures++;
                printf("[wifi] authentication failed %u/%u\n",
                       sta_badauth_failures, STA_BADAUTH_LIMIT);
                if (sta_badauth_failures >= STA_BADAUTH_LIMIT) {
                    printf("[wifi] repeated authentication failure; "
                           "returning to AP onboarding\n");
                    if (!wifi_reset_provisioning_apply()) {
                        // config_set_wifi_creds() changed the RAM copy before
                        // the failed save. Reboot to reload the persisted copy
                        // and allow another recovery attempt.
                        printf("[wifi] credential clear failed; rebooting to "
                               "reload saved credentials\n");
                        wifi_schedule_reboot(1200);
                    }
                    return;
                }

                sta_retry_at = make_timeout_time_ms(STA_BADAUTH_RETRY_MS);
                sta_retry_scheduled = true;
                sta_retry_for_badauth = true;
                printf("[wifi] authentication retry scheduled in %u s\n",
                       STA_BADAUTH_RETRY_MS / 1000);
            } else {
                // Only consecutive completed authentication failures count.
                // JOIN/NOIP are intermediate states and intentionally do not
                // reset this counter.
                sta_badauth_failures = 0;
                sta_retry_for_badauth = false;
                const unsigned max_stage =
                    sizeof(STA_RETRY_DELAYS_MS) /
                    sizeof(STA_RETRY_DELAYS_MS[0]) - 1;
                const unsigned delay_stage =
                    sta_retry_stage < max_stage ? sta_retry_stage : max_stage;
                uint32_t delay_ms = STA_RETRY_DELAYS_MS[delay_stage];
                const bool controller_limited =
                    bt_connected_count() != 0 &&
                    delay_ms < STA_CONTROLLER_RETRY_MIN_MS;
                if (controller_limited) delay_ms = STA_CONTROLLER_RETRY_MIN_MS;

                sta_retry_at = make_timeout_time_ms(delay_ms);
                sta_retry_scheduled = true;
                printf("[wifi] retry scheduled in %u s%s\n",
                       delay_ms / 1000,
                       controller_limited
                           ? " (controller active: 60 s minimum)" : "");
            }
        }

        if (!sta_retry_scheduled || !time_reached(sta_retry_at)) return;

        // A controller may have connected after a shorter retry was scheduled.
        // Enforce the one-minute radio-contention limit at launch time too.
        // Authentication recovery is deliberately exempt: it performs at most
        // two five-second retries before leaving STA mode.
        if (!sta_retry_for_badauth &&
            bt_connected_count() != 0 && sta_attempt_recorded) {
            const absolute_time_t controller_gate =
                delayed_by_ms(sta_last_attempt, STA_CONTROLLER_RETRY_MIN_MS);
            if (!time_reached(controller_gate)) {
                sta_retry_at = controller_gate;
                printf("[wifi] retry postponed (controller active: 60 s minimum)\n");
                return;
            }
        }

        const bool badauth_retry = sta_retry_for_badauth;
        sta_retry_scheduled = false;
        sta_retry_for_badauth = false;
        const unsigned max_stage =
            sizeof(STA_RETRY_DELAYS_MS) / sizeof(STA_RETRY_DELAYS_MS[0]) - 1;
        if (!badauth_retry && sta_retry_stage < max_stage) sta_retry_stage++;
        // Keep this failure episode latched after a successful kickoff until
        // the driver reports JOIN/NOIP or a 30-second status grace expires.
        sta_failure_latched = true;
        if (wifi_start_join() != 0) sta_failure_latched = false;
    }
}

#endif // ENABLE_WIFI_WOL
