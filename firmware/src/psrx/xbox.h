//
// PS-RX - modalita' Xbox: un'interfaccia XInput (controller Xbox 360 cablato) per posto.
//
// Il dispositivo USB e' composito con VID 1209 (pid.codes), non Microsoft: con 045E:028E Windows
// aggancerebbe xusb22 all'intero dispositivo (hardware ID nel suo .inf) e non alle singole interfacce.
// Cosi' invece:
//   - Windows: ogni interfaccia porta il compatible ID MS OS 2.0 "XUSB10" -> xusb22 (XInput), nessun
//     driver da installare; l'interfaccia di configurazione resta su WinUSB;
//   - Linux: xpad aggancia ogni interfaccia FF/5D/01 dei VID in elenco, fra cui 1209.
//
// Percorso dell'input: nessuna coda in piu'. Il report del DualSense (o del DualShock 4 gia' tradotto)
// sta in interrupt_in_data; xbox_invia(), chiamata nel ciclo principale subito dopo il servizio USB,
// lo traduce (copie di byte) e lo manda solo quando cambia, come un controller vero. La vibrazione
// arriva dall'endpoint OUT e va al controller con lo stesso percorso di DS5-Linux-Bridge.
//

#ifndef PSRX_XBOX_H
#define PSRX_XBOX_H

#include <cstdint>

constexpr uint16_t XBOX_VID = 0x1209;
constexpr uint16_t XBOX_PID = 0x0001;
constexpr uint8_t XBOX_ITF_LEN = 40;   // interfaccia (9) + descrittore 0x21 (17) + 2 endpoint (14)

// Modalita' Xbox attiva nella configurazione USB servita (letta dal ciclo principale).
extern volatile bool psrx_xbox_attivo;

// Blocco di descrittori di un'interfaccia XInput (posto = numero d'interfaccia).
void xbox_descrittore_interfaccia(uint8_t *dest, uint8_t posto);

// Nel ciclo principale: report verso il PC dei posti cambiati. Pochi microsecondi, non aspetta mai.
void xbox_invia();
// Nel ciclo principale (psrx_task): impostazioni per posto (click del touchpad, polling).
void xbox_task();

#endif // PSRX_XBOX_H
