//
// PS-RX - servizio USB per le app (vedi servizio_usb.h e protocollo.h).
//

#include "servizio_usb.h"

#include <cstring>
#include <malloc.h>

#include "aggiornamento.h"
#include "audio.h"
#include "bt.h"
#include "config.h"
#include "eventi.h"
#include "log_psrx.h"
#include "pad.h"
#include "protocollo.h"
#include "psrx_config.h"
#include "rete.h"
#include "salvataggio.h"
#include "state_mgr.h"
#include "usb.h"
#include "weblog.h"

#include "pico/cyw43_arch.h"
#include "pico/time.h"

extern bool spk_active; // main.cpp: l'host ha aperto l'altoparlante del controller

#ifndef PICO_PROGRAM_VERSION_STRING
#define PICO_PROGRAM_VERSION_STRING "sconosciuta"
#endif

static constexpr char BASE_DLB[] = "DS5-Linux-Bridge 2.3.0-b1";

static uint8_t risposta[2304] __attribute__((aligned(4)));  // il registro e' il piu' grande (circa 2,1 KB)
static uint8_t dati[128] __attribute__((aligned(4)));       // dati dei comandi brevi
static uint8_t *destinazione = nullptr;                     // dove arrivano i dati della richiesta in corso
static uint8_t ultimo_errore = ERR_NESSUNO;
static uint8_t ultimo_comando = 0;

static uint32_t t_ultimo_stato = 0;
static uint32_t t_rssi = 0;
static uint32_t t_heap = 0;
static uint32_t heap_libero = 0;

static Eventi eventi;
static uint32_t t_eventi = 0;

// Azioni messe in coda dalle richieste e eseguite in servizio_usb_task().
static bool rich_led = false;
static bool rich_forma_usb = false;   // posti fissi cambiati: ricalcola la variante USB
static bool rich_abbina = false;
static bool rich_dimentica = false;
static bool rich_dimentica_tutti = false;
static bool rich_spegni_tutti = false;
static bool rich_wol = false;
static uint8_t rich_spegni_posto = 0;   // un bit per posto
static uint8_t mac_dimentica[BT_ADDR_LEN];

static uint32_t ora_ms() {
    return to_ms_since_boot(get_absolute_time());
}

static void segna_errore(uint8_t errore, uint8_t comando) {
    ultimo_errore = errore;
    ultimo_comando = comando;
    // "occupato" e "controller collegato" arrivano a raffica mentre l'app aspetta: non li registro.
    if (errore != ERR_OCCUPATO && errore != ERR_PAD_CONNESSO) {
        psrx_log("servizio USB: comando 0x%02X rifiutato (errore %u)", comando, errore);
    }
}

static bool mac_zero(const uint8_t *m) {
    for (int i = 0; i < 6; i++) if (m[i]) return false;
    return true;
}

static bool bond_esiste(const uint8_t *mac) {
    uint8_t lista[4][BT_ADDR_LEN];
    const int n = bt_bond_list(lista, 4);
    for (int i = 0; i < n; i++) {
        if (memcmp(lista[i], mac, BT_ADDR_LEN) == 0) return true;
    }
    return false;
}

// --- impostazioni globali -------------------------------------------------------------------

static bool valore_impostazione(const Config_body &c, uint8_t id, uint16_t *valore) {
    switch (id) {
        case IMP_MODALITA:           *valore = c.psrx_modalita; return true;
        case IMP_POSTI_FISSI:        *valore = c.psrx_posti_fissi; return true;
        case IMP_LED_POSTO:          *valore = c.psrx_led_posto; return true;
        case IMP_WOL_SPENTO:         *valore = c.psrx_wol_spento; return true;
        case IMP_BUFFER_AUDIO:       *valore = c.audio_buffer_length; return true;
        case IMP_INATTIVITA_MIN:     *valore = c.inactive_time; return true;
        case IMP_MAI_DISCONNETTERE:  *valore = c.disable_inactive_disconnect; return true;
        case IMP_LED_PICO_SPENTO:    *valore = c.disable_pico_led; return true;
        case IMP_TASTIERA_RISVEGLIO: *valore = c.wake_kbd_enabled; return true;
        case IMP_REGISTRO:           *valore = c.weblog_enabled; return true;
    }
    return false;
}

