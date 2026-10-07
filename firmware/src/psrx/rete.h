//
// PS-RX - WiFi solo per il Wake-on-LAN (sostituisce wifi_net.cpp di DS5-Linux-Bridge).
//
// Implementa anche l'interfaccia di wifi_net.h usata da main.cpp (wifi_net_init, wifi_net_task,
// wifi_net_in_ap_mode) e wake_emit_wol() chiamata da wake.cpp. Quando tenere acceso il WiFi lo
// decide politica_rete.h; qui c'e' solo il lavoro sul chip: accendere/spegnere la modalita' STA,
// connettersi alle reti salvate a turno, mandare i pacchetti magici.
//

#ifndef PSRX_RETE_H
#define PSRX_RETE_H

#include <cstdint>

enum StatoRete : uint8_t {
    RETE_SPENTA = 0,       // radio WiFi spenta (controller collegati, o nessuna rete salvata)
    RETE_CONNESSIONE = 1,  // si sta connettendo a una rete salvata
    RETE_CONNESSA = 2,     // connessa, con indirizzo IP: il WoL puo' partire
    RETE_ERRORE = 3,       // nessuna rete salvata raggiungibile per ora (riprova da sola)
};

struct InfoRete {
    uint8_t stato;            // StatoRete
    int8_t rete;              // indice della rete in uso (-1 = nessuna)
    uint8_t ip[4];
    int16_t rssi;             // dBm della rete WiFi (0 = sconosciuto)
    uint8_t wol_finestra;     // 1 = finestra del WoL aperta
    uint32_t ms_da_ultimo_wol;  // 0xFFFFFFFF = mai
};

void rete_info(InfoRete *out);

// WiFi connesso con indirizzo IP (il WoL puo' partire).
bool rete_info_connessa();

// Manda subito i pacchetti magici (pulsante "Prova" dell'app). false se il WiFi non e' connesso.
bool rete_invia_wol_ora();

// Le reti salvate sono cambiate (dall'app): riparte dalla prima.
void rete_reti_cambiate();

// --- Prova di una rete salvata (pulsante "Prova" dell'app) ---------------------------------------
// Solo senza controller collegati (con un controller il WiFi e' spento). Tre passi: collegamento e
// password, indirizzo IP dal router (DHCP), internet (domanda DNS a 1.1.1.1 / 8.8.8.8). Se la prova
// riesce la connessione resta; se no il ricevitore torna a provare le reti salvate a turno.
enum FaseProva : uint8_t {
    PROVA_NESSUNA = 0, PROVA_CONNESSIONE = 1, PROVA_INDIRIZZO = 2, PROVA_INTERNET = 3, PROVA_FINITA = 4,
};
enum EsitoProva : uint8_t {
    ESITO_IN_CORSO = 0,
    ESITO_OK = 1,
    ESITO_PASSWORD = 2,          // password sbagliata
    ESITO_NON_TROVATA = 3,       // rete non trovata (spenta, lontana o solo a 5 GHz)
    ESITO_NESSUNA_RISPOSTA = 4,  // l'access point non ha completato il collegamento
    ESITO_NESSUN_IP = 5,         // collegato ma il router non ha dato un indirizzo (DHCP)
    ESITO_NO_INTERNET = 6,       // rete locale ok, internet non raggiungibile
    ESITO_ANNULLATA = 7,         // si e' collegato un controller (il WiFi si spegne)
};
struct ProvaRete {
    uint8_t fase;                // FaseProva
    uint8_t esito;               // EsitoProva
    int8_t rete;                 // indice della rete provata
    int8_t rssi;                 // dBm (0 = non misurato)
    uint8_t ip[4];
    uint8_t gateway[4];
    uint16_t ms_internet;        // tempo di risposta del DNS
    uint16_t ms_totale;
};
static_assert(sizeof(ProvaRete) == 16, "ProvaRete");

// false se il WiFi e' spento (controller collegati) o la rete non e' salvata.
bool rete_avvia_prova(uint8_t indice);
void rete_esito_prova(ProvaRete *out);

#endif // PSRX_RETE_H
