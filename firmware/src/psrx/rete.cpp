//
// PS-RX - WiFi solo per il Wake-on-LAN (vedi rete.h e politica_rete.h).
//
// Tutto gira nel ciclo principale del core 0. Le operazioni sul chip sono asincrone (connessione)
// o brevi (accendere/spegnere la modalita' STA, qualche ms) e avvengono solo senza controller
// collegati, oppure una volta subito dopo il collegamento del primo controller per spegnere la radio.
//

#include "rete.h"

#include <cstdio>
#include <cstring>

#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"

#include "bt.h"
#include "config.h"
#include "log_psrx.h"
#include "politica_rete.h"
#include "wifi_net.h"

namespace {

PoliticaRete politica;
bool radio_accesa = false;
int8_t rete_corrente = -1;            // indice in Config_body::psrx_reti
uint32_t t_inizio_join = 0;
uint32_t t_prossimo_tentativo = 0;
uint8_t fallimenti_di_fila = 0;
bool join_in_corso = false;
uint32_t t_ultimo_wol = 0;
bool wol_mai = true;
uint32_t t_controllo = 0;
int16_t rssi_wifi = 0;             // letto ogni 5 s nel ciclo (ioctl al chip: mai dentro le richieste USB)
uint32_t t_rssi = 0;

constexpr uint32_t T_JOIN_MAX_MS = 20000;       // tempo massimo per connettersi a una rete
constexpr uint32_t ATTESE_MS[] = {2000, 5000, 15000, 30000, 60000};

uint32_t ora_ms() { return to_ms_since_boot(get_absolute_time()); }

bool mac_zero(const uint8_t *m) {
    for (int i = 0; i < 6; i++) if (m[i]) return false;
    return true;
}

bool ha_reti() {
    for (const auto &r : get_config().psrx_reti) if (r.ssid[0]) return true;
    return false;
}

bool wol_attivo() {
    const Config_body &c = get_config();
    return !c.psrx_wol_spento && (!mac_zero(c.wol_target_mac) || !mac_zero(c.wol_target_mac2));
}

bool link_pronto() {
    if (!radio_accesa) return false;
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) return false;
    const netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    return !ip4_addr_isany_val(*netif_ip4_addr(n));
}

// Prossima rete salvata dopo 'da' (a giro), -1 se non ce ne sono.
int8_t prossima_rete(int8_t da) {
    const auto &reti = get_config().psrx_reti;
    for (int i = 1; i <= PSRX_MAX_RETI; i++) {
        const int k = (da + i + PSRX_MAX_RETI) % PSRX_MAX_RETI;
        if (reti[k].ssid[0]) return static_cast<int8_t>(k);
    }
    return -1;
}

void avvia_join(int8_t indice) {
    const ReteSalvata &r = get_config().psrx_reti[indice];
    const bool aperta = r.psk[0] == '\0';
    const uint32_t auth = aperta ? CYW43_AUTH_OPEN
                                 : r.auth == CONFIG_WIFI_AUTH_WPA3 ? CYW43_AUTH_WPA3_SAE_AES_PSK
                                                                   : CYW43_AUTH_WPA2_MIXED_PSK;
    rete_corrente = indice;
    t_inizio_join = ora_ms();
    const int rc = cyw43_arch_wifi_connect_async(r.ssid, aperta ? nullptr : r.psk, auth);
    join_in_corso = rc == 0;
    psrx_log("WiFi: mi connetto a \"%s\" (%s)%s", r.ssid, aperta ? "aperta" : r.auth ? "WPA3" : "WPA2",
             rc ? " - avvio fallito" : "");
}

void accendi() {
    cyw43_arch_enable_sta_mode();
    radio_accesa = true;
    fallimenti_di_fila = 0;
    t_prossimo_tentativo = ora_ms();
    rete_corrente = -1;
    join_in_corso = false;
    psrx_log("WiFi acceso (nessun controller collegato)");
}

void spegni() {
    cyw43_arch_disable_sta_mode();
    radio_accesa = false;
    join_in_corso = false;
    psrx_log("WiFi spento: la radio resta al Bluetooth");
}

void invia_wol_a_tutti() {
    const Config_body &c = get_config();
    const uint8_t *destinazioni[2] = {c.wol_target_mac, c.wol_target_mac2};
    for (const uint8_t *mac : destinazioni) {
        if (mac_zero(mac)) continue;
        const bool ok = wifi_wol_send(mac);
        psrx_log("Wake-on-LAN %02X:%02X:%02X:%02X:%02X:%02X %s", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                 ok ? "inviato" : "NON inviato");
    }
    t_ultimo_wol = ora_ms();
    wol_mai = false;
}