static uint8_t imposta(uint8_t id, uint16_t valore) {
    Config_body c = get_config();
    uint16_t attuale = 0;
    if (!valore_impostazione(c, id, &attuale)) return ERR_VALORE;
    switch (id) {
        case IMP_MODALITA:           if (valore > PSRX_MODALITA_STEAM) return ERR_VALORE;
                                     c.psrx_modalita = valore; break;
        case IMP_BUFFER_AUDIO:       if (valore < 16 || valore > 128) return ERR_VALORE;
                                     c.audio_buffer_length = valore; break;
        case IMP_INATTIVITA_MIN:     if (valore < 5 || valore > 60) return ERR_VALORE;
                                     c.inactive_time = valore; break;
        default:
            if (valore > 1) return ERR_VALORE;
            switch (id) {
                case IMP_POSTI_FISSI:        c.psrx_posti_fissi = valore; break;
                case IMP_LED_POSTO:          c.psrx_led_posto = valore; break;
                case IMP_WOL_SPENTO:         c.psrx_wol_spento = valore; break;
                case IMP_MAI_DISCONNETTERE:  c.disable_inactive_disconnect = valore; break;
                case IMP_LED_PICO_SPENTO:    c.disable_pico_led = valore; break;
                case IMP_TASTIERA_RISVEGLIO: c.wake_kbd_enabled = valore; break;
                case IMP_REGISTRO:           c.weblog_enabled = valore; break;
            }
    }
    if (attuale == valore) return ERR_NESSUNO;
    config_imposta(c);
    // Effetti immediati: solo variabili. Il cambio di descrittore USB lo fa usb_variant_task() di
    // DS5-Linux-Bridge nel ciclo principale.
    if (id == IMP_LED_PICO_SPENTO) rich_led = true;
    if (id == IMP_POSTI_FISSI) rich_forma_usb = true;
    if (id == IMP_MODALITA) usb_request_xbox(valore == PSRX_MODALITA_XBOX);
    if (id == IMP_TASTIERA_RISVEGLIO) usb_request_wake_kbd(valore != 0);
    if (id == IMP_REGISTRO) weblog_set_enabled(valore != 0);
    salvataggio_segna_modifica();
    psrx_log("app: impostazione %u = %u", id, valore);
    return ERR_NESSUNO;
}

static uint8_t predefinite() {
    const bool tastiera = get_config().wake_kbd_enabled;
    // Reti WiFi, destinazioni del WoL, nomi e impostazioni dei controller restano: si azzerano solo
    // le impostazioni del ricevitore.
    const Config_body vecchia = get_config();
    config_default();
    Config_body c = get_config();
    memcpy(c.psrx_reti, vecchia.psrx_reti, sizeof c.psrx_reti);
    memcpy(c.psrx_pad, vecchia.psrx_pad, sizeof c.psrx_pad);
    memcpy(c.bond_names, vecchia.bond_names, sizeof c.bond_names);
    memcpy(c.wol_target_mac, vecchia.wol_target_mac, sizeof c.wol_target_mac);
    memcpy(c.wol_target_mac2, vecchia.wol_target_mac2, sizeof c.wol_target_mac2);
    config_imposta(c);
    if (tastiera) usb_request_wake_kbd(false);
    usb_request_xbox(false);      // modalita' PlayStation
    rich_forma_usb = true;        // posti dinamici
    weblog_set_enabled(false);
    rich_led = true;
    salvataggio_segna_modifica();
    psrx_log("app: impostazioni del ricevitore di fabbrica");
    return ERR_NESSUNO;
}

// --- impostazioni per controller ----------------------------------------------------------------

static uint8_t imposta_pad(const ImpostaPad &r) {
    if (!bond_esiste(r.mac)) return ERR_NON_TROVATO;
    if (r.valore > (r.id == PAD_POLLING ? 3 : 1)) return ERR_VALORE;
    PadImpostazioni *p = config_pad_scrivibile(r.mac);
    if (!p) return ERR_PIENO;
    switch (r.id) {
        case PAD_AUDIO:               p->audio_spento = r.valore ? 0 : 1; break;
        case PAD_MICROFONO:           p->microfono_spento = r.valore ? 0 : 1; break;
        case PAD_POLLING:             p->polling = r.valore; break;
        case PAD_TRACKPAD:            p->trackpad = r.valore; break;
        case PAD_INVERTI_SCORRIMENTO: p->inverti_scorrimento = r.valore; break;
        default:                      return ERR_VALORE;
    }
    salvataggio_segna_modifica();
    psrx_log("app: controller %02X:%02X:%02X:%02X:%02X:%02X impostazione %u = %u", r.mac[0], r.mac[1], r.mac[2],
             r.mac[3], r.mac[4], r.mac[5], r.id, r.valore);
    return ERR_NESSUNO;
}

