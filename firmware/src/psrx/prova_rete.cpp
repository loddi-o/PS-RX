//
// PS-RX - prova del WiFi: domanda DNS (vedi prova_rete.h).
//

#include "prova_rete.h"

#include <cstring>

uint16_t dns_domanda(uint8_t *b, uint16_t id) {
    // intestazione: id, flag 0x0100 (ricorsione richiesta), 1 domanda, 0 risposte/autorita'/aggiuntivi
    const uint8_t intestazione[12] = {static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id & 0xFF), 0x01, 0x00,
                                      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    // domanda: github.com, tipo A (1), classe IN (1)
    const uint8_t domanda[] = {6, 'g', 'i', 't', 'h', 'u', 'b', 3, 'c', 'o', 'm', 0, 0x00, 0x01, 0x00, 0x01};
    memcpy(b, intestazione, sizeof intestazione);
    memcpy(b + sizeof intestazione, domanda, sizeof domanda);
    return static_cast<uint16_t>(sizeof intestazione + sizeof domanda);
}

bool dns_risposta_valida(const uint8_t *d, uint16_t n, uint16_t id) {
    if (n < 12) return false;
    if (d[0] != static_cast<uint8_t>(id >> 8) || d[1] != static_cast<uint8_t>(id & 0xFF)) return false;
    return (d[2] & 0x80) != 0;   // QR = 1: e' una risposta
}
