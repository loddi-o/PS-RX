//
// PS-RX - modalita' Steam, lato USB (vedi steam_usb.h e steam.h).
//

#include "steam_usb.h"

#include <cstdio>
#include <cstring>

#include "bt.h"
#include "config.h"
#include "device/usbd_pvt.h"
#include "log_psrx.h"
#include "pad.h"
#include "pico/time.h"
#include "pico/unique_id.h"
#include "slots.h"
#include "state_mgr.h"
#include "steam.h"
#include "tusb.h"
#include "usb.h"

extern uint8_t interrupt_in_data[][63];               // main.cpp: ultimo report di ogni posto
extern volatile uint16_t psrx_intervallo_us[];        // main.cpp: polling per posto (0 = nessun limite)
void state_push_slot_to_bt(uint8_t slot);             // main.cpp

volatile bool psrx_steam_attivo = false;

namespace {

// Descrittore dei report di un posto: quello del dongle vero (mouse 0x40 e tastiera 0x41 della
// "lizard mode", ingressi 0x42-0x47/0x79/0x7B, uscite 0x80-0x89, comandi feature 1 e 2 da 63 byte).
constexpr uint8_t DESC_REPORT[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x40, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x02,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02, 0x75, 0x06, 0x95, 0x01, 0x81, 0x01, 0x05, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06, 0x95, 0x01, 0x09, 0x38,
    0x81, 0x06, 0x05, 0x0C, 0x0A, 0x38, 0x02, 0x95, 0x01, 0x81, 0x06, 0xC0, 0xC0, 0x05, 0x01, 0x09, 0x06, 0xA1,
    0x01, 0x85, 0x41, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81,
    0x02, 0x81, 0x01, 0x19, 0x00, 0x29, 0x65, 0x15, 0x00, 0x25, 0x65, 0x75, 0x08, 0x95, 0x06, 0x81, 0x00, 0xC0,
    0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x42, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x35,
    0x09, 0x42, 0x81, 0x02, 0x85, 0x44, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x05, 0x09, 0x44, 0x81,
    0x02, 0x85, 0x79, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0x09, 0x79, 0x81, 0x02, 0x85, 0x43,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x0E, 0x09, 0x43, 0x81, 0x02, 0x85, 0x7B, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x75, 0x08, 0x95, 0x0C, 0x09, 0x7B, 0x81, 0x02, 0x85, 0x45, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75,
    0x08, 0x95, 0x2D, 0x09, 0x45, 0x81, 0x02, 0x85, 0x47, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x2D,
    0x09, 0x47, 0x81, 0x02, 0x85, 0x80, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x09, 0x09, 0x80, 0x91,
    0x02, 0x85, 0x81, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x07, 0x09, 0x81, 0x91, 0x02, 0x85, 0x82,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x03, 0x09, 0x82, 0x91, 0x02, 0x85, 0x83, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x75, 0x08, 0x95, 0x09, 0x09, 0x83, 0x91, 0x02, 0x85, 0x84, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75,
    0x08, 0x95, 0x08, 0x09, 0x84, 0x91, 0x02, 0x85, 0x85, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x03,
    0x09, 0x85, 0x91, 0x02, 0x85, 0x86, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x03, 0x09, 0x86, 0x91,
    0x02, 0x85, 0x87, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x09, 0x87, 0x91, 0x02, 0x85, 0x89,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x09, 0x89, 0x91, 0x02, 0x85, 0x88, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x09, 0x88, 0x91, 0x02, 0x85, 0x01, 0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02,
    0x85, 0x02, 0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0xC0,
};
static_assert(sizeof(DESC_REPORT) == 387, "descrittore dei report del dongle");

constexpr uint8_t DESC_HID[9] = {
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, sizeof(DESC_REPORT) & 0xFF, sizeof(DESC_REPORT) >> 8,
};

constexpr uint8_t EP_LEN = 64;
constexpr uint16_t LEN_POSTO = 9 + 9 + 7 + 7;
constexpr uint16_t LEN_CONFIG = 9 + 8 + 9 + 9 + STEAM_POSTI * LEN_POSTO;

constexpr uint32_t RIPETI_79_MS = 750;          // finche' Steam non risponde dopo un collegamento
constexpr uint32_t DURATA_SCOLLEGATO_MS = 6000; // ripetizione dello "scollegato" (un report perso non conta)
constexpr uint32_t PERIODO_7B_MS = 2000;
constexpr uint32_t PERIODO_43_MS = 2000;
constexpr uint32_t SCADENZA_VIBRAZIONE_MS = 150; // Steam ripete la vibrazione: senza ripetizioni si ferma
constexpr uint64_t ATTESA_MAX_PERIODICI_US = 5000;

struct Posto {
    bool aperto;
    uint8_t ep_in, ep_out;
    // verso il PC
    uint8_t seq;
    uint8_t mandato[STEAM_LEN_STATO];
    bool forza;
    uint64_t t_invio;
    uint8_t periodico[16];       // [id][dati]: un report periodico in attesa (0x79 ha la precedenza)
    uint8_t len_periodico;
    uint64_t t_periodico;
    // collegamento
    bool collegato_usb;
    uint32_t t_bordo, t_79, t_7b, t_43, t_scollegato;
    bool steam_ha_risposto;
    // dal PC
    uint8_t risposta[STEAM_LEN_FEATURE];
    uint16_t vib_sx, vib_dx;
    uint32_t t_vibrazione;
    bool vib_da_mandare;
    bool da_spegnere;
    SteamIdentita id;
};

Posto posto[STEAM_POSTI];
CFG_TUSB_MEM_ALIGN uint8_t buf_in[STEAM_POSTI][EP_LEN];
CFG_TUSB_MEM_ALIGN uint8_t buf_out[STEAM_POSTI][EP_LEN];
CFG_TUSB_MEM_ALIGN uint8_t buf_ctrl[EP_LEN];
uint8_t cfg[LEN_CONFIG];
char seriale_dongle[16], scheda_dongle[16];
uint32_t uuid_dongle;
bool identita_pronta = false;
volatile uint32_t ultimo_segnale_steam = 0;   // ms dell'ultima scrittura di Steam (uscite o impostazioni)

uint32_t ora_ms() {
    return to_ms_since_boot(get_absolute_time());
}

void prepara_identita() {
    if (identita_pronta) return;
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    steam_seriali(uid.id, sizeof uid.id, 'B', seriale_dongle, scheda_dongle, &uuid_dongle);
    identita_pronta = true;
}

int posto_da_itf(uint16_t itf) {
    const int k = static_cast<int>(itf & 0xFF) - STEAM_PRIMA_ITF;
    return (k >= 0 && k < STEAM_POSTI && posto[k].aperto) ? k : -1;
}

// --- dal PC ------------------------------------------------------------------------------------------
void uscita(int k, uint8_t id, const uint8_t *d, uint16_t n) {
    if (id < 0x80 || id > 0x89) return;
    ultimo_segnale_steam = ora_ms();
    posto[k].steam_ha_risposto = true;
    uint16_t sx, dx;
    if (id == STEAM_ID_VIBRAZIONE && steam_vibrazione(d, n, &sx, &dx)) {
        posto[k].vib_sx = sx;
        posto[k].vib_dx = dx;
        posto[k].t_vibrazione = ora_ms();
        posto[k].vib_da_mandare = true;
    }
    // 0x81-0x86: impulsi aptici dei trackpad (il DualSense non ha trackpad aptici): ignorati.
}

void comando(int k, uint8_t id, const uint8_t *d, uint16_t n) {
    Posto &p = posto[k];
    const uint8_t az = steam_comando(id, d, n, p.id, p.risposta);
    if (az == STEAM_AZ_SPEGNI) p.da_spegnere = true;
    if (az == STEAM_AZ_STEAM_ATTIVO) {
        ultimo_segnale_steam = ora_ms();
        p.steam_ha_risposto = true;
    }
}

// --- driver di classe ------------------------------------------------------------------------------------
void s_init() {
    memset(posto, 0, sizeof posto);
}

bool s_deinit() {
    return true;
}

void s_reset(uint8_t) {
    for (auto &p : posto) p.aperto = false;
    psrx_steam_attivo = false;
}

uint16_t s_open(uint8_t rhport, tusb_desc_interface_t const *d, uint16_t max_len) {
    if (!usb_steam_servita() || d->bInterfaceClass != TUSB_CLASS_HID) return 0;
    const int k = static_cast<int>(d->bInterfaceNumber) - STEAM_PRIMA_ITF;
    if (k < 0 || k >= STEAM_POSTI || d->bNumEndpoints != 2 || max_len < LEN_POSTO) return 0;
    uint8_t const *ep = reinterpret_cast<uint8_t const *>(d) + 9 + 9;   // dopo interfaccia e descrittore HID
    uint8_t ep_out = 0, ep_in = 0;
    if (!usbd_open_edpt_pair(rhport, ep, 2, TUSB_XFER_INTERRUPT, &ep_out, &ep_in)) return 0;
    Posto &p = posto[k];
    p.aperto = true;
    p.ep_in = ep_in;
    p.ep_out = ep_out;
    p.forza = true;
    p.len_periodico = 0;
    p.collegato_usb = false;
    usbd_edpt_xfer(rhport, ep_out, buf_out[k], EP_LEN, false);
    psrx_steam_attivo = true;
    return LEN_POSTO;
}

bool s_control(uint8_t rhport, uint8_t stage, tusb_control_request_t const *r) {
    if (r->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) return false;
    const int k = posto_da_itf(r->wIndex);
    if (k < 0) return false;
    if (r->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
        if (r->bRequest != TUSB_REQ_GET_DESCRIPTOR) return false;
        if (stage != CONTROL_STAGE_SETUP) return true;
        const uint8_t tipo = static_cast<uint8_t>(r->wValue >> 8);
        if (tipo == HID_DESC_TYPE_REPORT) return tud_control_xfer(rhport, r, (void *) DESC_REPORT, sizeof DESC_REPORT);
        if (tipo == HID_DESC_TYPE_HID) return tud_control_xfer(rhport, r, (void *) DESC_HID, sizeof DESC_HID);
        return false;
    }
    if (r->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) return false;
    const uint8_t tipo = static_cast<uint8_t>(r->wValue >> 8), id = static_cast<uint8_t>(r->wValue & 0xFF);
    switch (r->bRequest) {
        case HID_REQ_CONTROL_GET_REPORT: {
            if (stage != CONTROL_STAGE_SETUP) return true;
            if (tipo != HID_REPORT_TYPE_FEATURE) return false;
            buf_ctrl[0] = id;
            memcpy(buf_ctrl + 1, posto[k].risposta, STEAM_LEN_FEATURE);
            const uint16_t n = r->wLength < EP_LEN ? r->wLength : EP_LEN;
            return tud_control_xfer(rhport, r, buf_ctrl, n);
        }
        case HID_REQ_CONTROL_SET_REPORT: {
            if (stage == CONTROL_STAGE_SETUP) {
                if (r->wLength > EP_LEN) return false;
                return tud_control_xfer(rhport, r, buf_ctrl, r->wLength);
            }
            if (stage == CONTROL_STAGE_DATA) {
                const uint8_t *d = buf_ctrl;
                uint16_t n = r->wLength;
                if (id && n && d[0] == id) {   // Report ID in testa ai dati
                    d++;
                    n--;
                }
                if (tipo == HID_REPORT_TYPE_FEATURE) comando(k, id, d, n);
                else if (tipo == HID_REPORT_TYPE_OUTPUT) uscita(k, id, d, n);
            }
            return true;
        }
        case HID_REQ_CONTROL_SET_IDLE:
        case HID_REQ_CONTROL_SET_PROTOCOL:
            if (stage == CONTROL_STAGE_SETUP) tud_control_status(rhport, r);
            return true;
        case HID_REQ_CONTROL_GET_IDLE:
        case HID_REQ_CONTROL_GET_PROTOCOL:
            if (stage == CONTROL_STAGE_SETUP) {
                buf_ctrl[0] = r->bRequest == HID_REQ_CONTROL_GET_PROTOCOL ? 1 : 0;
                return tud_control_xfer(rhport, r, buf_ctrl, 1);
            }
            return true;
        default:
            return false;
    }
}

bool s_xfer(uint8_t rhport, uint8_t ep, xfer_result_t risultato, uint32_t n) {
    for (int k = 0; k < STEAM_POSTI; k++) {
        if (!posto[k].aperto || ep != posto[k].ep_out) continue;
        if (risultato == XFER_RESULT_SUCCESS && n >= 1) uscita(k, buf_out[k][0], buf_out[k] + 1, static_cast<uint16_t>(n - 1));
        usbd_edpt_xfer(rhport, ep, buf_out[k], EP_LEN, false);
        return true;
    }
    return true;   // report verso il PC consegnato
}

bool manda(int k, const uint8_t *dati, uint8_t n) {
    const uint8_t ep = posto[k].ep_in;
    if (!usbd_edpt_claim(0, ep)) return false;   // report precedente non ancora letto dal PC
    memcpy(buf_in[k], dati, n);
    if (usbd_edpt_xfer(0, ep, buf_in[k], n, false)) return true;
    usbd_edpt_release(0, ep);
    return false;
}

void accoda_periodico(int k, uint8_t id, const uint8_t *d, uint8_t n) {
    Posto &p = posto[k];
    if (p.len_periodico && p.periodico[0] == STEAM_ID_COLLEGAMENTO && id != STEAM_ID_COLLEGAMENTO) return;
    p.periodico[0] = id;
    memcpy(p.periodico + 1, d, n);
    p.len_periodico = static_cast<uint8_t>(n + 1);
    p.t_periodico = time_us_64();
}

} // namespace