// --- reti WiFi e Wake-on-LAN --------------------------------------------------------------------

static uint8_t rete_salva(uint16_t indice, const ReteDati &r) {
    if (indice >= PSRX_MAX_RETI) return ERR_VALORE;
    const size_t ls = strnlen(r.ssid, sizeof r.ssid);
    const size_t lp = strnlen(r.password, sizeof r.password);
    if (ls == 0 || ls >= sizeof r.ssid || lp >= sizeof r.password || (lp > 0 && lp < 8) ||
        r.auth > CONFIG_WIFI_AUTH_WPA3) {
        return ERR_VALORE;
    }
    Config_body c = get_config();
    ReteSalvata &s = c.psrx_reti[indice];
    const bool stessa = strcmp(s.ssid, r.ssid) == 0;
    if (!(r.mantieni_password && stessa && lp == 0)) {
        memset(s.psk, 0, sizeof s.psk);
        memcpy(s.psk, r.password, lp);
    }
    memset(s.ssid, 0, sizeof s.ssid);
    memcpy(s.ssid, r.ssid, ls);
    s.auth = r.auth;
    config_imposta(c);
    salvataggio_segna_modifica();
    rete_reti_cambiate();
    psrx_log("app: rete WiFi %u = \"%s\"", indice, s.ssid);
    return ERR_NESSUNO;
}

static uint8_t rete_cancella(uint16_t indice) {
    if (indice >= PSRX_MAX_RETI) return ERR_VALORE;
    Config_body c = get_config();
    memset(&c.psrx_reti[indice], 0, sizeof c.psrx_reti[indice]);
    config_imposta(c);
    salvataggio_segna_modifica();
    rete_reti_cambiate();
    psrx_log("app: rete WiFi %u cancellata", indice);
    return ERR_NESSUNO;
}

static uint8_t wol_destinazioni(const uint8_t *buf) {
    Config_body c = get_config();
    memcpy(c.wol_target_mac, buf, 6);
    memcpy(c.wol_target_mac2, buf + 6, 6);
    config_imposta(c);
    salvataggio_segna_modifica();
    psrx_log("app: destinazioni del Wake-on-LAN aggiornate");
    return ERR_NESSUNO;
}

// --- letture ---------------------------------------------------------------------------

static uint16_t info(uint8_t *out) {
    InfoPsrx i{};
    memcpy(i.magic, "PSRX", 4);
    i.protocollo = PSRX_PROTOCOLLO;
    i.slot_max = pad_posti();
    i.capacita = CAP_AGGIORNAMENTO_APP | CAP_WOL | CAP_REGISTRO;
    i.staging_max = aggiornamento_capacita();
    strncpy(i.versione, PICO_PROGRAM_VERSION_STRING, sizeof i.versione - 1);
    strncpy(i.base, BASE_DLB, sizeof i.base - 1);
    memcpy(out, &i, sizeof i);
    return sizeof i;
}

