//
// PS-RX - punto d'ingresso del codice PS-RX dentro DS5-Linux-Bridge (main.cpp).
//
//   psrx_init()   dopo Bluetooth e audio, prima della prima enumerazione USB
//   psrx_task()   a ogni giro del ciclo principale, dopo i percorsi critici; non aspetta mai
//

#ifndef PSRX_H
#define PSRX_H

void psrx_init();
void psrx_task();

#endif // PSRX_H
