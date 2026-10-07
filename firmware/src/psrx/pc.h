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
// Finestra di risveglio (risveglio.h): al primo controller collegato prova a svegliare il PC via USB e con
// il Wake-on-LAN per la durata scelta nell'app; il LED lampeggia.
void pc_risveglio_task(uint32_t ora_ms);
bool pc_finestra_risveglio();
uint8_t pc_durata_risveglio_s();   // 0 = disattivata

#endif // PSRX_PC_H
