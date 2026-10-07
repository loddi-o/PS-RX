//
// PS-RX - modalita' Steam: traduzioni e comandi (vedi steam.h).
//

#include "steam.h"

#include <cstdio>
#include <cstring>

#include "trackpad.h"

namespace {

// Attributi (comando 0x83): [tag][valore uint32 LE]. Quelli del controller sono letti da uno Steam
// Controller vero (28DE:1302, documentati da openpuck): Steam li confronta byte per byte. Quelli del
// dongle sono del dongle 28DE:1304.
constexpr uint8_t ATTR_PAD[25] = {
    0x01, 0x02, 0x13, 0x00, 0x00,   // prodotto 0x1302
    0x02, 0x00, 0x00, 0x00, 0x00,   // capacita'
    0x0A, 0x2E, 0xF9, 0xD2, 0x68,   // build del bootloader
    0x04, 0x57, 0xD0, 0x18, 0x6A,   // build del firmware
    0x09, 0x48, 0x00, 0x00, 0x00,   // revisione della scheda
};
constexpr uint8_t ATTR_DONGLE[25] = {
    0x01, 0x04, 0x13, 0x00, 0x00,   // prodotto 0x1304
    0x02, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0xF2, 0xF9, 0xD2, 0x68,
    0x04, 0x59, 0x83, 0x62, 0x6A,
    0x09, 0x47, 0x00, 0x00, 0x00,
};

constexpr uint8_t CMD_ATTRIBUTI = 0x83;
constexpr uint8_t CMD_IMPOSTAZIONI = 0x87;
constexpr uint8_t CMD_SPEGNI = 0x9F;
constexpr uint8_t CMD_SCRIVI_LEGAME = 0xA2;
constexpr uint8_t CMD_LEGGI_LEGAME = 0xA3;
constexpr uint8_t CMD_ABBINAMENTO = 0xAD;
constexpr uint8_t CMD_STRINGA = 0xAE;
constexpr uint8_t CMD_STATO_RADIO = 0xB4;
constexpr uint8_t IMP_LIZARD = 9;

void scrivi16(uint8_t *p, int32_t v) {
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    const uint16_t u = static_cast<uint16_t>(static_cast<int16_t>(v));
    p[0] = static_cast<uint8_t>(u & 0xFF);
    p[1] = static_cast<uint8_t>(u >> 8);
}

void scrivi16u(uint8_t *p, uint16_t u) {
    p[0] = static_cast<uint8_t>(u & 0xFF);
    p[1] = static_cast<uint8_t>(u >> 8);
}

int16_t leggi16(const uint8_t *p) {
    return static_cast<int16_t>(p[0] | p[1] << 8);
}

// Byte del DualSense (centro 128) -> asse dello Steam Controller (Y positivo in alto).
int32_t asse(uint8_t v, bool inverti) {
    const int32_t a = (static_cast<int32_t>(v) - 128) * 258;
    return inverti ? -a : a;
}

// Croce del DualSense -> bit dello Steam Controller.
constexpr uint32_t CROCE[9] = {
    TRI_SU, TRI_SU | TRI_DESTRA, TRI_DESTRA, TRI_GIU | TRI_DESTRA, TRI_GIU, TRI_GIU | TRI_SINISTRA, TRI_SINISTRA,
    TRI_SU | TRI_SINISTRA, 0,
};

// Touchpad del DualSense: 1920 x 1080 punti. Ogni meta' (960 x 1080) diventa un trackpad (-32768..32767).
constexpr int32_t META = 960;
constexpr int32_t ALTEZZA = 1080;
constexpr uint16_t PRESSIONE_TOCCO = 0x0400;   // bassa: Steam puo' ricavare il click dalla pressione
constexpr uint16_t PRESSIONE_CLICK = 0x7FFF;

void stringa(uint8_t *r, uint8_t indice, const char *s) {
    r[0] = CMD_STRINGA;
    r[1] = 0x14;
    r[2] = indice;
    size_t n = strlen(s);   // stringhe sempre terminate (al massimo 15 caratteri)
    if (n > 0x13) n = 0x13;
    memcpy(r + 3, s, n);
}

} // namespace

