//
// PS-RX - touchpad del controller come mouse (logica pura, testata sul PC).
//
// Il touchpad (1920 x 1080 punti, formato del DualSense) diventa:
// - zona principale: muove il puntatore (movimento relativo, come un trackpad di portatile);
// - striscia sinistra (X < LARGHEZZA_ROTELLA): rotellina, un passo ogni PASSO_ROTELLA punti in
//   verticale; il dito che comincia nella striscia resta "rotellina" finche' non si alza;
// - click del touchpad: tasto sinistro; click con due dita appoggiate: tasto destro.
// Il gioco continua a ricevere il touchpad nel report del gamepad.
//

#ifndef PSRX_TRACKPAD_H
#define PSRX_TRACKPAD_H

#include <cstdint>

constexpr uint16_t LARGHEZZA_ROTELLA = 384;   // 20% di 1920
constexpr int32_t PASSO_ROTELLA = 60;          // punti per un passo della rotellina
constexpr int32_t GUADAGNO_16 = 12;            // velocita' del puntatore: 12/16 = 0,75 pixel per punto
constexpr int32_t SALTO_MAX = 300;             // spostamento oltre il quale e' un dito nuovo, non un movimento

struct Tocco {
    bool attivo;
    uint16_t x;
    uint16_t y;
};

struct StatoTouchpad {
    Tocco dito[2];
    bool click;
};

struct MovimentoMouse {
    int32_t dx;
    int32_t dy;
    int32_t rotella;
    uint8_t tasti;     // bit 0 sinistro, bit 1 destro
};

// Legge i due punti e il click dal report del DualSense (63 byte senza Report ID).
StatoTouchpad touchpad_da_report(const uint8_t *report);

class TrackpadMouse {
public:
    MovimentoMouse aggiorna(const StatoTouchpad &t, bool inverti);

private:
    bool tocco_ = false;
    bool rotella_ = false;
    int32_t x_ = 0;
    int32_t y_ = 0;
    int32_t resto_x_ = 0;
    int32_t resto_y_ = 0;
    int32_t accumulo_rotella_ = 0;
    bool destro_ = false;      // il click in corso e' cominciato con due dita
    bool click_prima_ = false;
};

#endif // PSRX_TRACKPAD_H