static uint16_t stato(uint8_t *out, bool silenzioso) {
    if (!silenzioso) t_ultimo_stato = ora_ms();
    const Config_body &c = get_config();
    StatoPsrx s{};
    s.versione = 1;
    s.modalita = c.psrx_modalita;
    s.usb_gamepad = static_cast<uint8_t>(usb_active_gamepad_slots() + usb_xbox_posti());
    s.usb_flag = (tud_mounted() ? 1u : 0u) | (usb_host_suspended() ? 2u : 0u) | (usb_wake_kbd_active() ? 4u : 0u);
    s.audio_flag = (spk_active ? 1u : 0u) | (audio_mic_attivo() ? 2u : 0u) | (g_firmware_mic_muted ? 4u : 0u);
    s.pad_connessi = static_cast<uint8_t>(pad_connessi());
    s.flag = (pad_finestra_abbinamento() ? 1u : 0u) | (salvataggio_in_sospeso() ? 2u : 0u) |
             (c.psrx_posti_fissi ? 4u : 0u);
    s.caricamento = aggiornamento_stato();
    s.uptime_s = ora_ms() / 1000;
    s.heap_libero = heap_libero;
    s.ultimo_evento = eventi.ultimo();
    InfoRete r;
    rete_info(&r);
    s.rete_stato = r.stato;
    s.rete_indice = r.rete;
    memcpy(s.rete_ip, r.ip, 4);
    s.rete_rssi = r.rssi;
    s.wol_finestra = r.wol_finestra;
    s.ms_da_ultimo_wol = r.ms_da_ultimo_wol;
    for (uint8_t posto = 0; posto < PSRX_PAD_MAX && posto < pad_posti(); posto++) {
        PadPsrx &p = s.pad[posto];
        p.rssi = 127;
        PadPerEventi e;
        pad_info(posto, e);
        if (!e.connesso) continue;
        p.connesso = 1;
        p.modello = e.modello;
        p.batteria = e.batteria;
        const PadImpostazioni *imp = config_pad(e.mac);
        const bool audio = !(imp && imp->audio_spento);
        p.flag = (e.batteria_valida ? 1u : 0u) | (e.in_carica ? 2u : 0u) | (audio ? 4u : 0u);
        p.rssi = pad_rssi(posto);
        p.polling = imp ? imp->polling : 0;
        p.report_al_secondo = pad_report_al_secondo(posto);
        memcpy(p.mac, e.mac, sizeof p.mac);
    }
    memcpy(out, &s, sizeof s);
    return sizeof s;
}

static uint16_t impostazioni(uint8_t *out) {
    const Config_body &c = get_config();
    out[0] = PSRX_N_IMPOSTAZIONI;
    uint16_t n = 1;
    for (uint8_t id = 1; id <= PSRX_N_IMPOSTAZIONI; id++) {
        uint16_t valore = 0;
        valore_impostazione(c, id, &valore);
        VoceImpostazione v{};
        v.id = id;
        v.valore = valore;
        memcpy(out + n, &v, sizeof v);
        n += sizeof v;
    }
    return n;
}

static uint16_t abbinati(uint8_t *out) {
    uint8_t lista[4][BT_ADDR_LEN];
    const int quanti = bt_bond_list(lista, 4);
    out[0] = static_cast<uint8_t>(quanti);
    uint16_t n = 1;
    for (int i = 0; i < quanti; i++) {
        VoceAbbinato v{};
        memcpy(v.mac, lista[i], BT_ADDR_LEN);
        const char *nome = config_bond_name(lista[i]);
        if (nome) strncpy(v.nome, nome, sizeof v.nome - 1);
        v.posto = 0xFF;
        for (uint8_t posto = 0; posto < pad_posti(); posto++) {
            PadPerEventi e;
            pad_info(posto, e);
            if (e.connesso && memcmp(e.mac, lista[i], BT_ADDR_LEN) == 0) v.posto = posto;
        }
        const PadImpostazioni *imp = config_pad(lista[i]);
        v.audio = imp && imp->audio_spento ? 0 : 1;
        v.microfono = imp && imp->microfono_spento ? 0 : 1;
        v.polling = imp ? imp->polling : 0;
        v.trackpad = imp ? imp->trackpad : 0;
        v.inverti_scorrimento = imp ? imp->inverti_scorrimento : 0;
        memcpy(out + n, &v, sizeof v);
        n += sizeof v;
    }
    return n;
}

static uint16_t registro(uint8_t *out, uint16_t capienza) {
    out[0] = weblog_enabled() ? 1 : 0;
    out[1] = 0;
    const int n = weblog_snapshot(reinterpret_cast<char *>(out + 4), capienza - 4);
    out[2] = static_cast<uint8_t>(n & 0xFF);
    out[3] = static_cast<uint8_t>(n >> 8);
    return static_cast<uint16_t>(4 + n);
}

static uint16_t leggi_eventi(uint8_t *out, uint16_t dopo) {
    EventoPsrx lista[Eventi::N_EVENTI];
    const uint8_t n = eventi.leggi(dopo, lista, Eventi::N_EVENTI);
    out[0] = n;
    memcpy(out + 1, lista, n * sizeof(EventoPsrx));
    return static_cast<uint16_t>(1 + n * sizeof(EventoPsrx));
}