extern const usbd_class_driver_t psrx_driver_steam = {
    "PSRX-STEAM", s_init, s_deinit, s_reset, s_open, s_control, s_xfer, nullptr, nullptr,
};

const uint8_t *steam_descrittore_configurazione() {
    uint8_t *q = cfg;
    const uint8_t intestazione[9] = {0x09, 0x02, LEN_CONFIG & 0xFF, LEN_CONFIG >> 8, 2 + STEAM_POSTI, 0x01, 0x00,
                                     0xE0, 0xFA};
    memcpy(q, intestazione, 9);
    q += 9;
    // Interfacce 0-1: configurazione PS-RX, una sola funzione (IAD) dove il dongle vero ha la seriale.
    const uint8_t iad[8] = {0x08, 0x0B, 0x00, 0x02, 0xFF, 0x50, 0x00, 0x00};
    const uint8_t itf0[9] = {0x09, 0x04, 0x00, 0x00, 0x00, 0xFF, 0x50, 0x00, 0x04};
    const uint8_t itf1[9] = {0x09, 0x04, 0x01, 0x00, 0x00, 0xFF, 0x50, 0x01, 0x00};
    memcpy(q, iad, 8);
    q += 8;
    memcpy(q, itf0, 9);
    q += 9;
    memcpy(q, itf1, 9);
    q += 9;
    for (uint8_t k = 0; k < STEAM_POSTI; k++) {
        const uint8_t itf = static_cast<uint8_t>(STEAM_PRIMA_ITF + k);
        const uint8_t blocco[LEN_POSTO] = {
            0x09, 0x04, itf, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
            0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, sizeof(DESC_REPORT) & 0xFF, sizeof(DESC_REPORT) >> 8,
            0x07, 0x05, static_cast<uint8_t>(0x81 + k), 0x03, EP_LEN, 0x00, 0x01,
            0x07, 0x05, static_cast<uint8_t>(0x01 + k), 0x03, EP_LEN, 0x00, 0x01,
        };
        memcpy(q, blocco, sizeof blocco);
        q += sizeof blocco;
    }
    return cfg;
}

