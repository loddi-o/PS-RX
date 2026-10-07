//
// PS-RX - log su UART0 (GP0) che non blocca il ciclo principale.
//
// Lo stdio_uart dell'SDK aspetta che la FIFO della UART (32 byte, circa 87 us a carattere a
// 115200) abbia posto: una riga di log fermava il ciclo principale, quindi Bluetooth, report
// HID e audio, per 1-7 ms. Questo driver mette i caratteri in una coda in RAM (coda_uart.h) e
// il ciclo principale li passa alla FIFO solo quando c'e' posto, senza mai aspettare.
//

#ifndef PSRX_UART_ASINCRONA_H
#define PSRX_UART_ASINCRONA_H

// Sostituisce lo stdio_uart (gia' avviato da board_init) con il driver a coda.
void uart_asincrona_init();

// Nel ciclo principale: riempie la FIFO della UART finche' ha posto. Non aspetta mai.
void uart_asincrona_svuota();

// Bloccante: svuota tutta la coda e aspetta la fine della trasmissione. Solo prima di un
// riavvio voluto (macchina spenta), perche' le ultime righe arrivino sulla UART.
void uart_asincrona_svuota_tutto();

#endif // PSRX_UART_ASINCRONA_H
