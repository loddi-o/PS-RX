//
// PS-RX - stato del PC dal solo bus USB (Windows e Linux allo stesso modo) e spegnimento dei controller
// quando il PC si spegne o va in sospensione.
//
//   acceso  = il PC ha configurato il ricevitore e il bus e' attivo
//   sospeso = configurato, ma il PC ha sospeso il bus (sleep, S3)
//   spento  = la porta da' corrente ma nessuno configura il ricevitore (spento, ibernato, avvio rapido)
//
// Nessun lavoro in piu' sul bus: gli stati li tiene gia' TinyUSB (eventi di sospensione, ripresa,
// configurazione). Qui si leggono due valori al massimo 10 volte al secondo e si agisce solo quando lo
// stato cambia e resta tale per 3 s (mai durante un ricollegamento voluto, il cambio di forma USB).
//

#ifndef PSRX_PC_H
#define PSRX_PC_H

#include <cstdint>

enum StatoPc : uint8_t { PC_SCONOSCIUTO = 0, PC_ACCESO = 1, PC_SOSPESO = 2, PC_SPENTO = 3 };

void pc_task(uint32_t ora_ms);
StatoPc pc_stato();
// Il Wake-on-LAN al collegamento del primo controller serve (PC non acceso)? E con quanto ritardo (PC
// sospeso che si puo' svegliare via USB: prima l'USB, il WoL e' di riserva)?
bool pc_serve_wol();
uint32_t pc_ritardo_wol_ms();

#endif // PSRX_PC_H
