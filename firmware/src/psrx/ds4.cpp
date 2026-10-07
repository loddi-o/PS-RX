//
// PS-RX - DualShock 4 presentato come DualSense (vedi ds4.h).
//
// Report del DualShock 4 (formato USB, hid-sony): [1..4] sticks, [5] croce direzionale + tasti,
// [6] L1 R1 L2 R2 Share Options L3 R3, [7] PS, click del touchpad, contatore (6 bit), [8..9] L2 R2,
// [10..11] timestamp (5,33 us), [13..18] giroscopio, [19..24] accelerometro, [30] batteria
// (bit 0-3 livello, bit 4 cavo), [35..38] e [39..42] i due punti del touchpad.
// Via Bluetooth il report 0x11 arriva come A1 11 xx yy + lo stesso contenuto da [1]: byte USB k = bt[k + 3].
//
// Report del DualSense (USB, 63 byte senza Report ID, hid-playstation): [0..3] sticks, [4..5] L2 R2,
// [6] contatore, [7..10] tasti (stesso schema del DualShock 4 nei primi due byte), [15..20]
// giroscopio, [21..26] accelerometro, [27..30] timestamp (0,33 us), [32..35] e [36..39] touchpad,
// [52] batteria (bit 0-3 livello 0-10, bit 4-7: 0 scarica, 1 in carica, 2 carica completa).
//

#include "ds4.h"

#include <cstring>

namespace {
inline uint8_t u(const uint8_t *bt, int k) { return bt[k + 3]; }   // byte k del report USB del DualShock 4

// Punto del touchpad: stesso formato (byte 0 bit 7 = nessun tocco, 12 bit x e 12 bit y); la y del
// DualShock 4 arriva a 942, quella del DualSense a 1079.
void PSRX_RAM(punto)(const uint8_t *da, uint8_t *a) {
    a[0] = da[0];
    const uint16_t x = static_cast<uint16_t>(da[1] | (da[2] & 0x0F) << 8);
    uint32_t y = static_cast<uint32_t>((da[2] >> 4) | da[3] << 4);
    y = y * 1079u / 942u;
    if (y > 1079u) y = 1079u;
    a[1] = static_cast<uint8_t>(x & 0xFF);
    a[2] = static_cast<uint8_t>((x >> 8) | (y & 0x0F) << 4);
    a[3] = static_cast<uint8_t>(y >> 4);
}
} // namespace

bool PSRX_RAM(ds4_in_dualsense)(const uint8_t *bt, uint16_t lunghezza, const uint8_t neutro[DS5_LUNGHEZZA_REPORT],
                      StatoDs4 &stato, uint8_t out[DS5_LUNGHEZZA_REPORT]) {
    if (lunghezza < DS4_LUNGHEZZA_MIN || bt[1] != DS4_REPORT_BT) return false;
    memcpy(out, neutro, DS5_LUNGHEZZA_REPORT);
    out[0] = u(bt, 1);                 // sticks
    out[1] = u(bt, 2);
    out[2] = u(bt, 3);
    out[3] = u(bt, 4);
    out[4] = u(bt, 8);                 // grilletti
    out[5] = u(bt, 9);
    out[6] = static_cast<uint8_t>(u(bt, 7) >> 2);   // contatore
    out[7] = u(bt, 5);                 // croce direzionale + quadrato, croce, cerchio, triangolo
    out[8] = u(bt, 6);                 // L1 R1 L2 R2 Share(=Create) Options L3 R3
    out[9] = static_cast<uint8_t>(u(bt, 7) & 0x03);  // PS, click del touchpad (niente tasto muto)
    memcpy(&out[15], &bt[13 + 3], 12); // giroscopio e accelerometro, stesso ordine

    // Timestamp: 16 bit da 5,33 us -> 32 bit da 0,33 us (x16), continuo fra un report e l'altro.
    const uint16_t ts = static_cast<uint16_t>(u(bt, 10) | u(bt, 11) << 8);
    if (stato.primo) {
        stato.primo = false;
        stato.ultimo_ts = ts;
    }
    stato.ts32 += static_cast<uint32_t>(static_cast<uint16_t>(ts - stato.ultimo_ts)) * 16u;
    stato.ultimo_ts = ts;
    memcpy(&out[27], &stato.ts32, 4);

    punto(&bt[35 + 3], &out[32]);
    punto(&bt[39 + 3], &out[36]);

    // Batteria: livello 0-10 (11 = carica completa col cavo), bit 4 = cavo collegato.
    const uint8_t b = u(bt, 30);
    uint8_t livello = b & 0x0F;
    const bool cavo = b & 0x10;
    uint8_t carica = 0;
    if (cavo) carica = livello >= 11 ? 2 : 1;
    if (livello > 10) livello = 10;
    out[52] = static_cast<uint8_t>(livello | carica << 4);
    return true;
}

void PSRX_RAM(ds4_uscita)(const uint8_t *stato, uint8_t intervallo_ms, uint8_t out[DS4_LUNGHEZZA_USCITA]) {
    memset(out, 0, DS4_LUNGHEZZA_USCITA);
    out[0] = DS4_REPORT_BT;
    out[1] = static_cast<uint8_t>(0xC0 | (intervallo_ms & 0x3F));   // HID + CRC, intervallo dei report
    out[3] = 0x07;                     // vale: lampeggio, lightbar, motori
    // SetStateData: [2] motore destro (leggero), [3] sinistro (pesante); [44..46] colore.
    out[6] = stato[2];
    out[7] = stato[3];
    out[8] = stato[44];
    out[9] = stato[45];
    out[10] = stato[46];
}

void ds4_calibrazione_in_dualsense(uint8_t *dati, uint16_t lunghezza) {
    if (lunghezza < 19) return;
    // dati[7..18]: 6 valori da 16 bit
    uint8_t v[12];
    memcpy(v, &dati[7], 12);
    // DualShock 4: pitch+ yaw+ roll+ pitch- yaw- roll-  ->  DualSense: pitch+ pitch- yaw+ yaw- roll+ roll-
    const int ordine[6] = {0, 3, 1, 4, 2, 5};
    for (int i = 0; i < 6; i++) memcpy(&dati[7 + i * 2], &v[ordine[i] * 2], 2);
}

void ds4_feature_09(const uint8_t mac[6], uint8_t out[DS5_LUNGHEZZA_FEATURE_09]) {
    memset(out, 0, DS5_LUNGHEZZA_FEATURE_09);
    out[0] = 0x09;
    for (int i = 0; i < 6; i++) out[1 + i] = mac[5 - i];   // il DualSense lo da' in little endian
}

void ds4_feature_20(uint8_t out[DS5_LUNGHEZZA_FEATURE_20]) {
    memset(out, 0, DS5_LUNGHEZZA_FEATURE_20);
    out[0] = 0x20;
    memcpy(&out[1], "Jan  1 2026", 11);       // data e ora di compilazione (testo)
    memcpy(&out[12], "00:00:00", 8);
    // hid-playstation legge: hardware_version (le32 @24), firmware_version (le32 @28),
    // update_version (le16 @44). update_version >= 0x0224 abilita il rumble "migliorato".
    const uint32_t hw = 0x00000412, fw = 0x01000024;
    const uint16_t upd = 0x0224;
    memcpy(&out[24], &hw, 4);
    memcpy(&out[28], &fw, 4);
    memcpy(&out[44], &upd, 2);
}