static uint16_t reti(uint8_t *out) {
    const Config_body &c = get_config();
    RetiPsrx r{};
    for (uint8_t i = 0; i < PSRX_RETI_MAX; i++) {
        memcpy(r.reti[i].ssid, c.psrx_reti[i].ssid, sizeof r.reti[i].ssid);
        r.reti[i].auth = c.psrx_reti[i].auth;
        r.reti[i].ha_password = c.psrx_reti[i].psk[0] ? 1 : 0;
    }
    memcpy(r.wol_mac[0], c.wol_target_mac, 6);
    memcpy(r.wol_mac[1], c.wol_target_mac2, 6);
    r.wol_spento = c.psrx_wol_spento;
    memcpy(out, &r, sizeof r);
    return sizeof r;
}

static uint8_t rispondi(uint8_t comando, uint16_t indice, uint16_t *lunghezza) {
    switch (comando) {
        case CMD_INFO:         *lunghezza = info(risposta); return ERR_NESSUNO;
        case CMD_STATO:        *lunghezza = stato(risposta, (indice & STATO_SILENZIOSO) != 0); return ERR_NESSUNO;
        case CMD_IMPOSTAZIONI: *lunghezza = impostazioni(risposta); return ERR_NESSUNO;
        case CMD_ABBINATI:     *lunghezza = abbinati(risposta); return ERR_NESSUNO;
        case CMD_REGISTRO:     *lunghezza = registro(risposta, sizeof risposta); return ERR_NESSUNO;
        case CMD_EVENTI:       *lunghezza = leggi_eventi(risposta, indice); return ERR_NESSUNO;
        case CMD_RETI:         *lunghezza = reti(risposta); return ERR_NESSUNO;
        case CMD_ERRORE: {
            const ErroreRisposta e{ultimo_errore, ultimo_comando};
            memcpy(risposta, &e, sizeof e);
            *lunghezza = sizeof e;
            return ERR_NESSUNO;
        }
        case CMD_CARICAMENTO: {
            CaricamentoPsrx c{};
            aggiornamento_leggi(&c);
            memcpy(risposta, &c, sizeof c);
            *lunghezza = sizeof c;
            return ERR_NESSUNO;
        }
    }
    return ERR_COMANDO;
}

// --- scritture -------------------------------------------------------------------------

static uint8_t rinomina(const uint8_t *buf) {
    char nome[CONFIG_BOND_NAME_LEN] = {};
    memcpy(nome, buf + BT_ADDR_LEN, CONFIG_BOND_NAME_LEN - 1);
    if (!bond_esiste(buf)) return ERR_NON_TROVATO;
    if (!config_set_bond_name(buf, nome)) return ERR_PIENO;
    salvataggio_segna_modifica();
    psrx_log("app: controller %02X:%02X:%02X:%02X:%02X:%02X rinominato \"%s\"", buf[0], buf[1], buf[2], buf[3],
             buf[4], buf[5], nome);
    return ERR_NESSUNO;
}

// Lunghezza dei dati attesa per ogni scrittura (e dove riceverli).
static uint8_t prepara(uint8_t comando, uint16_t indice, uint16_t lunghezza) {
    uint16_t attesa = 0;
    destinazione = dati;
    switch (comando) {
        case CMD_IMPOSTA:          attesa = 2; break;
        case CMD_IMPOSTA_PAD:      attesa = sizeof(ImpostaPad); break;
        case CMD_RINOMINA:         attesa = BT_ADDR_LEN + CONFIG_BOND_NAME_LEN; break;
        case CMD_DIMENTICA:        attesa = BT_ADDR_LEN; break;
        case CMD_RETE_SALVA:       attesa = sizeof(ReteDati); break;
        case CMD_WOL_DESTINAZIONI: attesa = 12; break;
        case CMD_CARICA_INIZIO:    attesa = sizeof(CaricaInizio); break;
        case CMD_CARICA_BLOCCO:    return aggiornamento_prepara_blocco(indice, lunghezza, &destinazione);
        case CMD_SALVA_ORA:
        case CMD_PREDEFINITE:
        case CMD_DIMENTICA_TUTTI:
        case CMD_ABBINA:
        case CMD_SPEGNI_PAD:
        case CMD_RETE_CANCELLA:
        case CMD_WOL_PROVA:
        case CMD_CARICA_FINE:
        case CMD_CARICA_ANNULLA:
        case CMD_INSTALLA_ORA:
        case CMD_BOOTSEL_ORA:      attesa = 0; break;
        default:                   return ERR_COMANDO;
    }
    static_assert(sizeof(ReteDati) <= sizeof dati, "dati troppo piccolo");
    return lunghezza == attesa ? ERR_NESSUNO : ERR_LUNGHEZZA;
}

