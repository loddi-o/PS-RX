//
// PS-RX - tasto BOOTSEL del Pico, a firmware in funzione.
//
//   1 click  -> finestra di abbinamento di 30 s
//   3 click  -> modalita' aggiornamento (bootloader, chiavetta RP2350)
//
// Il tasto si legge ogni T_BOOTSEL_LETTURA_MS e solo senza controller collegati: ogni lettura
// sospende per pochi microsecondi l'accesso alla flash (core 1 parcheggiato).
//

#ifndef PSRX_BOOTSEL_GESTI_H
#define PSRX_BOOTSEL_GESTI_H

void bootsel_gesti_task();

#endif // PSRX_BOOTSEL_GESTI_H
