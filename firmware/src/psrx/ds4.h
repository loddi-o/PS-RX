//
// PS-RX - DualShock 4 via Bluetooth presentato all'host come DualSense (logica pura, testata sul PC).
//
// Un solo dispositivo USB non puo' avere due identita' Sony diverse su Linux (hid-playstation e
// hid-sony scelgono per VID/PID), quindi in modalita' PlayStation il DualShock 4 diventa un DualSense:
// - ingresso: report Bluetooth 0x11 -> report USB 0x01 del DualSense (63 byte senza Report ID);
// - uscita: stato del DualSense (SetStateData) -> report Bluetooth 0x11 del DualShock 4 (rumble,
//   lightbar); l'audio e il microfono del DualShock 4 via Bluetooth non sono supportati;
// - report "feature" che l'host legge all'avvio (hid-playstation non si aggancia senza): calibrazione
//   0x05 riordinata, MAC 0x09 e versione 0x20 sintetici.
//
// Riferimenti: driver Linux hid-sony (DualShock 4) e hid-playstation (DualSense).
//

#ifndef PSRX_DS4_H
#define PSRX_DS4_H

#include <cstdint>

// Funzioni del percorso degli input: in RAM nel firmware (niente attese della flash XIP).
#ifdef PSRX_TEST_HOST
#define PSRX_RAM(nome) nome
#else
#include "pico.h"
#define PSRX_RAM(nome) __not_in_flash_func(nome)
#endif

constexpr uint8_t DS4_REPORT_BT = 0x11;          // report d'ingresso e d'uscita via Bluetooth
constexpr uint8_t DS4_FEATURE_FIRMWARE = 0xA3;   // solo il DualShock 4 lo ha: serve a riconoscerlo
constexpr uint16_t DS4_LUNGHEZZA_MIN = 3 + 43;   // A1 11 xx + dati fino al secondo punto del touchpad
constexpr uint8_t DS5_LUNGHEZZA_REPORT = 63;     // report USB del DualSense senza Report ID

// Contatori del report tradotto (uno per controller).
struct StatoDs4 {
    uint16_t ultimo_ts = 0;
    uint32_t ts32 = 0;
    bool primo = true;
};

// bt: pacchetto L2CAP dell'interrupt (A1 11 ...). Scrive in 'out' il report USB del DualSense,
// partendo da 'neutro' (sticks al centro, niente tasti). false se il pacchetto non e' un 0x11 valido.
bool ds4_in_dualsense(const uint8_t *bt, uint16_t lunghezza, const uint8_t neutro[DS5_LUNGHEZZA_REPORT],
                      StatoDs4 &stato, uint8_t out[DS5_LUNGHEZZA_REPORT]);

// stato: SetStateData del DualSense (63 byte, come in state_mgr). Scrive il report Bluetooth 0x11 del
// DualShock 4 (78 byte, gli ultimi 4 sono il CRC che aggiunge bt_write).
constexpr uint8_t DS4_LUNGHEZZA_USCITA = 78;
void ds4_uscita(const uint8_t *stato, uint8_t intervallo_ms, uint8_t out[DS4_LUNGHEZZA_USCITA]);

// Calibrazione: il report 0x05 via Bluetooth del DualShock 4 ha i limiti del giroscopio in ordine
// "pitch+, yaw+, roll+, pitch-, yaw-, roll-"; il DualSense in "pitch+, pitch-, yaw+, yaw-, roll+, roll-".
// dati = report con il Report ID in testa (0x05, ...), almeno 19 byte; riordina sul posto.
void ds4_calibrazione_in_dualsense(uint8_t *dati, uint16_t lunghezza);

// Report feature sintetici del DualSense (Report ID in testa, piu' 4 byte di CRC a zero).
constexpr uint8_t DS5_LUNGHEZZA_FEATURE_09 = 1 + 19 + 4;
constexpr uint8_t DS5_LUNGHEZZA_FEATURE_20 = 1 + 63 + 4;
void ds4_feature_09(const uint8_t mac[6], uint8_t out[DS5_LUNGHEZZA_FEATURE_09]);   // mac in ordine leggibile
void ds4_feature_20(uint8_t out[DS5_LUNGHEZZA_FEATURE_20]);

#endif // PSRX_DS4_H