const char *steam_seriale_usb() {
    prepara_identita();
    return seriale_dongle;
}

void steam_invia() {
    if (!tud_ready()) return;
    const uint64_t ora = time_us_64();
    for (int k = 0; k < STEAM_POSTI; k++) {
        Posto &p = posto[k];
        if (!p.aperto) continue;
        // Un report periodico in attesa da troppo passa davanti all'input (succede di rado: ogni 2 s).
        if (p.len_periodico && ora - p.t_periodico > ATTESA_MAX_PERIODICI_US) {
            if (manda(k, p.periodico, p.len_periodico)) p.len_periodico = 0;
            continue;
        }
        if (p.collegato_usb) {
            const uint8_t *ds = interrupt_in_data[k];
            uint32_t ts_ds;
            memcpy(&ts_ds, ds + 27, sizeof ts_ds);   // timestamp dei sensori del DualSense (1/3 di us)
            uint8_t r[1 + STEAM_LEN_STATO];
            r[0] = STEAM_ID_STATO;
            steam_da_dualsense(ds, p.seq, ts_ds / 3, r + 1);
            const bool nuovo = p.forza || memcmp(r + 2, p.mandato + 1, STEAM_LEN_STATO - 1) != 0;
            const uint16_t intervallo = psrx_intervallo_us[k];
            if (nuovo && !(intervallo && ora - p.t_invio < intervallo)) {
                if (manda(k, r, sizeof r)) {
                    memcpy(p.mandato, r + 1, STEAM_LEN_STATO);
                    p.seq++;
                    p.forza = false;
                    p.t_invio = ora;
                }
                continue;
            }
        }
        if (p.len_periodico && manda(k, p.periodico, p.len_periodico)) p.len_periodico = 0;
    }
}

