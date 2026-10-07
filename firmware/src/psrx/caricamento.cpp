//
// PS-RX - contabilita' del caricamento di un firmware nuovo via USB (logica pura).
//

#include "caricamento.h"

#include <cstring>

static uint32_t parola(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
           static_cast<uint32_t>(p[3]) << 24;
}

bool immagine_rp2350_valida(const uint8_t *img, uint32_t dim) {
    constexpr uint32_t FLASH = 0x10000000u, SRAM = 0x20000000u, SRAM_FINE = 0x20082000u;
    constexpr uint32_t INIZIO_BLOCCO = 0xffffded3u, FINE_BLOCCO = 0xab123579u;
    if (dim < 4096) return false;
    const uint32_t sp = parola(img), reset = parola(img + 4);
    if (sp < SRAM || sp > SRAM_FINE || (sp & 7u) != 0) return false;
    if ((reset & 1u) == 0 || (reset & ~1u) < FLASH || (reset & ~1u) >= FLASH + dim) return false;
    for (uint32_t i = 8; i + 4 <= 4096; i += 4) {
        if (parola(img + i) != INIZIO_BLOCCO) continue;
        for (uint32_t j = i + 4; j + 4 <= 4096; j += 4) {
            if (parola(img + j) == FINE_BLOCCO) return true;
        }
        return false;
    }
    return false;
}

uint8_t Caricamento::inizia(uint32_t dimensione, const uint8_t sha256[32]) {
    if (stato_ == CAR_SCRITTURA || stato_ == CAR_VERIFICA || stato_ == CAR_INSTALLAZIONE) return ERR_OCCUPATO;
    if (dimensione < minimo_ || dimensione > capacita_) return ERR_DIMENSIONE;
    dimensione_ = dimensione;
    totali_ = static_cast<uint16_t>((dimensione + PSRX_SETTORE - 1) / PSRX_SETTORE);
    scritti_ = 0;
    in_scrittura_ = 0;
    memcpy(sha_, sha256, sizeof sha_);
    errore_ = ERR_NESSUNO;
    stato_ = CAR_RICEZIONE;
    return ERR_NESSUNO;
}

uint8_t Caricamento::controlla_blocco(uint16_t indice, uint16_t lunghezza) const {
    if (stato_ == CAR_SCRITTURA) return ERR_OCCUPATO;
    if (stato_ != CAR_RICEZIONE) return ERR_SEQUENZA;
    if (lunghezza != PSRX_SETTORE) return ERR_LUNGHEZZA;
    if (indice != scritti_ || indice >= totali_) return ERR_SEQUENZA;
    return ERR_NESSUNO;
}

uint8_t Caricamento::accetta_blocco(uint16_t indice, uint16_t lunghezza) {
    const uint8_t esito = controlla_blocco(indice, lunghezza);
    if (esito != ERR_NESSUNO) return esito;
    in_scrittura_ = indice;
    stato_ = CAR_SCRITTURA;
    return ERR_NESSUNO;
}

void Caricamento::blocco_scritto(bool ok) {
    if (stato_ != CAR_SCRITTURA) return;
    if (!ok) {
        errore(ERR_FLASH);
        return;
    }
    scritti_++;
    stato_ = CAR_RICEZIONE;
}

uint8_t Caricamento::fine() {
    if (stato_ == CAR_SCRITTURA) return ERR_OCCUPATO;
    if (stato_ != CAR_RICEZIONE || scritti_ != totali_) return ERR_SEQUENZA;
    stato_ = CAR_VERIFICA;
    return ERR_NESSUNO;
}

void Caricamento::verificato(uint8_t esito) {
    if (stato_ != CAR_VERIFICA) return;
    if (esito == ERR_NESSUNO) stato_ = CAR_PRONTO;
    else errore(esito);
}

void Caricamento::errore(uint8_t codice) {
    errore_ = codice;
    stato_ = CAR_ERRORE;
}

void Caricamento::annulla() {
    stato_ = CAR_INATTIVO;
    errore_ = ERR_NESSUNO;
    scritti_ = 0;
    totali_ = 0;
    dimensione_ = 0;
}

void Caricamento::installazione() {
    if (stato_ == CAR_PRONTO) stato_ = CAR_INSTALLAZIONE;
}
