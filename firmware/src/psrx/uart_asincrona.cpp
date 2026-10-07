//
// PS-RX - log su UART0 che non blocca il ciclo principale (vedi uart_asincrona.h).
//

#include "uart_asincrona.h"

#include "coda_uart.h"

#include "hardware/uart.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/stdio_uart.h"

static CodaUart<4096> coda; // circa 350 ms di UART: basta per la raffica di una connessione
static stdio_driver_t driver = {};

static uart_inst_t *uart() {
    return uart_get_instance(PICO_DEFAULT_UART);
}

static void out_chars(const char *buf, int len) {
    coda.scrivi(buf, len);
}

void uart_asincrona_svuota() {
    uart_inst_t *u = uart();
    char c;
    while (uart_is_writable(u) && coda.leggi(c)) {
        uart_putc_raw(u, c);
    }
}

void uart_asincrona_svuota_tutto() {
    uart_inst_t *u = uart();
    char c;
    while (coda.leggi(c)) {
        uart_putc_raw(u, c); // aspetta la FIFO: solo prima di un riavvio
    }
    uart_tx_wait_blocking(u);
}

void uart_asincrona_init() {
    driver.out_chars = out_chars;
    driver.out_flush = uart_asincrona_svuota_tutto;
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    driver.crlf_enabled = PICO_STDIO_DEFAULT_CRLF;
#endif
    // Prima il nuovo, poi via il vecchio: nessuna riga va persa nel passaggio.
    stdio_set_driver_enabled(&driver, true);
    stdio_set_driver_enabled(&stdio_uart, false);
}
