//
// PS-RX - punto d'ingresso del codice PS-RX dentro DS5-Linux-Bridge.
//

#include "psrx.h"

#include "log_psrx.h"
#include "uart_asincrona.h"

#ifndef PICO_PROGRAM_VERSION_STRING
#define PICO_PROGRAM_VERSION_STRING "sconosciuta"
#endif

void psrx_init() {
    uart_asincrona_init(); // da qui in poi i log non fermano piu' il ciclo principale
    psrx_log("PS-RX %s (base DS5-Linux-Bridge v2.3.0-beta.1)", PICO_PROGRAM_VERSION_STRING);
}

void psrx_task() {
    uart_asincrona_svuota();
}
