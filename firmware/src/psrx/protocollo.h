//
// PS-RX - protocollo USB fra il Pico e le app (app Windows, plugin Decky, pagina WebUSB).
// Contratto unico: docs/PROTOCOLLO_USB.md lo descrive, app/psrx/protocollo.py lo rispecchia (un
// test controlla che i numeri coincidano).
//
// Trasporto: richieste di controllo VENDOR sull'endpoint 0, destinatario DISPOSITIVO.
//   lettura:   bmRequestType 0xC0, bRequest PSRX_RICHIESTA, wValue = comando, wIndex = argomento
//   scrittura: bmRequestType 0x40, bRequest PSRX_RICHIESTA, wValue = comando, wIndex = argomento,
//              dati (fino a PSRX_DATI_MAX byte)
// Errore: STALL; il motivo si legge con CMD_ERRORE. Interi little-endian, strutture impacchettate.
// Su Windows le richieste passano dall'interfaccia vendor (WinUSB, agganciato da solo con i
// descrittori MS OS 2.0); su Linux da usbfs.
//

#ifndef PSRX_PROTOCOLLO_H
#define PSRX_PROTOCOLLO_H

#include <cstdint>

constexpr uint8_t PSRX_RICHIESTA = 0x50;     // bRequest ('P'); 0x01 resta a MS OS 2.0
constexpr uint8_t PSRX_PROTOCOLLO = 1;
constexpr uint16_t PSRX_DATI_MAX = 4096;     // dati di una richiesta (un settore di flash)
constexpr uint32_t PSRX_SETTORE = 4096;
constexpr uint8_t PSRX_PAD_MAX = 4;          // posti sempre presenti nello stato
constexpr uint8_t PSRX_RETI_MAX = 5;
constexpr uint8_t PSRX_SLOT_TUTTI = 0xFF;    // SPEGNI_PAD: tutti i controller
constexpr uint16_t STATO_SILENZIOSO = 0x0001; // STATO, wIndex: lettura di sorveglianza (app in background)

// --- Comandi (wValue) ---------------------------------------------------------
enum ComandoPsrx : uint8_t {
    // lettura (0xC0)
    CMD_INFO          = 0x00,
    CMD_STATO         = 0x01,  // wIndex: 0, oppure STATO_SILENZIOSO
    CMD_IMPOSTAZIONI  = 0x02,
    CMD_ABBINATI      = 0x03,
    CMD_REGISTRO      = 0x04,
    CMD_ERRORE        = 0x05,
    CMD_CARICAMENTO   = 0x06,
    CMD_EVENTI        = 0x07,  // wIndex = ultimo numero d'evento gia' letto
    CMD_RETI          = 0x08,
    // scrittura (0x40)
    CMD_IMPOSTA       = 0x10,  // wIndex = id impostazione globale, dati = valore u16
    CMD_SALVA_ORA     = 0x11,
    CMD_PREDEFINITE   = 0x12,
    CMD_IMPOSTA_PAD   = 0x13,  // dati = ImpostaPad
    CMD_RINOMINA      = 0x20,  // dati = MAC[6] + nome[16]
    CMD_DIMENTICA     = 0x21,  // dati = MAC[6]
    CMD_DIMENTICA_TUTTI = 0x22,
    CMD_ABBINA        = 0x23,
    CMD_SPEGNI_PAD    = 0x24,  // wIndex = posto, oppure PSRX_SLOT_TUTTI
    CMD_RETE_SALVA    = 0x30,  // wIndex = posto (0..4), dati = ReteDati
    CMD_RETE_CANCELLA = 0x31,  // wIndex = posto
    CMD_WOL_DESTINAZIONI = 0x32, // dati = 2 x MAC[6] (tutto zero = nessuna)
    CMD_WOL_PROVA     = 0x33,
    CMD_CARICA_INIZIO = 0x40,  // dati = CaricaInizio
    CMD_CARICA_BLOCCO = 0x41,  // wIndex = numero del blocco, dati = 4096 byte
    CMD_CARICA_FINE   = 0x42,
    CMD_CARICA_ANNULLA = 0x43,
    CMD_INSTALLA_ORA  = 0x44,  // solo senza controller collegati: il Pico si riavvia
    CMD_BOOTSEL_ORA   = 0x45,  // solo senza controller collegati
};

// --- Codici d'errore (CMD_ERRORE) ---------------------------------------------
enum ErrorePsrx : uint8_t {
    ERR_NESSUNO       = 0,
    ERR_COMANDO       = 1,   // comando sconosciuto o direzione sbagliata
    ERR_LUNGHEZZA     = 2,
    ERR_VALORE        = 3,
    ERR_OCCUPATO      = 4,   // azione precedente non ancora finita
    ERR_PAD_CONNESSO  = 5,   // serve che nessun controller sia collegato
    ERR_NON_TROVATO   = 6,
    ERR_PIENO         = 7,
    ERR_FLASH         = 8,
    ERR_SEQUENZA      = 9,   // blocco fuori ordine o caricamento non iniziato
    ERR_DIMENSIONE    = 10,
    ERR_VERIFICA      = 11,  // SHA-256 diverso
    ERR_IMMAGINE      = 12,  // non e' un firmware RP2350
    ERR_NON_PRONTO    = 13,
    ERR_NON_CONNESSO  = 14,  // WiFi non connesso (prova del WoL)
};

