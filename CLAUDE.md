# PS-RX: note per Claude

Ricevitore Bluetooth per DualSense, DualSense Edge e DualShock 4 (fino a 4) su Raspberry Pi Pico 2 W.
Repo GitHub privata `loddi-o/PS-RX`. L'utente scrive in italiano: rispondere in italiano.

**Priorità assoluta: la stabilità dell'input.** Niente singhiozzi. Ogni modifica che tocca il percorso dei
pacchetti va detta all'utente con il costo misurato. Le alternative più leggere si discutono prima.

## Base e regole

- **Base:** DS5-Linux-Bridge v2.3.0-beta.1, commit `1e15ea2` (GPLv3), vedi `firmware/BASE_DLB.txt`.
  - Il codice PS-RX sta in `firmware/src/psrx/`.
  - I file originali hanno modifiche piccole, marcate `PS-RX`.
- **Ciclo unico su core0** (`pico_cyw43_arch_poll`). Le funzioni del percorso per pacchetto non devono
  cambiare.
  - `strumenti/confronta_codice.py` confronta il codice macchina con la build di riferimento: elenco
    `CRITICHE`.
  - Le eccezioni volute stanno in `MODIFICATE`, ognuna con il motivo.
  - Se una funzione critica cambia senza essere in elenco, la build fallisce.
- **Flash:** si scrive solo senza controller collegati (`salvataggio.cpp`, salvataggio differito).
- **Configurazione:** solo in coda (append-only) da offset 238 (`config.h`, con static_assert).
- **Log:** UART non bloccante (`log_psrx`, `uart_asincrona`).

## Strumenti (PowerShell, da questa cartella)

| Comando | Cosa fa |
|---|---|
| `strumenti/prepara_ambiente.ps1` | Toolchain, pico-sdk-dlb e opus in `%USERPROFILE%\.ds5-build` |
| `strumenti/compila_firmware.ps1 [-Versione x] [-SenzaConfronto]` | Build normale + riferimento in `.ds5-build\build-psrx`, poi confronto del codice macchina e dell'heap |
| `strumenti/prova_logica.ps1` | Test g++ della logica pura (moduli in `firmware/test/moduli.txt`) |
| `strumenti/flash.ps1` | Copia l'uf2 sul Pico in BOOTSEL |
| `cd app; python -m unittest discover -s test` | Test della libreria Python con il Pico simulato |

## Stato delle fasi (piano approvato)

1. **Fondamenta:** fatta. Tolti web, portale, OTA e mDNS; WiFi solo per il WoL; log non bloccante.
2. **Servizio USB e configurazione:** fatta.
   - Interfaccia vendor senza endpoint (FF/50) in coda a ogni variante, con MS OS 2.0 `WINUSB` +
     GUID `{6F1D2C3B-8A4E-4C59-9B7A-52A1D3E0C7F1}`.
   - Richieste di controllo `bRequest 0x50` (`protocollo.h`), eventi, aggiornamento dall'app con
     installazione immediata a controller spenti.
3. **Rete:** fatta. Politica pura in `politica_rete.cpp`, lavoro sul chip in `rete.cpp`; 5 reti; 3 pacchetti
   WoL entro 30 s, poi WiFi spento.
4. **DualShock 4:** fatta. Riconosciuto con il feature report 0xA3 e tradotto in DualSense (`ds4.cpp`,
   `ds4_posti.cpp`); l'uscita è tradotta in `bt_write`.
5. **Funzioni per controller:** fatta.
   - Colore del posto, combinazione di spegnimento (Share/Create + Options + L2 + R2), polling per posto.
   - Variante USB FISSO (audio + 4 gamepad sempre presenti).
   - Touchpad come mouse: interfaccia HID mouse EP 0x86; la striscia sinistra fa da rotellina.
6. **Modalità Xbox:** firmware scritto (`xbox.cpp`, `xinput.cpp`), **non ancora provato sull'hardware**.
   - Dispositivo composito con VID/PID **1209:0001**, non 045E:028E: `xusb22.inf` ha
     `USB\Vid_045E&Pid_028E` come hardware ID e prenderebbe l'intero dispositivo.
   - Su Windows ogni interfaccia XInput (FF/5D/01) ha il compatible ID MS OS 2.0 `XUSB10` e va a xusb22.
   - Su Linux xpad aggancia FF/5D/01 per i VID in elenco, fra cui 1209.
   - Senza controller resta il segnaposto HID di DS5-Linux-Bridge, perché il dispositivo deve restare
     composito.
   - Niente tastiera e niente audio in modalità Xbox.
   - Invio solo quando lo stato cambia, da `xbox_invia()` nel ciclo principale; `interrupt_loop` resta
     identica.
7. **App Windows (PySide6), plugin Decky, pagina WebUSB:** da fare. La libreria `app/psrx` è pronta
   (protocollo, WinUSB/usbfs, notifiche, CLI).
8. **Modalità Steam Controller (sperimentale):** da fare.
9. **Documenti e release GitHub:** da fare.

**Prove sull'hardware:** nessuna finora. Primo flash con BOOTSEL e `strumenti/flash.ps1`. Poi:
- `python -m psrx stato` (da `app/`);
- DualSense e DualShock 4;
- modalità Xbox con `XInputGetState` e con xpad su SteamOS.

## Release

Stesse regole di PS250:
- l'ultima release è "Latest", le precedenti diventano prerelease;
- niente dati personali: MAC d'esempio `12:34:56:78:9A:BC`, nome "Giocatore".