// Connessione alle reti salvate, a turno, con attese crescenti fra un giro e l'altro.
void sorveglia_join(uint32_t ora) {
    if (link_pronto()) {
        if (join_in_corso) {
            const netif *n = &cyw43_state.netif[CYW43_ITF_STA];
            psrx_log("WiFi connesso a \"%s\", IP %s", get_config().psrx_reti[rete_corrente].ssid,
                     ip4addr_ntoa(netif_ip4_addr(n)));
        }
        join_in_corso = false;
        fallimenti_di_fila = 0;
        if (ora - t_rssi >= 5000) {
            t_rssi = ora;
            int32_t r = 0;
            if (cyw43_wifi_get_rssi(&cyw43_state, &r) == 0) rssi_wifi = static_cast<int16_t>(r);
        }
        return;
    }
    const int stato = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (join_in_corso) {
        const bool fallito = stato == CYW43_LINK_FAIL || stato == CYW43_LINK_NONET || stato == CYW43_LINK_BADAUTH;
        if (!fallito && ora - t_inizio_join < T_JOIN_MAX_MS) return;
        psrx_log("WiFi: \"%s\" non raggiungibile (%s)", get_config().psrx_reti[rete_corrente].ssid,
                 stato == CYW43_LINK_BADAUTH ? "password sbagliata" : stato == CYW43_LINK_NONET ? "rete assente"
                                                                                                 : "nessuna risposta");
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        join_in_corso = false;
        const uint8_t i = fallimenti_di_fila < 4 ? fallimenti_di_fila : 4;
        t_prossimo_tentativo = ora + ATTESE_MS[i];
        if (fallimenti_di_fila < 255) fallimenti_di_fila++;
        return;
    }
    if (static_cast<int32_t>(ora - t_prossimo_tentativo) < 0) return;
    const int8_t k = prossima_rete(rete_corrente);
    if (k >= 0) avvia_join(k);
}

} // namespace

// --- interfaccia di wifi_net.h (usata da main.cpp) --------------------------------------------

bool wifi_wol_send(const uint8_t mac[6]) {
    uint8_t magico[102];
    memset(magico, 0xFF, 6);
    for (int i = 0; i < 16; i++) memcpy(magico + 6 + i * 6, mac, 6);
    udp_pcb *pcb = udp_new();
    if (!pcb) return false;
    ip_set_option(pcb, SOF_BROADCAST);
    pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof magico, PBUF_RAM);
    if (!p) {
        udp_remove(pcb);
        return false;
    }
    memcpy(p->payload, magico, sizeof magico);
    ip_addr_t broadcast;
    IP4_ADDR(&broadcast, 255, 255, 255, 255);
    const err_t e = udp_sendto(pcb, p, &broadcast, 9);
    pbuf_free(p);
    udp_remove(pcb);
    return e == ERR_OK;
}

void wifi_net_request_ap_onboarding() {}
bool wifi_net_in_ap_mode() { return false; }

void wifi_net_init() {
    const Config_body &c = get_config();
    int n = 0;
    for (const auto &r : c.psrx_reti) if (r.ssid[0]) n++;
    psrx_log("rete: %d reti WiFi salvate, Wake-on-LAN %s", n, wol_attivo() ? "attivo" : "non configurato");
}

void wifi_net_task() {
    sys_check_timeouts();   // timer di lwIP (DHCP, ARP)
    const uint32_t ora = ora_ms();
    if (ora - t_controllo < 10) return;
    t_controllo = ora;

    const DecisioneRete d = politica.aggiorna(ora, bt_connected_count(), link_pronto(), wol_attivo(), ha_reti());
    if (d.invia_wol) invia_wol_a_tutti();
    if (d.wifi_acceso && !radio_accesa) accendi();
    if (!d.wifi_acceso && radio_accesa) spegni();
    if (radio_accesa) sorveglia_join(ora);
}

// Chiamata da wake.cpp quando un controller vuole svegliare l'host sospeso. La politica manda gia'
// il WoL al collegamento del primo controller; qui si aggiunge un invio solo se il WiFi e' connesso.
extern "C" bool wake_emit_wol(void) {
    if (!link_pronto() || !wol_attivo()) return false;
    invia_wol_a_tutti();
    return true;
}

// --- per il servizio USB ----------------------------------------------------------------------

void rete_info(InfoRete *out) {
    *out = InfoRete{};
    out->rete = rete_corrente;
    out->wol_finestra = politica.finestra_wol() ? 1 : 0;
    out->ms_da_ultimo_wol = wol_mai ? 0xFFFFFFFFu : ora_ms() - t_ultimo_wol;
    if (!radio_accesa) {
        out->stato = RETE_SPENTA;
        return;
    }
    if (link_pronto()) {
        out->stato = RETE_CONNESSA;
        const ip4_addr_t *ip = netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]);
        out->ip[0] = ip4_addr1(ip);
        out->ip[1] = ip4_addr2(ip);
        out->ip[2] = ip4_addr3(ip);
        out->ip[3] = ip4_addr4(ip);
        out->rssi = rssi_wifi;
        return;
    }
    out->stato = join_in_corso ? RETE_CONNESSIONE : RETE_ERRORE;
}

bool rete_invia_wol_ora() {
    if (!link_pronto()) return false;
    invia_wol_a_tutti();
    return true;
}

void rete_reti_cambiate() {
    if (!radio_accesa) return;
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    join_in_corso = false;
    rete_corrente = -1;
    fallimenti_di_fila = 0;
    t_prossimo_tentativo = ora_ms();
}
