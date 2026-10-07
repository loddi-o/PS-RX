# PS-RX

**[English](README.en.md)** · Italiano

Ricevitore Bluetooth per **DualSense**, **DualSense Edge** e **DualShock 4** costruito con un
**Raspberry Pi Pico 2 W**. Collega fino a **4 controller** al PC (Windows, Linux, SteamOS) come se fossero
via cavo, con bassa latenza, audio e microfono. Si configura con un'app per Windows, un plugin Decky per
SteamOS oppure una pagina web, senza driver da installare.

## Funzioni

- **Tre modalità**, da scegliere nell'app:
  - **PlayStation**: il PC vede dei DualSense, con giroscopio, touchpad, vibrazione, audio e microfono.
    Anche il DualShock 4 appare come DualSense.
  - **Xbox**: il PC vede dei controller Xbox 360 (XInput), per i giochi che non riconoscono i controller
    PlayStation.
  - **Steam Controller 2026** (sperimentale): ogni controller diventa un nuovo Steam Controller, con due
    trackpad e giroscopio. Funziona solo con Steam aperto.
- **Audio e microfono** del controller (altoparlante e jack delle cuffie), con un controller collegato.
- **Touchpad come mouse**, con la striscia sinistra che fa da rotellina.
- **Colore della barra luminosa secondo il posto**, come su PS5 (1 blu, 2 rosso, 3 verde, 4 rosa).
- **Spegnimento dei controller** quando il PC si spegne o va in sospensione, per inattività o con la
  combinazione Share/Create + Options + L2 + R2.
- **Risveglio del PC** premendo PS: via USB dalla sospensione, oppure con il **Wake-on-LAN** dal PC spento.
- **WiFi solo per il Wake-on-LAN**: si spegne appena si collega un controller, così il Bluetooth ha tutta
  la radio. Ci sono la ricerca delle reti e una prova della rete (password, indirizzo, internet).
- **Aggiornamenti da GitHub** del firmware, dell'app e del plugin, direttamente dall'app.
- **Notifiche**: controller collegato (posto, modello, batteria) e batteria in esaurimento.
- **Italiano e inglese**: si usa la lingua del sistema; se non è l'italiano, l'inglese.

## Cosa serve

- Un **Raspberry Pi Pico 2 W** (con il WiFi). Il Pico 2 senza "W" e il primo Pico non vanno bene.
- Un **cavo USB dati** (alcuni cavi caricano soltanto).
- Facoltativa, per il Wake-on-LAN: una rete **WiFi a 2,4 GHz**. Il Pico 2 W non vede le reti a 5 GHz.

## Installazione

Tutti i file sono nell'ultima [release](https://github.com/loddi-o/PS-RX/releases/latest).

### Windows

1. Scarica ed esegui `PS-RX-Setup-X.Y.Z.exe`. L'installer usa la lingua di Windows e mette PS-RX nel menu
   Start.
   L'exe non è firmato: se Windows SmartScreen lo blocca, scegli "Ulteriori informazioni" → "Esegui
   comunque".
2. Apri PS-RX e scegli **"Prepara un nuovo ricevitore…"**.
3. Collega il Pico:
   - un Pico nuovo si presenta da solo;
   - un Pico già usato va collegato tenendo premuto il tasto **BOOTSEL**.

   L'app scarica l'ultimo firmware, lo copia nel Pico e aspetta il ricevitore pronto.

### SteamOS / Steam Deck (plugin Decky)