void steam_da_dualsense(const uint8_t *ds, uint8_t seq, uint32_t imu_us, uint8_t *p) {
    memset(p, 0, STEAM_LEN_STATO);
    const uint8_t b0 = ds[7], b1 = ds[8], b2 = ds[9];
    const uint8_t croce = b0 & 0x0F;
    uint32_t t = croce < 9 ? CROCE[croce] : 0;
    if (b0 & 0x20) t |= TRI_A;               // croce
    if (b0 & 0x40) t |= TRI_B;               // cerchio
    if (b0 & 0x10) t |= TRI_X;               // quadrato
    if (b0 & 0x80) t |= TRI_Y;               // triangolo
    if (b1 & 0x01) t |= TRI_LB;
    if (b1 & 0x02) t |= TRI_RB;
    if (b1 & 0x10) t |= TRI_VIEW;            // Create / Share
    if (b1 & 0x20) t |= TRI_MENU;            // Options
    if (b1 & 0x40) t |= TRI_L3;
    if (b1 & 0x80) t |= TRI_R3;
    if (b2 & 0x01) t |= TRI_STEAM;           // PS
    if (b2 & 0x04) t |= TRI_QAM;             // microfono -> menu rapido
    if (b2 & 0x40) t |= TRI_L4;              // DualSense Edge: levette posteriori e tasti Fn
    if (b2 & 0x80) t |= TRI_R4;
    if (b2 & 0x10) t |= TRI_L5;
    if (b2 & 0x20) t |= TRI_R5;
    if (ds[4] >= 250) t |= TRI_CLICK_GRILLETTO_SX;
    if (ds[5] >= 250) t |= TRI_CLICK_GRILLETTO_DX;

    // Trackpad: ogni dito va al lato in cui si trova.
    const StatoTouchpad tp = touchpad_da_report(ds);
    bool sx = false, dx = false;
    for (const auto &d : tp.dito) {
        if (!d.attivo) continue;
        const bool destra = d.x >= META;
        const int32_t x = destra ? d.x - META : d.x;
        const int32_t px = (x > META - 1 ? META - 1 : x) * 65535 / (META - 1) - 32768;
        const int32_t y = d.y > ALTEZZA - 1 ? ALTEZZA - 1 : d.y;
        const int32_t py = 32767 - y * 65535 / (ALTEZZA - 1);
        uint8_t *q = p + (destra ? 23 : 17);   // X, Y, pressione
        if ((destra && dx) || (!destra && sx)) continue;   // due dita sullo stesso lato: vale la prima
        scrivi16(q, px);
        scrivi16(q + 2, py);
        scrivi16u(q + 4, tp.click ? PRESSIONE_CLICK : PRESSIONE_TOCCO);
        (destra ? dx : sx) = true;
    }
    if (sx) t |= TRI_TOCCO_PAD_SX;
    if (dx) t |= TRI_TOCCO_PAD_DX;
    if (tp.click) {
        if (sx) t |= TRI_CLICK_PAD_SX;
        if (dx || !sx) t |= TRI_CLICK_PAD_DX;   // click senza dita registrate: destro
    }

    p[0] = seq;
    p[1] = static_cast<uint8_t>(t & 0xFF);
    p[2] = static_cast<uint8_t>(t >> 8);
    p[3] = static_cast<uint8_t>(t >> 16);
    p[4] = static_cast<uint8_t>(t >> 24);
    scrivi16u(p + 5, static_cast<uint16_t>(ds[4] << 7));    // grilletti 0..32640
    scrivi16u(p + 7, static_cast<uint16_t>(ds[5] << 7));
    scrivi16(p + 9, asse(ds[0], false));
    scrivi16(p + 11, asse(ds[1], true));
    scrivi16(p + 13, asse(ds[2], false));
    scrivi16(p + 15, asse(ds[3], true));

    // Sensori: timestamp in microsecondi; giroscopio con la stessa scala (circa 16,4 punti per grado/s),
    // accelerometro: il DualSense ha 8192 punti per g, lo Steam Controller 16384. Assi: SDL legge lo
    // Steam Controller come (X, Z, -Y) e il DualSense come (X, Y, Z).
    p[29] = static_cast<uint8_t>(imu_us);
    p[30] = static_cast<uint8_t>(imu_us >> 8);
    p[31] = static_cast<uint8_t>(imu_us >> 16);
    p[32] = static_cast<uint8_t>(imu_us >> 24);
    const int32_t gx = leggi16(ds + 15), gy = leggi16(ds + 17), gz = leggi16(ds + 19);
    const int32_t ax = leggi16(ds + 21), ay = leggi16(ds + 23), az = leggi16(ds + 25);
    scrivi16(p + 33, ax * 2);
    scrivi16(p + 35, -az * 2);
    scrivi16(p + 37, ay * 2);
    scrivi16(p + 39, gx);
    scrivi16(p + 41, -gz);
    scrivi16(p + 43, gy);
}

bool steam_vibrazione(const uint8_t *d, uint32_t n, uint16_t *sinistro, uint16_t *destro) {
    // tipo (1), intensita' (2), sinistro: velocita' (2) guadagno (1), destro: velocita' (2) guadagno (1)
    if (n < 9) return false;
    *sinistro = static_cast<uint16_t>(d[3] | d[4] << 8);
    *destro = static_cast<uint16_t>(d[6] | d[7] << 8);
    return true;
}

