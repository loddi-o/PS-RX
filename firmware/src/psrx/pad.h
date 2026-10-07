//
// PS-RX - le funzioni con cui il codice PS-RX parla con DS5-Linux-Bridge (Bluetooth e report).
// Tutto gira sul core 0, nel ciclo principale; niente tocca il percorso degli input.
//

#ifndef PSRX_PAD_H
#define PSRX_PAD_H

#include <cstdint>

#include "eventi.h"

uint8_t pad_posti();                 // controller insieme (MULTI_SLOT_COUNT)
int pad_connessi();
bool pad_setup_bt();                 // collegamento o abbinamento in corso

void pad_avvia_abbinamento();        // finestra di abbinamento di DS5-Linux-Bridge
bool pad_finestra_abbinamento();
void pad_spegni(uint8_t posto);      // come tenere premuto PS; l'abbinamento resta
void pad_spegni_tutti();

// Stato di un posto per eventi e app (batteria, modello, MAC).
void pad_info(uint8_t posto, PadPerEventi &out);
int8_t pad_rssi(uint8_t posto);      // 127 = sconosciuto
uint16_t pad_report_al_secondo(uint8_t posto);

// Nel ciclo principale: conta i report ricevuti (guardando il contatore del report gia' copiato,
// non il percorso degli input).
void pad_task(uint32_t ora_ms);

#endif // PSRX_PAD_H
