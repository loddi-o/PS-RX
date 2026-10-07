//
// PS-RX - modalita' Steam: il ricevitore si presenta come il dongle del nuovo Steam Controller (2026,
// "Proteus", 28DE:1304) con 4 posti, e ogni DualSense/DualShock 4 come uno Steam Controller ("Triton").
//
// Funzioni pure (nessun accesso all'hardware): le prova test/test_logica.cpp sul PC. Il lato USB e'
// in steam_usb.cpp.
//
// Fonti (protocollo pubblico, nessun codice copiato): SDL3 src/joystick/hidapi/SDL_hidapi_steam_triton.c
// e steam/controller_structs.h (report e comandi), github.com/CouchTurtle/sc2-research (topologia USB
// del dongle), github.com/safijari/openpuck (dongle ricostruito che Steam accetta: risposte ai comandi,
// attributi del controller catturati da un esemplare vero).
//
// Report verso il PC su ogni posto (interfaccia HID): 0x45 stato del controller (45 byte), 0x79
// collegato/scollegato, 0x7B stato del collegamento, 0x43 batteria. Dal PC: report di uscita 0x80
// (vibrazione) e comandi "feature" (report 1 = al controller, 2 = al dongle) [cmd][len][dati].
//

#pragma once

#include <cstdint>

constexpr uint8_t STEAM_ID_STATO = 0x45;
constexpr uint8_t STEAM_LEN_STATO = 45;        // senza Report ID (TritonMTUNoQuat_t)
constexpr uint8_t STEAM_ID_COLLEGAMENTO = 0x79;
constexpr uint8_t STEAM_ID_STATO_LINK = 0x7B;
constexpr uint8_t STEAM_ID_BATTERIA = 0x43;
constexpr uint8_t STEAM_ID_VIBRAZIONE = 0x80;
constexpr uint8_t STEAM_LEN_FEATURE = 63;      // senza Report ID

// Tasti (campo buttons, uint32): come SDL3 TritonButtons.
enum : uint32_t {
    TRI_A = 0x00000001, TRI_B = 0x00000002, TRI_X = 0x00000004, TRI_Y = 0x00000008,
    TRI_QAM = 0x00000010, TRI_R3 = 0x00000020, TRI_VIEW = 0x00000040, TRI_R4 = 0x00000080,
    TRI_R5 = 0x00000100, TRI_RB = 0x00000200, TRI_GIU = 0x00000400, TRI_DESTRA = 0x00000800,
    TRI_SINISTRA = 0x00001000, TRI_SU = 0x00002000, TRI_MENU = 0x00004000, TRI_L3 = 0x00008000,
    TRI_STEAM = 0x00010000, TRI_L4 = 0x00020000, TRI_L5 = 0x00040000, TRI_LB = 0x00080000,
    TRI_TOCCO_PAD_DX = 0x00200000, TRI_CLICK_PAD_DX = 0x00400000, TRI_CLICK_GRILLETTO_DX = 0x00800000,
    TRI_TOCCO_PAD_SX = 0x02000000, TRI_CLICK_PAD_SX = 0x04000000, TRI_CLICK_GRILLETTO_SX = 0x08000000,
};

// ds: report USB del DualSense senza Report ID (63 byte, come interrupt_in_data). p: 45 byte del report
// 0x45. Il touchpad del DualSense diventa i due trackpad: meta' sinistra e meta' destra.
void steam_da_dualsense(const uint8_t *ds, uint8_t seq, uint32_t imu_us, uint8_t *p);

// Report di uscita 0x80 (dati dopo il Report ID): vibrazione, motore sinistro (forte) e destro (debole)
// 0..65535.
bool steam_vibrazione(const uint8_t *dati, uint32_t n, uint16_t *sinistro, uint16_t *destro);

// Identita' di un posto (stringhe senza terminatore oltre i 15 caratteri).
struct SteamIdentita {
    char seriale_dongle[16];      // "FXB99602xxxxx"
    char scheda_dongle[16];       // "MXB99602xxxxx"
    char seriale_pad[16];         // "FXA99602xxxxx"
    char scheda_pad[16];          // "MXA99602xxxxx"
    uint32_t uuid_dongle;
    uint32_t uuid_pad;
    bool collegato;
};

// Cose da fare dopo un comando di Steam (le esegue steam_usb.cpp).
enum : uint8_t {
    STEAM_AZ_NESSUNA = 0,
    STEAM_AZ_SPEGNI = 1,          // "spegni il controller" (comando 0x9F "off!")
    STEAM_AZ_STEAM_ATTIVO = 2,    // impostazioni 0x87 con lizard mode spento: Steam guida il controller
};

// Comando feature (report_id 1 o 2, dati = [cmd][len][...]) -> risposta di 63 byte da restituire alla
// prossima lettura della feature. Restituisce le azioni da eseguire. Non riavvia, non aggiorna, non
// resetta mai nulla: i comandi di aggiornamento del firmware ricevono solo l'eco.
uint8_t steam_comando(uint8_t report_id, const uint8_t *dati, uint16_t n, const SteamIdentita &id, uint8_t *risposta);

// Report 0x43 (14 byte): batteria in percentuale e carica.
void steam_batteria(uint8_t livello, bool in_carica, bool carica_completa, uint8_t *p);
// Report 0x7B (12 byte) con la potenza del segnale in dBm (0 = sconosciuta).
void steam_stato_link(int8_t rssi_dbm, uint8_t *p);
// Seriali e UUID derivati da un identificativo (MAC del controller o id del Pico): stabili e unici.
void steam_seriali(const uint8_t *id, uint8_t n, char prefisso_seriale, char *seriale, char *scheda, uint32_t *uuid);
