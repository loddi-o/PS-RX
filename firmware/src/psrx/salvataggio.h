//
// PS-RX - salvataggio differito delle impostazioni cambiate dall'app.
//
// Le modifiche valgono subito (in RAM). In flash si scrivono solo quando nessun controller e'
// collegato (vedi politiche.h): una scrittura ferma il Pico per circa 50 ms.
//

#ifndef PSRX_SALVATAGGIO_H
#define PSRX_SALVATAGGIO_H

void salvataggio_segna_modifica();   // dopo ogni modifica della configurazione in RAM
void salvataggio_forza();            // "Salva ora" dall'app (gia' controllato: nessun controller)
bool salvataggio_in_sospeso();
void salvataggio_task();             // nel ciclo principale

#endif // PSRX_SALVATAGGIO_H