// --- Impostazioni globali (CMD_IMPOSTAZIONI / CMD_IMPOSTA) ----------------------
enum ImpostazionePsrx : uint8_t {
    IMP_MODALITA           = 1,  // 0 PlayStation, 1 Xbox, 2 Steam (ricollega l'USB)
    IMP_POSTI_FISSI        = 2,  // 0/1: sempre 4 gamepad sull'USB (ricollega l'USB)
    IMP_LED_POSTO          = 3,  // 0/1: lightbar col colore del posto
    IMP_WOL_SPENTO         = 4,  // 0/1: niente Wake-on-LAN al collegamento
    IMP_BUFFER_AUDIO       = 5,  // 16..128
    IMP_INATTIVITA_MIN     = 6,  // 5..60 minuti
    IMP_MAI_DISCONNETTERE  = 7,  // 0/1
    IMP_LED_PICO_SPENTO    = 8,  // 0/1
    IMP_TASTIERA_RISVEGLIO = 9,  // 0/1: tastiera USB per svegliare il PC dalla sospensione
    IMP_REGISTRO           = 10, // 0/1: registro diagnostico in RAM
};
constexpr uint8_t PSRX_N_IMPOSTAZIONI = 10;

// --- Impostazioni per controller (CMD_IMPOSTA_PAD) -------------------------------
enum ImpostazionePad : uint8_t {
    PAD_AUDIO              = 1,  // 0/1 (1 = altoparlante e cuffie attivi)
    PAD_MICROFONO          = 2,  // 0/1
    PAD_POLLING            = 3,  // 0 = 1000 Hz, 1 = 500, 2 = 250, 3 = 125
    PAD_TRACKPAD           = 4,  // 0/1: il touchpad muove il mouse
    PAD_INVERTI_SCORRIMENTO = 5, // 0/1
};

// --- Modelli di controller ---------------------------------------------------
enum ModelloPad : uint8_t {
    MODELLO_NESSUNO   = 0,
    MODELLO_DUALSENSE = 1,
    MODELLO_EDGE      = 2,
    MODELLO_DS4       = 3,
};

// --- Stato del caricamento di un firmware ------------------------------------
enum StatoCaricamento : uint8_t {
    CAR_INATTIVO      = 0,
    CAR_RICEZIONE     = 1,   // aspetta il prossimo blocco
    CAR_SCRITTURA     = 2,   // il blocco ricevuto si sta scrivendo in flash
    CAR_VERIFICA      = 3,   // tutti i blocchi scritti, SHA-256 in corso
    CAR_PRONTO        = 4,   // verificato, si installa con CMD_INSTALLA_ORA
    CAR_INSTALLAZIONE = 5,
    CAR_ERRORE        = 6,
};

// --- Eventi (CMD_EVENTI) ------------------------------------------------------------
enum TipoEvento : uint8_t {
    EVENTO_COLLEGATO   = 1,  // un controller si e' collegato (con batteria, modello, posto)
    EVENTO_SCOLLEGATO  = 2,
    EVENTO_BATTERIA    = 3,  // batteria scesa al 20% (flag 0) o al 10% (flag 1)
};

// Capacita' (InfoPsrx.capacita)
constexpr uint32_t CAP_AGGIORNAMENTO_APP = 1u << 0;
constexpr uint32_t CAP_WOL               = 1u << 1;
constexpr uint32_t CAP_REGISTRO          = 1u << 2;
constexpr uint32_t CAP_DS4               = 1u << 3;
constexpr uint32_t CAP_XBOX              = 1u << 4;
constexpr uint32_t CAP_STEAM             = 1u << 5;
constexpr uint32_t CAP_TRACKPAD          = 1u << 6;

// --- Strutture ----------------------------------------------------------------
#pragma pack(push, 1)

struct InfoPsrx {                    // CMD_INFO, 76 byte
    char magic[4];                   // "PSRX"
    uint8_t protocollo;              // PSRX_PROTOCOLLO
    uint8_t slot_max;                // controller insieme
    uint16_t riservato;
    uint32_t capacita;               // CAP_*
    uint32_t staging_max;            // byte massimi di un firmware caricabile
    char versione[32];               // es. "ps-rx-0.1.0"
    char base[28];                   // base DS5-Linux-Bridge
};

struct PadPsrx {                     // 20 byte
    uint8_t connesso;
    uint8_t modello;                 // ModelloPad
    uint8_t batteria;                // 0..100
    uint8_t flag;                    // bit0 batteria valida, bit1 in carica, bit2 audio di questo pad attivo
    int8_t rssi;                     // dB rispetto alla fascia ideale, 127 = sconosciuto
    uint8_t mac[6];
    uint8_t polling;                 // impostazione PAD_POLLING
    uint16_t report_al_secondo;      // report ricevuti dal Bluetooth nell'ultimo secondo
    uint8_t riservato[6];
};

