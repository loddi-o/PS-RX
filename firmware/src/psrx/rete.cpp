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
#include "pc.h"
#include "politica_rete.h"
#include "prova_rete.h"
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

// Prova di una rete (rete.h).
struct StatoProva {
    bool attiva = false;
    ProvaRete esito{};
    uint32_t t_inizio = 0, t_fase = 0, t_invio = 0;
    uint8_t invii = 0;
    uint16_t id = 0;
    udp_pcb *pcb = nullptr;
    volatile bool risposta = false;
};
StatoProva prova;

// Reti rilevate (rete.h).
ScansionePsrx scansione{};
uint32_t t_scansione = 0;
constexpr uint32_t T_SCANSIONE_MAX_MS = 15000;

constexpr uint32_t T_JOIN_MAX_MS = 20000;       // tempo massimo per connettersi a una rete
constexpr uint32_t T_PROVA_IP_MS = 15000;       // indirizzo dal router
constexpr uint32_t T_PROVA_DNS_MS = 1500;       // fra una domanda DNS e l'altra
constexpr uint8_t PROVA_DNS_INVII = 4;          // 1.1.1.1, 8.8.8.8, 1.1.1.1, 8.8.8.8
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

// --- prova di una rete ---------------------------------------------------------------------------
void chiudi_pcb_prova() {
    if (prova.pcb) {
        udp_remove(prova.pcb);
        prova.pcb = nullptr;
    }
}

void fine_prova(uint8_t esito, uint32_t ora) {
    prova.attiva = false;
    prova.esito.fase = PROVA_FINITA;
    prova.esito.esito = esito;
    prova.esito.ms_totale = static_cast<uint16_t>(ora - prova.t_inizio > 65535 ? 65535 : ora - prova.t_inizio);
    chiudi_pcb_prova();
    join_in_corso = false;
    fallimenti_di_fila = 0;
    static const char *const TESTI[] = {"", "riuscita", "password sbagliata", "rete non trovata", "nessuna risposta",
                                        "nessun indirizzo IP", "internet non raggiungibile", "annullata"};
    psrx_log("prova del WiFi: %s", esito < 8 ? TESTI[esito] : "?");
    if (esito != ESITO_OK && radio_accesa) {
        // Si torna alle reti salvate a turno, da capo.
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        rete_corrente = -1;
        t_prossimo_tentativo = ora + 1000;
    }
}

void ricevi_dns(void *, udp_pcb *, pbuf *p, const ip_addr_t *, u16_t) {
    if (!p) return;
    uint8_t h[12];
    if (pbuf_copy_partial(p, h, sizeof h, 0) == sizeof h && dns_risposta_valida(h, sizeof h, prova.id)) {
        prova.risposta = true;
    }
    pbuf_free(p);
}

bool invia_dns() {
    if (!prova.pcb) {
        prova.pcb = udp_new();
        if (!prova.pcb) return false;
        udp_recv(prova.pcb, ricevi_dns, nullptr);
    }
    uint8_t domanda[DNS_DOMANDA_MAX];
    const uint16_t n = dns_domanda(domanda, prova.id);
    pbuf *p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
    if (!p) return false;
    memcpy(p->payload, domanda, n);
    ip_addr_t server;
    if (prova.invii % 2 == 0) IP4_ADDR(&server, 1, 1, 1, 1);
    else IP4_ADDR(&server, 8, 8, 8, 8);
    const err_t e = udp_sendto(prova.pcb, p, &server, DNS_PORTA);
    pbuf_free(p);
    return e == ERR_OK;
}

void sorveglia_prova(uint32_t ora) {
    ProvaRete &e = prova.esito;
    if (e.fase == PROVA_CONNESSIONE) {
        const int s = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (s == CYW43_LINK_BADAUTH) return fine_prova(ESITO_PASSWORD, ora);
        if (s == CYW43_LINK_NONET) return fine_prova(ESITO_NON_TROVATA, ora);
        if (s == CYW43_LINK_FAIL) return fine_prova(ESITO_NESSUNA_RISPOSTA, ora);
        if (s == CYW43_LINK_NOIP || s == CYW43_LINK_UP) {   // associato: la password e' giusta
            e.fase = PROVA_INDIRIZZO;
            prova.t_fase = ora;
        } else if (ora - prova.t_fase > T_JOIN_MAX_MS) {
            return fine_prova(ESITO_NESSUNA_RISPOSTA, ora);
        }
        return;
    }
    if (e.fase == PROVA_INDIRIZZO) {
        if (link_pronto()) {
            const netif *n = &cyw43_state.netif[CYW43_ITF_STA];
            memcpy(e.ip, &netif_ip4_addr(n)->addr, 4);
            memcpy(e.gateway, &netif_ip4_gw(n)->addr, 4);
            int32_t r = 0;
            if (cyw43_wifi_get_rssi(&cyw43_state, &r) == 0) {
                e.rssi = static_cast<int8_t>(r < -127 ? -127 : r > 0 ? 0 : r);
                rssi_wifi = static_cast<int16_t>(r);
            }
            e.fase = PROVA_INTERNET;
            prova.t_fase = ora;
            prova.invii = 0;
            prova.risposta = false;
            prova.id = static_cast<uint16_t>(ora * 2654435761u >> 16);
            prova.t_invio = ora;
            invia_dns();
        } else if (ora - prova.t_fase > T_PROVA_IP_MS) {
            return fine_prova(ESITO_NESSUN_IP, ora);
        }
        return;
    }
    if (e.fase == PROVA_INTERNET) {
        if (prova.risposta) {
            e.ms_internet = static_cast<uint16_t>(ora - prova.t_invio);
            return fine_prova(ESITO_OK, ora);
        }
        if (ora - prova.t_invio >= T_PROVA_DNS_MS) {
            if (++prova.invii >= PROVA_DNS_INVII) return fine_prova(ESITO_NO_INTERNET, ora);
            prova.t_invio = ora;
            invia_dns();
        }
    }
}

