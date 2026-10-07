//
// Conta i click di un tasto letto a intervalli regolari: serve per il tasto BOOTSEL del
// Pico, che si legge ogni T_BOOTSEL_LETTURA_MS e solo senza controller connessi.
// Logica pura: il tempo arriva come parametro (testabile sul PC).
//
// Una serie di click finisce T_BOOTSEL_SERIE_MS dopo l'ultimo rilascio: in quel momento
// aggiorna() restituisce il numero di click. Una pressione piu' lunga di T_CLICK_MAX_MS
// annulla la serie (non e' un click).
//

#ifndef PSRX_CONTA_CLICK_H
#define PSRX_CONTA_CLICK_H

#include <cstdint>

class ContaClick {
public:
    // Restituisce 0 finche' la serie non e' finita, poi il numero di click (una volta sola).
    uint8_t aggiorna(bool premuto, uint32_t ora_ms);

    // Dimentica la serie in corso (per esempio quando si connette un controller).
    void azzera();

private:
    bool premuto_ = false;
    bool annullata_ = false;
    uint8_t click_ = 0;
    uint32_t t_pressione_ = 0;
    uint32_t t_rilascio_ = 0;
};

#endif // PSRX_CONTA_CLICK_H
