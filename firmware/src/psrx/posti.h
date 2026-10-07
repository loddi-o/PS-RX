//
// PS-RX - funzioni per posto nel ciclo principale: colore della barra luminosa secondo il posto e
// combinazione che spegne il controller. Leggono solo il report gia' ricevuto (interrupt_in_data),
// mai il percorso degli input.
//

#ifndef PSRX_POSTI_H
#define PSRX_POSTI_H

#include <cstdint>

void posti_task(uint32_t ora_ms);

#endif // PSRX_POSTI_H