static uint8_t esegui(uint8_t comando, uint16_t indice, const uint8_t *buf, uint16_t lunghezza) {
    switch (comando) {
        case CMD_IMPOSTA:
            return imposta(static_cast<uint8_t>(indice), static_cast<uint16_t>(buf[0] | buf[1] << 8));
        case CMD_IMPOSTA_PAD: {
            ImpostaPad r;
            memcpy(&r, buf, sizeof r);
            return imposta_pad(r);
        }
        case CMD_SALVA_ORA:
            if (pad_connessi() > 0) return ERR_PAD_CONNESSO;
            salvataggio_forza();
            return ERR_NESSUNO;
        case CMD_PREDEFINITE:
            return predefinite();
        case CMD_RINOMINA:
            return rinomina(buf);
        case CMD_DIMENTICA:
            if (!bond_esiste(buf)) return ERR_NON_TROVATO;
            memcpy(mac_dimentica, buf, BT_ADDR_LEN);
            rich_dimentica = true;
            return ERR_NESSUNO;
        case CMD_DIMENTICA_TUTTI:
            rich_dimentica_tutti = true;
            return ERR_NESSUNO;
        case CMD_ABBINA:
            rich_abbina = true;
            return ERR_NESSUNO;
        case CMD_SPEGNI_PAD:
            if (indice == PSRX_SLOT_TUTTI) rich_spegni_tutti = true;
            else if (indice < pad_posti()) rich_spegni_posto |= static_cast<uint8_t>(1u << indice);
            else return ERR_VALORE;
            return ERR_NESSUNO;
        case CMD_RETE_SALVA: {
            ReteDati r;
            memcpy(&r, buf, sizeof r);
            return rete_salva(indice, r);
        }
        case CMD_RETE_CANCELLA:
            return rete_cancella(indice);
        case CMD_WOL_DESTINAZIONI:
            return wol_destinazioni(buf);
        case CMD_WOL_PROVA:
            if (rete_info_connessa()) {
                rich_wol = true;
                return ERR_NESSUNO;
            }
            return ERR_NON_CONNESSO;
        case CMD_CARICA_INIZIO: {
            CaricaInizio ci;
            memcpy(&ci, buf, sizeof ci);
            return aggiornamento_inizia(ci.dimensione, ci.sha256);
        }
        case CMD_CARICA_BLOCCO:
            return aggiornamento_blocco_ricevuto(indice, lunghezza);
        case CMD_CARICA_FINE:
            return aggiornamento_fine();
        case CMD_CARICA_ANNULLA:
            aggiornamento_annulla();
            return ERR_NESSUNO;
        case CMD_INSTALLA_ORA:
            return aggiornamento_installa_ora();
        case CMD_BOOTSEL_ORA:
            return aggiornamento_bootsel_ora();
    }
    return ERR_COMANDO;
}

// --- richiesta USB ---------------------------------------------------------------------

bool psrx_servizio_usb(uint8_t rhport, uint8_t fase, tusb_control_request_t const *r) {
    const uint8_t comando = static_cast<uint8_t>(r->wValue & 0xFF);
    const bool lettura = r->bmRequestType_bit.direction == TUSB_DIR_IN;

    if (fase == CONTROL_STAGE_SETUP) {
        if (r->wValue > 0xFF || lettura != (comando < 0x10)) {
            segna_errore(ERR_COMANDO, comando);
            return false;
        }
        if (lettura) {
            uint16_t n = 0;
            const uint8_t e = rispondi(comando, r->wIndex, &n);
            if (e != ERR_NESSUNO) {
                segna_errore(e, comando);
                return false;
            }
            if (n > r->wLength) n = r->wLength;
            return tud_control_xfer(rhport, r, risposta, n);
        }
        ultimo_errore = ERR_NESSUNO;
        if (r->wLength > PSRX_DATI_MAX) {
            segna_errore(ERR_LUNGHEZZA, comando);
            return false;
        }
        uint8_t e = prepara(comando, r->wIndex, r->wLength);
        if (e == ERR_NESSUNO && r->wLength == 0) {
            e = esegui(comando, r->wIndex, nullptr, 0);
            if (e == ERR_NESSUNO) return tud_control_status(rhport, r);
        }
        if (e != ERR_NESSUNO) {
            segna_errore(e, comando);
            return false;
        }
        return tud_control_xfer(rhport, r, destinazione, r->wLength);
    }

    if (fase == CONTROL_STAGE_DATA && !lettura) {
        const uint8_t e = esegui(comando, r->wIndex, destinazione, r->wLength);
        if (e != ERR_NESSUNO) {
            segna_errore(e, comando);
            return false; // STALL della fase di stato: l'app vede l'errore
        }
    }
    return true;
}

