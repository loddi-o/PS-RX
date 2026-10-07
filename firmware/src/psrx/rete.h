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

#endif // PSRX_RETE_H
