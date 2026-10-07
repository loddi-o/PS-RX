//
// PS-RX - DualShock 4 nei posti del ricevitore (riconoscimento e traduzione; vedi ds4.h).
//
// Riconoscimento: DS5-Linux-Bridge chiede al controller alcuni report feature appena si apre il
// canale HID; PS-RX aggiunge in coda il report 0xA3, che solo il DualShock 4 possiede. Se arriva, il
// posto passa in "modalita' DualShock 4" (bt_slot_segna_ds4): uscita tradotta in bt_write, report
// feature sintetici per l'host, calibrazione riordinata. Il modello si ricorda per MAC: alle
// riconnessioni successive vale subito.
//

#ifndef PSRX_DS4_POSTI_H
#define PSRX_DS4_POSTI_H

#include <cstdint>

// Nel percorso degli input (on_bt_data, RAM): report 0x11 di un DualShock 4.
void psrx_ds4_ingresso(uint8_t posto, const uint8_t *dati, uint16_t lunghezza);

// Nel ciclo principale.
void ds4_posti_task(uint32_t ora_ms);

#endif // PSRX_DS4_POSTI_H
