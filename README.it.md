# PS-RX

**[English](README.md)** · Italiano

Ricevitore Bluetooth per **DualSense**, **DualSense Edge** e **DualShock 4** costruito con un
**Raspberry Pi Pico 2 W**. Collega fino a **4 controller** al PC (Windows, Linux, SteamOS) come se fossero
via cavo, con bassa latenza, audio e microfono. Non servono driver né programmi: il ricevitore funziona da
solo, e un'app per Windows, un plugin Decky per SteamOS o una pagina web servono quando vuoi cambiarne le
impostazioni.

## Funzioni

- **Tre modalità**:
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
- **Italiano e inglese**: app, installer e plugin seguono la lingua del sistema (l'inglese se non è
  l'italiano); la pagina web è in inglese, con un menu per la lingua.

## Cosa serve

- Un **Raspberry Pi Pico 2 W** (con il WiFi). Il Pico 2 senza "W" e il primo Pico non vanno bene.
- Un **cavo USB dati** (alcuni cavi caricano soltanto).
- Facoltativa, per il Wake-on-LAN: una rete **WiFi a 2,4 GHz**. Il Pico 2 W non vede le reti a 5 GHz.

## Serve il software?

**No.** Tutto il lavoro lo fa il firmware sul Pico; il software serve solo a configurarlo. Il ricevitore
tiene le impostazioni nella sua memoria: puoi configurarlo una volta da un computer qualsiasi e poi usarlo
ovunque, anche dove non è installato niente.

**Senza nessun software** valgono i valori predefiniti: modalità PlayStation, audio e microfono attivi,
risveglio del PC dalla sospensione via USB, controller spenti insieme al PC.
- **Installare il firmware:** copia il file `.uf2` nel Pico (vedi
  [Installare il firmware sul Pico](#installare-il-firmware-sul-pico)).
- **Abbinare un controller:** senza controller collegati, premi una volta il tasto **BOOTSEL** del Pico. Si
  apre l'abbinamento per 30 secondi: tieni premuti **Create + PS** (DualSense) o **Share + PS** (DualShock 4)
  finché la luce lampeggia veloce.

**Il software serve (almeno una volta) per:**
- configurare il **Wake-on-LAN** (rete WiFi e MAC del PC);
- passare alla modalità **Xbox** o **Steam Controller**;
- cambiare qualsiasi altra impostazione, comprese quelle dei singoli controller;
- aggiornare il firmware via USB.

### App, plugin o pagina web?

Tutti e tre cambiano ogni impostazione del ricevitore, e tutti e tre cercano e provano le reti WiFi.
L'**app per Windows** è quella che fa più cose da sola:

| | App Windows | Plugin Decky (SteamOS) | Pagina web |
|---|---|---|---|
| Tutte le impostazioni del ricevitore e dei controller | ✓ | ✓ | ✓ |
| Ricerca e prova delle reti WiFi | ✓ | ✓ | ✓ |
| MAC del PC per il Wake-on-LAN | **Rilevato**: lo scegli fra le schede di rete del PC | Scritto a mano | Scritto a mano |
| Aggiornamenti del firmware | Download e installazione con un clic | Download e installazione con un clic | Li trova; il file lo scarichi tu e lo carichi |
| Preparare un Pico nuovo | **Automatico** appena lo colleghi | Un pulsante | Copi il `.uf2` a mano |
| Notifiche (controller collegato, batteria scarica) | ✓ vicino all'orologio | ✓ in Steam | — |
| Resta attivo in background | ✓ (anche all'avvio di Windows, se vuoi) | ✓ | — |
| Installazione | Installer | Decky Loader | Nessuna: basta aprire l'indirizzo |

## Installazione

Tutti i file sono nell'ultima [release](https://github.com/loddi-o/PS-RX/releases/latest).

### Windows

1. Scarica ed esegui `PS-RX-Setup-X.Y.Z.exe`. L'installer usa la lingua di Windows e mette PS-RX nel menu
   Start.
   L'exe non è firmato: se Windows SmartScreen lo blocca, scegli "Ulteriori informazioni" → "Esegui
   comunque".
2. Apri PS-RX e scegli **"Prepara un nuovo ricevitore…"**.
3. Collega il Pico. L'app scarica l'ultimo firmware, lo copia nel Pico e aspetta il ricevitore pronto. Per
   un Pico già usato, o con un altro firmware, vedi [Installare il firmware sul Pico](#installare-il-firmware-sul-pico).

### SteamOS / Steam Deck (plugin Decky)

1. Installa [Decky Loader](https://decky.xyz).
2. Scarica `ps-rx-decky-X.Y.Z.zip`. In Decky, apri Impostazioni → Sviluppatore → **"Installa plugin da
   ZIP"**.
3. Nel menu rapido (tasto `…`) apri PS-RX. Per un Pico nuovo usa **"Installa PS-RX sul Pico"**, nella sezione
   "Nuovo ricevitore".

### Pagina web (qualsiasi sistema, anche Linux e Steam Deck)

Apri **https://loddi-o.github.io/PS-RX/** in Chrome o Edge, con il ricevitore collegato via USB a quel computer.
Ha le stesse impostazioni dell'app e si aggiorna da sola a ogni release. La pagina gira nel browser e parla
con il ricevitore solo via USB: niente viene mandato in rete. La stessa pagina è anche nella release come
file `ps-rx-X.Y.Z.html`, da usare senza internet.

- **Lingua:** inglese di default; per l'italiano usa il menu in alto a destra. Il browser ricorda la scelta.
- **Browser:** servono Chrome, Edge o un altro browser basato su Chromium (WebUSB). Firefox, Safari e il
  browser integrato di Steam non vanno: la pagina lo dice e indica le alternative.
- **Linux e SteamOS, la prima volta:** serve una regola udev per il permesso sull'USB. La pagina mostra i comandi
  da copiare nel terminale (sezione "Linux e SteamOS: primo utilizzo"); con il plugin Decky installato c'è già.
- **Steam Deck:** in modalità desktop installa Google Chrome da Discover e apri l'indirizzo. In modalità gioco
  è più comodo il plugin Decky.
- **Aggiornamento del firmware:** la pagina trova la versione nuova, ma il file lo scarichi tu e poi lo carichi
  con "Aggiorna il firmware…".

## Installare il firmware sul Pico

Il firmware si installa con il Pico in **modalità BOOTSEL**. In questa modalità il Pico si presenta al PC come
un'unità USB chiamata **"RP2350"**, e basta copiarci dentro il file `.uf2`. La modalità BOOTSEL sta nella
memoria fissa del chip: non si può cancellare, quindi c'è sempre e funziona con qualsiasi firmware.

### Che Pico ho collegato?

Collega il Pico al PC con un cavo dati e guarda cosa compare:

| Cosa vedi | Cosa vuol dire |
|---|---|
| Un'unità **"RP2350"** | Il Pico è in BOOTSEL: è nuovo, ha la memoria vuota oppure hai tenuto premuto BOOTSEL. Vai allo scenario 1. |
| Un'unità **"RPI-RP2"** | È un Pico o Pico W della prima generazione (RP2040): **non è compatibile**, serve un Pico 2 W. |
| Il ricevitore PS-RX nell'app | PS-RX è già installato: vai allo scenario 3. |
| Nient'altro | Il Pico ha un altro firmware (scenario 2), oppure il cavo serve solo per caricare: provane un altro. |

### Scenario 1: Pico nuovo o con la memoria vuota

Un Pico 2 W appena comprato è vuoto e si avvia da solo in BOOTSEL: non serve premere niente.

- **Windows:** apri l'app PS-RX e collega il Pico. L'app trova l'unità "RP2350", apre da sola "Prepara un
  nuovo ricevitore" e installa l'ultimo firmware. Puoi aprire la procedura anche a mano, dal pulsante
  "Prepara un nuovo ricevitore…".
- **SteamOS:** nel plugin Decky, sezione "Nuovo ricevitore", premi **"Installa PS-RX sul Pico"**.
- **A mano (qualsiasi sistema):** copia `ps-rx-firmware-X.Y.Z.uf2` nell'unità "RP2350". Il Pico scrive il
  firmware, l'unità sparisce e dopo qualche secondo il Pico si riavvia come ricevitore PS-RX.

Senza internet, l'app e il plugin usano il firmware che hanno incluso.

### Scenario 2: Pico con un altro firmware

Il Pico ha MicroPython, CircuitPython o il firmware di un altro progetto, quindi non si avvia in BOOTSEL da solo.

1. **Salva prima quello che ti serve.** Ad esempio, gli script di MicroPython: con l'installazione di PS-RX
   il vecchio firmware viene sovrascritto.
2. Stacca il Pico.
3. **Tieni premuto il tasto BOOTSEL** (il pulsante bianco sulla scheda) e, sempre tenendolo premuto, ricollega
   il cavo USB.
4. Rilascia il tasto quando compare l'unità "RP2350".
5. Prosegui come nello **scenario 1**.

I dati lasciati in memoria dal vecchio firmware non danno problemi: PS-RX controlla le proprie impostazioni
con un codice di verifica e, se non le trova, parte con quelle predefinite.

### Scenario 3: Pico con PS-RX già installato

- **Aggiornare:** Sistema → Aggiornamenti (GitHub) → "Cerca aggiornamenti", poi installa il firmware nuovo. Il
  firmware passa via USB, senza BOOTSEL.
- **Reinstallare** (per esempio se un aggiornamento si è interrotto): apri "Prepara un nuovo ricevitore…" con il
  ricevitore collegato. L'app lo riavvia in BOOTSEL e gli reinstalla l'ultimo firmware.

In entrambi i casi controller abbinati, reti WiFi e impostazioni restano. Servono i controller spenti.

Se il ricevitore non risponde più, si recupera sempre con il tasto BOOTSEL, come nello scenario 2.

> **Attenzione:** con l'app Windows aperta, *qualsiasi* Pico 2 collegato in BOOTSEL riceve PS-RX
> automaticamente. Se stai programmando un Pico per un altro progetto, chiudi prima l'app PS-RX
> (anche dall'icona vicino all'orologio).

## Primi passi

1. **Abbina un controller:** premi "Abbina un nuovo controller" (oppure premi una volta il tasto BOOTSEL del
   Pico), poi tieni premuti **Create + PS** (DualSense) o **Share + PS** (DualShock 4) finché la luce
   lampeggia veloce. Dalla volta dopo basta premere PS.
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
  2. scrivi il MAC della scheda di rete **cablata** del PC (per esempio `12:34:56:78:9A:BC`). L'app Windows
     mostra le schede di rete del PC: basta sceglierla;
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