// --- ciclo principale ------------------------------------------------------------------

uint16_t servizio_usb_ultimo_evento() {
    return eventi.ultimo();
}

void servizio_usb_task() {
    const uint32_t ora = ora_ms();

    // Eventi per le notifiche: ogni 20 ms, dal buffer dei report gia' ricevuti.
    if (ora - t_eventi >= 20) {
        t_eventi = ora;
        PadPerEventi pad[PSRX_PAD_MAX];
        for (uint8_t i = 0; i < PSRX_PAD_MAX; i++) pad_info(i, pad[i]);
        eventi.aggiorna(ora, pad, get_config().psrx_modalita);
    }

    if (rich_led) {
        rich_led = false;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !get_config().disable_pico_led && pad_connessi() > 0);
    }
    if (rich_forma_usb) {
        // "Posti fissi" cambiato: stessa scelta di bt_apply_usb_variant_policy() (bt.cpp), chiesta da
        // qui per non aggiungere chiamate a quella funzione (resta copiata nel gestore L2CAP).
        rich_forma_usb = false;
        const int n = bt_connected_count();
        if (get_config().psrx_posti_fissi) usb_request_variant_fisso();
        else if (n == 0) usb_request_variant_minimal();
        else if (n == 1) usb_request_variant_full();
        else usb_request_variant_multi(static_cast<uint8_t>(n));
    }
    if (rich_abbina) {
        rich_abbina = false;
        psrx_log("app: abbinamento, finestra di 30 s (Create/Share + PS sul controller)");
        pad_avvia_abbinamento();
    }
    if (rich_dimentica) {
        rich_dimentica = false;
        psrx_log("app: dimentico il controller %02X:%02X:%02X:%02X:%02X:%02X", mac_dimentica[0], mac_dimentica[1],
                 mac_dimentica[2], mac_dimentica[3], mac_dimentica[4], mac_dimentica[5]);
        config_clear_bond_name(mac_dimentica);
        config_pad_dimentica(mac_dimentica);
        salvataggio_segna_modifica();
        bt_bond_forget(mac_dimentica);   // BTstack scrive subito in flash (abbinamento + blacklist)
    }
    if (rich_dimentica_tutti) {
        rich_dimentica_tutti = false;
        psrx_log("app: dimentico tutti i controller");
        Config_body c = get_config();
        memset(c.bond_names, 0, sizeof c.bond_names);
        memset(c.psrx_pad, 0, sizeof c.psrx_pad);
        config_imposta(c);
        salvataggio_segna_modifica();
        bt_bond_forget_all();
    }
    if (rich_spegni_tutti) {
        rich_spegni_tutti = false;
        rich_spegni_posto = 0;
        psrx_log("app: spengo tutti i controller");
        pad_spegni_tutti();
    }
    for (uint8_t posto = 0; rich_spegni_posto && posto < pad_posti(); posto++) {
        if (!(rich_spegni_posto & (1u << posto))) continue;
        rich_spegni_posto &= static_cast<uint8_t>(~(1u << posto));
        psrx_log("app: spengo il controller del posto %u", posto + 1);
        pad_spegni(posto);
    }
    if (rich_wol) {
        rich_wol = false;
        rete_invia_wol_ora();
    }

    // RSSI e heap solo mentre l'app e' aperta (ha chiesto lo stato non silenzioso negli ultimi 5 s).
    if (t_ultimo_stato == 0 || ora - t_ultimo_stato > T_APP_APERTA_MS) return;
    if (ora - t_rssi >= T_RSSI_MS) {
        t_rssi = ora;
        bt_richiedi_rssi();
    }
    if (heap_libero == 0 || ora - t_heap >= T_HEAP_MS) {
        t_heap = ora;
        extern char __StackLimit[], __bss_end__[];
        const int regione = static_cast<int>(__StackLimit - __bss_end__);
        const int usato = mallinfo().uordblks;
        heap_libero = regione > usato ? static_cast<uint32_t>(regione - usato) : 0;
    }
}
