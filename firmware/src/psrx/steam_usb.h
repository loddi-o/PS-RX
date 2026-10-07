//
// PS-RX - modalita' Steam, lato USB: il dongle del nuovo Steam Controller (28DE:1304) con 4 posti.
//
// Forma USB (come il dongle vero, che SDL e Steam si aspettano): interfacce 0-1 = configurazione PS-RX
// (WinUSB/WebUSB, unite da un IAD dove il dongle vero ha la seriale CDC), interfacce 2-5 = i 4 posti
// HID, sempre presenti: collegare o spegnere un controller non ricollega mai l'USB, lo annuncia il
// report 0x79 del suo posto.
//
// Percorso dell'input: come la modalita' Xbox. Il report gia' ricevuto (interrupt_in_data) viene
// tradotto (steam.cpp, copie di byte) da steam_invia() nel ciclo principale e parte quando cambia.
// I posti li serve un driver di classe nostro: le funzioni HID di DS5-Linux-Bridge restano intatte.
//

#ifndef PSRX_STEAM_USB_H
#define PSRX_STEAM_USB_H

#include <cstdint>

constexpr uint16_t STEAM_VID = 0x28DE;
constexpr uint16_t STEAM_PID = 0x1304;
constexpr uint16_t STEAM_BCD = 0x0002;
constexpr uint8_t STEAM_POSTI = 4;
constexpr uint8_t STEAM_PRIMA_ITF = 2;

// Modalita' Steam attiva nella configurazione servita (letta dal ciclo principale).
extern volatile bool psrx_steam_attivo;

// Descrittore di configurazione completo della modalita' Steam (wTotalLength nei byte 2-3).
const uint8_t *steam_descrittore_configurazione();
// Seriale USB del dongle ("FXB99602xxxxx").
const char *steam_seriale_usb();

// Nel ciclo principale: report dei posti cambiati e report periodici. Pochi microsecondi, non aspetta mai.
void steam_invia();
// Nel ciclo principale (psrx_task): collegamenti, batteria, vibrazione, comandi di Steam.
void steam_task(uint32_t ora_ms);

#endif // PSRX_STEAM_USB_H
