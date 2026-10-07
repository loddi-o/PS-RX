//
// PS-RX - prova del WiFi: domanda DNS usata per verificare che la rete arrivi a internet.
//
// Funzioni pure (le prova test/test_logica.cpp sul PC). Il ricevitore non ha un client DNS (lwIP ridotto
// al minimo): manda a mano una domanda "A github.com" a un server pubblico (1.1.1.1, poi 8.8.8.8) via UDP;
// qualsiasi risposta con lo stesso identificativo vuol dire che internet e' raggiungibile.
//

#pragma once

#include <cstdint>

constexpr uint16_t DNS_PORTA = 53;
constexpr uint16_t DNS_DOMANDA_MAX = 32;

// Scrive la domanda in buf (almeno DNS_DOMANDA_MAX byte); restituisce la lunghezza.
uint16_t dns_domanda(uint8_t *buf, uint16_t id);
// true se i dati sono una risposta DNS (bit QR) alla domanda con questo identificativo.
bool dns_risposta_valida(const uint8_t *dati, uint16_t n, uint16_t id);
