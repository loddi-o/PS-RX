//
// PS-RX - contabilita' del caricamento di un firmware nuovo via USB (logica pura).
//
// L'app manda INIZIO (dimensione + SHA-256), poi i blocchi da 4 KB in ordine, poi FINE.
// Ogni blocco viene scritto in flash dal ciclo principale (aggiornamento.cpp); il blocco
// successivo e' accettato solo dopo la scrittura del precedente. Alla FINE si verifica lo
// SHA-256 dell'immagine scritta: se va bene il firmware e' "pronto" e l'app chiede di
// installarlo (solo senza controller collegati).
//

#ifndef PSRX_CARICAMENTO_H
#define PSRX_CARICAMENTO_H

#include <cstdint>

#include "protocollo.h"

// Controllo minimo che un'immagine sia un firmware RP2350 per il Pico 2 W (logica pura):
//   - tabella dei vettori all'inizio: stack in SRAM, reset nell'immagine (indirizzo Thumb);
//   - blocco IMAGE_DEF (marcatori 0xffffded3 ... 0xab123579) nei primi 4 KB.
bool immagine_rp2350_valida(const uint8_t *immagine, uint32_t dimensione);

class Caricamento {
public:
    Caricamento(uint32_t capacita, uint32_t minimo) : capacita_(capacita), minimo_(minimo) {}

    // Restituiscono ERR_NESSUNO oppure il motivo del rifiuto (lo stato non cambia).
    uint8_t inizia(uint32_t dimensione, const uint8_t sha256[32]);
    uint8_t accetta_blocco(uint16_t indice, uint16_t lunghezza);
    uint8_t fine();

    // Come accetta_blocco() ma senza cambiare stato: serve all'inizio della richiesta USB,
    // prima che arrivino i dati.
    uint8_t controlla_blocco(uint16_t indice, uint16_t lunghezza) const;

    void blocco_scritto(bool ok);          // esito della scrittura del blocco accettato
    void verificato(uint8_t esito);        // ERR_NESSUNO = pronto
    void errore(uint8_t codice);           // passa in CAR_ERRORE
    void annulla();                        // torna in CAR_INATTIVO
    void installazione();                  // CAR_PRONTO -> CAR_INSTALLAZIONE

    StatoCaricamento stato() const { return stato_; }
    uint8_t codice_errore() const { return errore_; }
    uint16_t settori_scritti() const { return scritti_; }
    uint16_t settori_totali() const { return totali_; }
    uint16_t blocco_in_scrittura() const { return in_scrittura_; }
    uint32_t dimensione() const { return dimensione_; }
    const uint8_t *sha256() const { return sha_; }

    // Caricamento o verifica in corso (per l'anello LED).
    bool in_corso() const {
        return stato_ == CAR_RICEZIONE || stato_ == CAR_SCRITTURA || stato_ == CAR_VERIFICA;
    }

private:
    uint32_t capacita_;
    uint32_t minimo_;
    StatoCaricamento stato_ = CAR_INATTIVO;
    uint8_t errore_ = ERR_NESSUNO;
    uint16_t scritti_ = 0;
    uint16_t totali_ = 0;
    uint16_t in_scrittura_ = 0;
    uint32_t dimensione_ = 0;
    uint8_t sha_[32] = {};
};

#endif // PSRX_CARICAMENTO_H
