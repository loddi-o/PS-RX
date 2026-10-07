//
// PS-RX - aggiornamento del firmware dall'app, senza premere BOOTSEL.
//
// L'app carica il firmware nuovo nell'area di appoggio (da 2 MB in su) a blocchi da 4 KB, solo con
// nessun controller collegato. Il Pico verifica lo SHA-256 (hardware) e che sia un'immagine RP2350;
// poi l'app chiede l'installazione: il Pico copia l'appoggio sopra il firmware in esecuzione e si
// riavvia (circa 15 s; configurazione e abbinamenti restano). In alternativa "BOOTSEL ora" porta il
// Pico nel bootloader (chiavetta RP2350).
//
// Le funzioni chiamate dal servizio USB sono veloci: niente flash, niente attese. Il lavoro pesante
// (scrittura, verifica, installazione) lo fa aggiornamento_task() nel ciclo principale.
//

#ifndef PSRX_AGGIORNAMENTO_H
#define PSRX_AGGIORNAMENTO_H

#include <cstdint>

#include "protocollo.h"

void aggiornamento_task();

// --- servizio USB: restituiscono ERR_NESSUNO o il motivo del rifiuto ---------------
uint8_t aggiornamento_inizia(uint32_t dimensione, const uint8_t sha256[32]);
uint8_t aggiornamento_prepara_blocco(uint16_t indice, uint16_t lunghezza, uint8_t **destinazione);
uint8_t aggiornamento_blocco_ricevuto(uint16_t indice, uint16_t lunghezza);
uint8_t aggiornamento_fine();
void aggiornamento_annulla();
uint8_t aggiornamento_installa_ora();
uint8_t aggiornamento_bootsel_ora();

// --- stato ------------------------------------------------------------------------------
uint8_t aggiornamento_stato();          // StatoCaricamento
void aggiornamento_leggi(CaricamentoPsrx *out);
uint32_t aggiornamento_capacita();      // dimensione massima di un firmware

// Bootloader subito (3 click sul BOOTSEL, gia' controllato: nessun controller collegato).
void aggiornamento_entra_in_bootsel(const char *motivo);

#endif // PSRX_AGGIORNAMENTO_H