void steam_task(uint32_t ora) {
    if (!psrx_steam_attivo) return;
    prepara_identita();
    const bool sospeso = usb_host_suspended();
    for (int k = 0; k < STEAM_POSTI; k++) {
        Posto &p = posto[k];
        if (!p.aperto) continue;
        PadPerEventi info;
        pad_info(static_cast<uint8_t>(k), info);
        const bool collegato = info.connesso;

        // identita' del posto (serve alle risposte ai comandi di Steam)
        memcpy(p.id.seriale_dongle, seriale_dongle, sizeof seriale_dongle);
        memcpy(p.id.scheda_dongle, scheda_dongle, sizeof scheda_dongle);
        p.id.uuid_dongle = uuid_dongle;
        if (collegato && !p.id.collegato) {
            steam_seriali(info.mac, sizeof info.mac, 'A', p.id.seriale_pad, p.id.scheda_pad, &p.id.uuid_pad);
        }
        p.id.collegato = collegato;

        // comandi arrivati da Steam
        if (p.da_spegnere) {
            p.da_spegnere = false;
            if (collegato) {
                psrx_log("Steam: spengo il controller nel posto %d", k + 1);
                pad_spegni(static_cast<uint8_t>(k));
            }
        }
        if (p.vib_da_mandare || ((p.vib_sx || p.vib_dx) && ora - p.t_vibrazione > SCADENZA_VIBRAZIONE_MS)) {
            if (!p.vib_da_mandare) p.vib_sx = p.vib_dx = 0;   // Steam ha smesso di ripeterla: si ferma
            p.vib_da_mandare = false;
            state_imposta_vibrazione(static_cast<uint8_t>(k), static_cast<uint8_t>(p.vib_sx >> 8),
                                     static_cast<uint8_t>(p.vib_dx >> 8));
            if (collegato) state_push_slot_to_bt(static_cast<uint8_t>(k));
        }
        if (sospeso) continue;   // PC in sospensione: niente report periodici

        // 0x79: collegato/scollegato, sul bordo e poi ripetuto finche' serve
        bool manda79 = false;
        if (collegato != p.collegato_usb) {
            manda79 = true;
            p.collegato_usb = collegato;
            p.t_bordo = ora;
            p.steam_ha_risposto = false;
            p.forza = true;
            if (!collegato) p.t_scollegato = ora;
        } else if (collegato && !p.steam_ha_risposto && ora - p.t_79 >= RIPETI_79_MS) {
            manda79 = true;
        } else if (!collegato && ora - p.t_scollegato < DURATA_SCOLLEGATO_MS && ora - p.t_79 >= RIPETI_79_MS) {
            manda79 = true;
        }
        if (manda79) {
            const uint8_t stato = collegato ? 0x02 : 0x01;
            accoda_periodico(k, STEAM_ID_COLLEGAMENTO, &stato, 1);
            p.t_79 = ora;
            continue;
        }
        if (!collegato) continue;
        if (ora - p.t_7b >= PERIODO_7B_MS) {
            uint8_t s[12];
            const int8_t rssi = pad_rssi(static_cast<uint8_t>(k));
            steam_stato_link(rssi == 127 ? 0 : rssi, s);
            accoda_periodico(k, STEAM_ID_STATO_LINK, s, sizeof s);
            p.t_7b = ora;
        } else if (info.batteria_valida && ora - p.t_43 >= PERIODO_43_MS) {
            uint8_t b[14];
            steam_batteria(info.batteria, info.in_carica, info.in_carica && info.batteria >= 100, b);
            accoda_periodico(k, STEAM_ID_BATTERIA, b, sizeof b);
            p.t_43 = ora;
        }
    }
}