// --- reti rilevate -------------------------------------------------------------------------------
int risultato_scansione(void *, const cyw43_ev_scan_result_t *r) {
    if (!r || r->ssid_len == 0 || r->ssid_len > 32) return 0;   // reti nascoste: niente nome
    char ssid[33] = {};
    memcpy(ssid, r->ssid, r->ssid_len);
    const int8_t rssi = static_cast<int8_t>(r->rssi < -127 ? -127 : r->rssi > 0 ? 0 : r->rssi);
    ReteVista *dove = nullptr;
    for (uint8_t i = 0; i < scansione.n; i++) {
        if (strcmp(scansione.reti[i].ssid, ssid) == 0) {
            if (rssi <= scansione.reti[i].rssi) return 0;   // stessa rete, segnale peggiore (altro access point)
            dove = &scansione.reti[i];
            break;
        }
    }
    if (!dove && scansione.n < RETI_VISTE_MAX) dove = &scansione.reti[scansione.n++];
    if (!dove) {   // elenco pieno: sostituisce la piu' debole, se questa e' piu' forte
        ReteVista *debole = &scansione.reti[0];
        for (auto &v : scansione.reti) if (v.rssi < debole->rssi) debole = &v;
        if (rssi <= debole->rssi) return 0;
        dove = debole;
    }
    memcpy(dove->ssid, ssid, sizeof ssid);
    dove->rssi = rssi;
    dove->canale = static_cast<uint8_t>(r->channel);
    dove->sicurezza = r->auth_mode;
    return 0;
}

void sorveglia_scansione(uint32_t ora) {
    if (cyw43_wifi_scan_active(&cyw43_state) && ora - t_scansione < T_SCANSIONE_MAX_MS) return;
    scansione.stato = SCAN_FINITA;
    // dalla piu' forte alla piu' debole
    for (uint8_t i = 1; i < scansione.n; i++) {
        for (uint8_t j = i; j > 0 && scansione.reti[j].rssi > scansione.reti[j - 1].rssi; j--) {
            const ReteVista t = scansione.reti[j];
            scansione.reti[j] = scansione.reti[j - 1];
            scansione.reti[j - 1] = t;
        }
    }
    psrx_log("WiFi: %u reti rilevate", scansione.n);
    t_prossimo_tentativo = ora;   // si riprende a collegarsi alle reti salvate
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

    // Il WoL parte durante la finestra di risveglio (pc.h, risveglio.h).
    const DecisioneRete d = politica.aggiorna(ora, bt_connected_count(), link_pronto(), wol_attivo(), ha_reti(),
                                              pc_finestra_risveglio());
    if (d.invia_wol) invia_wol_a_tutti();
    if (d.wifi_acceso && !radio_accesa) accendi();
    if (!d.wifi_acceso && radio_accesa) {
        if (prova.attiva) fine_prova(ESITO_ANNULLATA, ora);   // un controller si e' collegato
        if (scansione.stato == SCAN_IN_CORSO) scansione.stato = SCAN_ANNULLATA;
        spegni();
    }
    if (radio_accesa) {
        if (scansione.stato == SCAN_IN_CORSO) sorveglia_scansione(ora);
        else if (prova.attiva) sorveglia_prova(ora);
        else sorveglia_join(ora);
    }
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

bool rete_info_connessa() {
    return link_pronto();
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

bool rete_avvia_prova(uint8_t indice) {
    if (!radio_accesa || indice >= PSRX_MAX_RETI || !get_config().psrx_reti[indice].ssid[0]) return false;
    const uint32_t ora = ora_ms();
    chiudi_pcb_prova();
    prova.attiva = true;
    prova.esito = ProvaRete{};
    prova.esito.rete = static_cast<int8_t>(indice);
    prova.esito.fase = PROVA_CONNESSIONE;
    prova.t_inizio = prova.t_fase = ora;
    psrx_log("prova del WiFi su \"%s\"", get_config().psrx_reti[indice].ssid);
    if (link_pronto() && rete_corrente == static_cast<int8_t>(indice)) {
        prova.esito.fase = PROVA_INDIRIZZO;   // gia' connessi a quella rete: si parte dall'indirizzo
        return true;
    }
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    avvia_join(static_cast<int8_t>(indice));
    if (!join_in_corso) fine_prova(ESITO_NESSUNA_RISPOSTA, ora);
    return true;
}

void rete_esito_prova(ProvaRete *out) {
    *out = prova.esito;
    if (prova.attiva) out->ms_totale = static_cast<uint16_t>(ora_ms() - prova.t_inizio);
}

bool rete_avvia_scansione() {
    if (!radio_accesa || prova.attiva || scansione.stato == SCAN_IN_CORSO) return false;
    if (join_in_corso) {   // un collegamento a meta' impedisce la scansione: lo si riprende dopo
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        join_in_corso = false;
    }
    scansione = ScansionePsrx{};
    cyw43_wifi_scan_options_t opzioni = {};
    if (cyw43_wifi_scan(&cyw43_state, &opzioni, nullptr, risultato_scansione) != 0) {
        scansione.stato = SCAN_ANNULLATA;
        return false;
    }
    scansione.stato = SCAN_IN_CORSO;
    t_scansione = ora_ms();
    psrx_log("WiFi: ricerca delle reti");
    return true;
}

void rete_scansione(ScansionePsrx *out) {
    *out = scansione;
}
