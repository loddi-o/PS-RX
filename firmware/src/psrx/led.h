//
// PS-RX - LED del Pico.
//
//   - "LED del ricevitore spento" attivo: sempre spento, in ogni caso;
//   - abbinamento in corso (doppio click sul BOOTSEL o "Abbina" dall'app): due lampi brevi e una pausa, come un
//     DualSense in attesa di abbinarsi; finisce quando il nuovo controller si collega o dopo i 30 s della finestra;
//   - risveglio del PC in corso: lampeggia (2 volte al secondo). Comincia quando si collega un controller e
//     il PC non e' acceso (si sta cercando di svegliarlo: prima via USB, poi Wake-on-LAN); finisce appena il
//     PC e' acceso, se si scollegano tutti i controller, o dopo 2 minuti;
//   - altrimenti acceso con almeno un controller collegato, spento senza.
//
// Il LED e' un pin del chip radio: ogni cambio e' una breve scrittura al chip. Si scrive solo quando lo stato
// cambia (al massimo 4 volte al secondo, e solo durante l'abbinamento o il risveglio, mai mentre si gioca).
//

#ifndef PSRX_LED_H
#define PSRX_LED_H

#include <cstdint>

#include "psrx_config.h"

void led_task(uint32_t ora_ms);
// Riscrive il LED al prossimo giro (impostazione cambiata).
void led_forza();
// Il risveglio del PC e' in corso (per lo stato dell'app e il registro).
bool led_risveglio_in_corso();

// Schema dell'abbinamento (logica pura): acceso o spento a t_ms dall'apertura della finestra.
inline bool led_lampeggio_abbinamento(uint32_t t_ms) {
    const uint32_t fase = (t_ms % T_CICLO_ABBINAMENTO_MS) / T_LAMPO_ABBINAMENTO_MS;
    return fase == 0 || fase == 2;
}

#endif // PSRX_LED_H