uint8_t steam_comando(uint8_t report_id, const uint8_t *dati, uint16_t n, const SteamIdentita &id, uint8_t *r) {
    memset(r, 0, STEAM_LEN_FEATURE);
    if (n < 1) return STEAM_AZ_NESSUNA;
    const uint8_t cmd = dati[0];
    const uint8_t len = n > 1 ? dati[1] : 0;
    const uint8_t *pl = dati + 2;
    const uint16_t pln = n > 2 ? static_cast<uint16_t>(n - 2) : 0;
    const bool al_pad = report_id == 1;
    uint8_t azione = STEAM_AZ_NESSUNA;

    switch (cmd) {
        case CMD_ATTRIBUTI:
            r[0] = cmd;
            r[1] = sizeof ATTR_PAD;
            memcpy(r + 2, al_pad ? ATTR_PAD : ATTR_DONGLE, sizeof ATTR_PAD);
            return azione;
        case CMD_STRINGA: {
            const uint8_t indice = pln ? pl[0] : 1;
            const char *s = "NA";
            if (al_pad) {
                if (indice == 0) s = id.scheda_pad;
                else if (indice == 1) s = id.seriale_pad;
                else if (indice == 3) s = "7054257d2da7";       // costante di Valve che Steam controlla
            } else {
                if (indice == 0 || indice == 4) s = id.scheda_dongle;
                else if (indice == 1) s = id.seriale_dongle;
                else if (indice == 3) s = "PS-RX";
            }
            stringa(r, indice, s);
            return azione;
        }
        case CMD_STATO_RADIO:
            r[0] = cmd;
            r[1] = 1;
            r[2] = id.collegato ? 0x02 : 0x01;
            return azione;
        case CMD_LEGGI_LEGAME:
            if (al_pad) break;
            r[0] = cmd;
            r[1] = 24;
            if (id.collegato) {   // legame finto ma coerente: uuid del dongle, uuid e seriale del controller
                memcpy(r + 2, &id.uuid_dongle, 4);
                memcpy(r + 6, &id.uuid_pad, 4);
                memcpy(r + 10, id.seriale_pad, strnlen(id.seriale_pad, 16));
            }
            return azione;
        case CMD_SCRIVI_LEGAME:
        case CMD_ABBINAMENTO:
            if (al_pad) break;
            r[0] = cmd;   // abbinamento di Steam Controller veri: non supportato, solo conferma
            return azione;
        case CMD_SPEGNI:
            if (al_pad) azione = STEAM_AZ_SPEGNI;
            break;
        case CMD_IMPOSTAZIONI:
            for (uint16_t o = 0; o + 2 < pln && o + 2 < len; o += 3) {
                if (pl[o] == IMP_LIZARD && pl[o + 1] == 0 && pl[o + 2] == 0) azione = STEAM_AZ_STEAM_ATTIVO;
            }
            break;
        default:
            break;
    }
    // Tutto il resto (impostazioni, mappature, e anche riavvio e aggiornamento del firmware, che qui non
    // fanno nulla): eco del comando, come il dongle quando non c'e' una risposta del controller.
    r[0] = cmd;
    r[1] = len;
    if (pln) memcpy(r + 2, pl, pln > 60 ? 60 : pln);
    return azione;
}

void steam_batteria(uint8_t livello, bool in_carica, bool carica_completa, uint8_t *p) {
    memset(p, 0, 14);
    p[0] = carica_completa ? 4 : in_carica ? 2 : 1;   // EChargeState: scarica, in carica, carica completa
    p[1] = livello > 100 ? 100 : livello;
}

void steam_stato_link(int8_t rssi_dbm, uint8_t *p) {
    // Modello catturato da un dongle vero (openpuck); il byte 8 e' il segnale in dBm.
    constexpr uint8_t MODELLO[12] = {0xF7, 0x01, 0x89, 0x00, 0x00, 0x00, 0x03, 0x00, 0xDD, 0x00, 0x3A, 0x02};
    memcpy(p, MODELLO, sizeof MODELLO);
    if (rssi_dbm < 0) p[8] = static_cast<uint8_t>(rssi_dbm);
}

void steam_seriali(const uint8_t *id, uint8_t n, char prefisso, char *seriale, char *scheda, uint32_t *uuid) {
    uint32_t h = 2166136261u;   // FNV-1a
    for (uint8_t i = 0; i < n; i++) h = (h ^ id[i]) * 16777619u;
    snprintf(seriale, 16, "FX%c99602%05lX", prefisso, static_cast<unsigned long>(h & 0xFFFFF));
    snprintf(scheda, 16, "MX%c99602%05lX", prefisso, static_cast<unsigned long>(h & 0xFFFFF));
    *uuid = h;
}
