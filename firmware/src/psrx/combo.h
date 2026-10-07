//
// PS-RX - combinazione Share/Create + Options + L2 + R2: spegne subito quel controller (logica pura).
//
// Si legge dal byte dei tasti del report gia' ricevuto (byte 8 del report del DualSense, uguale per il
// DualShock 4 tradotto): L2 (0x04), R2 (0x08), Share/Create (0x10), Options (0x20) premuti insieme.
// Scatta una volta per pressione; serve rilasciare tutto prima di un nuovo spegnimento.
//

#ifndef PSRX_COMBO_H
#define PSRX_COMBO_H

#include <cstdint>

constexpr uint8_t COMBO_SPEGNI = 0x04 | 0x08 | 0x10 | 0x20;

class ComboSpegni {
public:
    // true nel giro in cui la combinazione viene completata.
    bool aggiorna(uint8_t tasti) {
        const bool premuta = (tasti & COMBO_SPEGNI) == COMBO_SPEGNI;
        const bool scatta = premuta && armata_;
        if (premuta) armata_ = false;
        if ((tasti & COMBO_SPEGNI) == 0) armata_ = true;
        return scatta;
    }
    void azzera() { armata_ = true; }

private:
    bool armata_ = true;
};

#endif // PSRX_COMBO_H
