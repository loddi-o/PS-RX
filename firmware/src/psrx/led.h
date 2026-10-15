//
// PS-RX - LED del Pico.
//
//   - "LED del ricevitore spento" attivo: sempre spento, in ogni caso;
//   - finestra di abbinamento aperta (30 s, doppio click sul BOOTSEL o dall'app): lampeggia rapido
//     (circa 4-5 volte al secondo, come i pad PlayStation in modalita' abbinamento). Ha la precedenza
//     sul lampeggio di risveglio;
//   - risveglio del PC in corso: lampeggia (2 volte al secondo). Comincia quando si collega un controller e
//     il PC non e' acceso (si sta cercando di svegliarlo: prima via USB, poi Wake-on-LAN); finisce appena il
//     PC e' acceso, se si scollegano tutti i controller, o dopo 2 minuti;
//   - un click sul BOOTSEL (nessuna azione): acceso fisso per qualche secondo, per far vedere che il
//     Pico e' acceso e pronto;
//   - altrimenti acceso con almeno un controller collegato, spento senza.
//
// Il LED e' un pin del chip radio: ogni cambio e' una breve scrittura al chip. Si scrive solo quando lo stato
// cambia (al massimo 4 volte al secondo, e solo durante il risveglio, mai mentre si gioca).
//

#ifndef PSRX_LED_H
#define PSRX_LED_H

#include <cstdint>

void led_task(uint32_t ora_ms);
// Riscrive il LED al prossimo giro (impostazione cambiata).
void led_forza();
// Il risveglio del PC e' in corso (per lo stato dell'app e il registro).
bool led_risveglio_in_corso();
// Accende il LED per qualche secondo: un click sul BOOTSEL, nessuna azione, solo per far vedere
// all'utente che il Pico e' acceso e pronto (nessun controller collegato in quel momento).
void led_segnala_pronto();

#endif // PSRX_LED_H
