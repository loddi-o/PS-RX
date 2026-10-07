//
// PS-RX - mouse del touchpad sull'USB (vedi trackpad.h per i gesti).
//
// L'interfaccia mouse c'e' solo se almeno un controller abbinato ha "touchpad come mouse": cambiarlo
// ricollega l'USB come la tastiera di risveglio. Nel ciclo principale si guardano i report gia'
// ricevuti dei controller con l'opzione attiva e si manda un report del mouse quando c'e' movimento,
// al massimo uno per millisecondo (intervallo dell'endpoint).
//

#ifndef PSRX_MOUSE_H
#define PSRX_MOUSE_H

void mouse_task();

#endif // PSRX_MOUSE_H
