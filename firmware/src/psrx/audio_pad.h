//
// PS-RX - audio e microfono secondo il controller che li usa.
//
// DS5-Linux-Bridge manda l'audio (altoparlante, cuffie, vibrazione HD) e riceve il microfono solo con
// un controller collegato. PS-RX aggiunge: niente audio se quel controller e' un DualShock 4 o ha
// l'audio spento nelle impostazioni (tier_audio_allowed() torna falso: niente pacchetti audio via
// Bluetooth, il rumble passa dallo stato); niente microfono se e' spento nelle impostazioni (il
// controller smette di trasmetterlo).
//

#ifndef PSRX_AUDIO_PAD_H
#define PSRX_AUDIO_PAD_H

void audio_pad_task();

#endif // PSRX_AUDIO_PAD_H
