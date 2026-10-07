//
// PS-RX - riga di log con l'ora dall'avvio.
//

#include "log_psrx.h"

#include <cstdarg>
#include <cstdio>

#include "pico/time.h"

void psrx_log(const char *formato, ...) {
    char riga[160];
    va_list ap;
    va_start(ap, formato);
    vsnprintf(riga, sizeof riga, formato, ap);
    va_end(ap);
    const uint32_t t = to_ms_since_boot(get_absolute_time());
    printf("[PS-RX %6lu.%03lu] %s\n", static_cast<unsigned long>(t / 1000), static_cast<unsigned long>(t % 1000), riga);
}
