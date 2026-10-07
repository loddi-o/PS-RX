//
// PS-RX - punto d'ingresso del codice PS-RX dentro DS5-Linux-Bridge.
//

#include "psrx.h"

#include "aggiornamento.h"
#include "audio_pad.h"
#include "ds4_posti.h"
#include "bootsel_gesti.h"
#include "log_psrx.h"
#include "mouse.h"
#include "pad.h"
#include "led.h"
#include "pc.h"
#include "posti.h"
#include "salvataggio.h"
#include "servizio_usb.h"
#include "uart_asincrona.h"
#include "steam_usb.h"
#include "xbox.h"

#include "pico/time.h"

#ifndef PICO_PROGRAM_VERSION_STRING
#define PICO_PROGRAM_VERSION_STRING "sconosciuta"
#endif

void psrx_init() {
    uart_asincrona_init(); // da qui in poi i log non fermano piu' il ciclo principale
    psrx_log("PS-RX %s (base DS5-Linux-Bridge v2.3.0-beta.1)", PICO_PROGRAM_VERSION_STRING);
}

void psrx_task() {
    const uint32_t ora = to_ms_since_boot(get_absolute_time());
    ds4_posti_task(ora);
    audio_pad_task();
    posti_task(ora);
    mouse_task();
    xbox_task();
    steam_task(ora);
    pad_task(ora);
    pc_task(ora);
    pc_risveglio_task(ora);
    led_task(ora);
    servizio_usb_task();
    salvataggio_task();
    aggiornamento_task();
    bootsel_gesti_task();
    uart_asincrona_svuota();
}
