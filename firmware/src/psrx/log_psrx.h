//
// PS-RX - riga di log con l'ora dall'avvio, su UART0 (GP0) e nel registro in RAM letto dall'app.
// Solo agli eventi (azioni, cambi di stato). La riga va in coda (uart_asincrona.h): non ferma
// il ciclo principale.
//

#ifndef PSRX_LOG_H
#define PSRX_LOG_H

void psrx_log(const char *formato, ...) __attribute__((format(printf, 1, 2)));

#endif // PSRX_LOG_H