1. Installa [Decky Loader](https://decky.xyz).
2. Scarica `ps-rx-decky-X.Y.Z.zip`. In Decky, apri Impostazioni → Sviluppatore → **"Installa plugin da
   ZIP"**.
3. Nel menu rapido (tasto `…`) apri PS-RX. Per un Pico nuovo usa **"Installa PS-RX sul Pico"**, nella sezione
   "Nuovo ricevitore".

### Linux e altri sistemi (pagina web)

Apri `ps-rx-X.Y.Z.html` in Chrome o Edge (WebUSB). Il firmware lo scarichi a mano, poi lo carichi dalla
pagina. Su un Pico nuovo copia il file `ps-rx-firmware-X.Y.Z.uf2` nell'unità "RP2350" che compare
collegandolo con BOOTSEL premuto.

## Primi passi

1. **Abbina un controller:** premi "Abbina un nuovo controller", poi tieni premuti **Create + PS**
   (DualSense) o **Share + PS** (DualShock 4) finché la luce lampeggia veloce. Dalla volta dopo basta
   premere PS.
2. Scegli la **modalità** in Sistema. Cambiarla ricollega l'USB del ricevitore.
3. Ogni controller abbinato ha le sue impostazioni: audio, microfono, frequenza (fino a 1000 Hz), touchpad
   come mouse.

Le modifiche si salvano da sole quando i controller sono spenti: il ricevitore scrive in memoria solo
senza controller collegati, per non disturbare l'input.

## Risvegliare il PC

Premendo PS su un controller abbinato, il ricevitore prova a risvegliare il PC.

- **Dalla sospensione, via USB.** "Sveglia il PC dalla sospensione via USB" è attiva di default: il
  ricevitore aggiunge una piccola tastiera USB che preme un tasto. È il modo più affidabile. Se qualche
  anticheat la nota, puoi spegnerla.
- **Da spento o in alternativa: Wake-on-LAN.** In Rete:
  1. salva la rete WiFi a 2,4 GHz e usa **Prova la rete** (Prova nel plugin e nella pagina);
  2. scrivi il MAC della scheda di rete **cablata** del PC (per esempio `12:34:56:78:9A:BC`);
  3. attiva il Wake-on-LAN nel BIOS e nella scheda di rete.

  La porta USB del ricevitore deve restare alimentata a PC spento (opzione del BIOS tipo "USB power in
  S5").

Mentre il ricevitore prova a svegliare il PC, il LED del Pico lampeggia (a meno che "LED del ricevitore
spento" sia attiva) e il controller funziona normalmente.

## Da sapere

- **Posti:** di default il PC vede solo i controller accesi. Quando se ne collega uno nuovo, gli altri si
  fermano per circa 1 s. Con **"Sempre 4 gamepad sull'USB"** il PC vede sempre 4 gamepad e nessuno si
  interrompe.
- **Audio:**
  - funziona solo con **un** controller collegato, perché con due o più il Bluetooth non ce la fa;
  - il DualShock 4 non ha l'audio.
- **WiFi:** è spento mentre giochi. Si accende solo senza controller, per il Wake-on-LAN, la ricerca e la
  prova delle reti.
- **Un programma alla volta:** l'app, il plugin e la pagina usano lo stesso canale USB. Se uno non trova
  il ricevitore, chiudi gli altri.
- **Modalità Steam Controller:** è sperimentale. Steam potrebbe proporre aggiornamenti del firmware del
  controller, ma non hanno effetto.
- **Modalità Xbox:** niente giroscopio e niente audio; il touchpad funziona solo come mouse.

## Problemi comuni

| Problema | Cosa fare |
|---|---|
| Il ricevitore non si trova | Chiudi l'app, il plugin o la pagina aperti altrove. Prova un altro cavo o un'altra porta. |
| "comando sconosciuto" | Il firmware è più vecchio dell'app: aggiornalo da Sistema → Aggiornamenti (GitHub). |
| La rete non compare nella ricerca | È a 5 GHz: usa il nome a 2,4 GHz del router. |
| Il PC non si sveglia dalla sospensione | Controlla che "Sveglia il PC dalla sospensione via USB" sia attiva e che la porta resti alimentata. In Gestione dispositivi, sulla tastiera HID del ricevitore, attiva "Consenti al dispositivo di riattivare il computer". |
| Il PC non si accende da spento | Usa Rete → Prova la rete, controlla il MAC e il Wake-on-LAN nel BIOS, e verifica l'alimentazione USB in S5. |
| Il controller non si abbina | Spegnilo (tieni PS per 10 s), premi "Abbina" e tieni Create/Share + PS finché lampeggia veloce. |

## Per sviluppatori

Il firmware deriva da [DS5-Linux-Bridge](https://github.com/kungaa/ds5-linux-bridge) v2.3.0-beta.1 (`firmware/BASE_DLB.txt`):

- il codice di PS-RX sta in `firmware/src/psrx/`;
- l'app Windows (PySide6) e la libreria Python stanno in `app/`;
- il plugin Decky sta in `app/decky/`;
- la pagina WebUSB sta in `web/`.

Gli strumenti di build (PowerShell) sono in `strumenti/`. Le note tecniche sono in [`CLAUDE.md`](CLAUDE.md).

## Licenza e ringraziamenti

PS-RX è distribuito con licenza **GPLv3** (vedi `firmware/LICENSE`), come DS5-Linux-Bridge da cui deriva.

Grazie a:
- DS5-Linux-Bridge e DS5Dongle, per la base del firmware;
- SDL3, per la documentazione dei controller;
- openpuck e sc2-research, per il protocollo del nuovo Steam Controller.

PS-RX non è affiliato a Sony, Microsoft o Valve. DualSense, DualShock, Xbox e Steam sono marchi dei
rispettivi proprietari.