struct StatoPsrx {                   // CMD_STATO
    uint8_t versione;                // della struttura (1)
    uint8_t modalita;                // IMP_MODALITA
    uint8_t usb_gamepad;             // gamepad esposti all'host
    uint8_t usb_flag;                // bit0 configurato, bit1 sospeso, bit2 tastiera di risveglio
    uint8_t audio_flag;              // bit0 altoparlante aperto dall'host, bit1 microfono, bit2 muto
    uint8_t pad_connessi;
    uint8_t flag;                    // bit0 finestra di abbinamento, bit1 salvataggio in sospeso, bit2 posti fissi
    uint8_t caricamento;             // StatoCaricamento
    uint32_t uptime_s;
    uint32_t heap_libero;            // byte (0 = non misurato)
    uint16_t ultimo_evento;          // numero dell'ultimo evento (per CMD_EVENTI)
    // rete
    uint8_t rete_stato;              // StatoRete (rete.h)
    int8_t rete_indice;
    uint8_t rete_ip[4];
    int16_t rete_rssi;
    uint8_t wol_finestra;
    uint8_t riservato;
    uint32_t ms_da_ultimo_wol;       // 0xFFFFFFFF = mai
    PadPsrx pad[PSRX_PAD_MAX];
};

struct VoceImpostazione {            // CMD_IMPOSTAZIONI: n (u8) + n voci
    uint8_t id;
    uint16_t valore;
};

struct VoceAbbinato {                // CMD_ABBINATI: n (u8) + n voci
    uint8_t mac[6];
    char nome[16];
    uint8_t posto;                   // 0xFF = non collegato
    uint8_t audio;                   // impostazioni del controller (vedi ImpostazionePad)
    uint8_t microfono;
    uint8_t polling;
    uint8_t trackpad;
    uint8_t inverti_scorrimento;
    uint8_t riservato;
};

struct ImpostaPad {                  // CMD_IMPOSTA_PAD
    uint8_t mac[6];
    uint8_t id;                      // ImpostazionePad
    uint8_t valore;
};

struct EventoPsrx {                  // CMD_EVENTI: n (u8) + n eventi, i piu' vecchi prima
    uint16_t numero;                 // cresce di 1 a ogni evento
    uint8_t tipo;                    // TipoEvento
    uint8_t posto;
    uint8_t modello;                 // ModelloPad
    uint8_t batteria;
    uint8_t flag;                    // EVENTO_BATTERIA: 1 = critica (10%)
    uint8_t modalita;                // modalita' del ricevitore al momento dell'evento
    uint8_t mac[6];
};

struct VoceRete {                    // CMD_RETI: 5 voci + 2 MAC di WoL + flag
    char ssid[33];                   // "" = posto libero
    uint8_t auth;                    // 0 WPA2, 1 WPA3
    uint8_t ha_password;             // la password non esce mai dal Pico
};

struct RetiPsrx {
    VoceRete reti[PSRX_RETI_MAX];
    uint8_t wol_mac[2][6];
    uint8_t wol_spento;
};

struct ReteDati {                    // CMD_RETE_SALVA
    char ssid[33];
    char password[64];               // "" con mantieni_password = 1: tiene quella salvata
    uint8_t auth;
    uint8_t mantieni_password;
};

struct CaricamentoPsrx {             // CMD_CARICAMENTO
    uint8_t stato;                   // StatoCaricamento
    uint8_t errore;
    uint16_t settori_scritti;
    uint16_t settori_totali;
    uint16_t riservato;
    uint32_t dimensione;
};

struct CaricaInizio {                // CMD_CARICA_INIZIO
    uint32_t dimensione;
    uint8_t sha256[32];
};

struct ErroreRisposta {              // CMD_ERRORE
    uint8_t errore;
    uint8_t comando;
};

#pragma pack(pop)

static_assert(sizeof(InfoPsrx) == 76, "InfoPsrx");
static_assert(sizeof(PadPsrx) == 20, "PadPsrx");
static_assert(sizeof(StatoPsrx) == 32 + PSRX_PAD_MAX * sizeof(PadPsrx), "StatoPsrx");
static_assert(sizeof(VoceImpostazione) == 3, "VoceImpostazione");
static_assert(sizeof(VoceAbbinato) == 29, "VoceAbbinato");
static_assert(sizeof(ImpostaPad) == 8, "ImpostaPad");
static_assert(sizeof(EventoPsrx) == 14, "EventoPsrx");
static_assert(sizeof(VoceRete) == 35, "VoceRete");
static_assert(sizeof(RetiPsrx) == 5 * 35 + 13, "RetiPsrx");
static_assert(sizeof(ReteDati) == 99, "ReteDati");
static_assert(sizeof(CaricamentoPsrx) == 12, "CaricamentoPsrx");
static_assert(sizeof(CaricaInizio) == 36, "CaricaInizio");

#endif // PSRX_PROTOCOLLO_H
