//
// PS-RX - tempi e soglie del codice PS-RX (un solo posto dove cambiarli).
//

#ifndef PSRX_CONFIG_H
#define PSRX_CONFIG_H

#include <cstdint>

// --- Tasto BOOTSEL del Pico (letto solo senza controller collegati) ---------------
constexpr uint32_t T_BOOTSEL_LETTURA_MS = 100;   // lettura (parcheggia il core 1 per pochi us)
constexpr uint32_t T_BOOTSEL_SERIE_MS = 600;     // fine di una serie di click
constexpr uint32_t T_CLICK_MAX_MS = 1500;        // pressione piu' lunga: non e' un click

// --- Salvataggio differito delle impostazioni ----------------------------------------
constexpr uint32_t T_SALVATAGGIO_ATTESA_MS = 2000;    // dall'ultima modifica
constexpr uint32_t T_SALVATAGGIO_RIPROVA_MS = 10000;  // dopo una scrittura fallita

// --- Eventi per le notifiche dell'app ----------------------------------------------------
constexpr uint32_t T_ATTESA_BATTERIA_MS = 3000;  // al collegamento: aspetta la batteria al massimo 3 s
constexpr uint8_t SOGLIA_BATTERIA_BASSA = 20;
constexpr uint8_t SOGLIA_BATTERIA_CRITICA = 10;
constexpr uint8_t BATTERIA_RIARMO = 40;          // sopra questo livello (o in carica) gli avvisi si riarmano

// --- Servizio USB ----------------------------------------------------------------------
constexpr uint32_t T_APP_APERTA_MS = 5000;       // stato chiesto da poco (non silenzioso): app aperta
constexpr uint32_t T_RSSI_MS = 2000;             // RSSI dei controller, solo con l'app aperta
constexpr uint32_t T_HEAP_MS = 10000;

#endif // PSRX_CONFIG_H
