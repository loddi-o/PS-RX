//
// PS-RX - touchpad del controller come mouse (vedi trackpad.h).
//

#include "trackpad.h"

namespace {
Tocco punto(const uint8_t *p) {
    Tocco t;
    t.attivo = (p[0] & 0x80) == 0;
    t.x = static_cast<uint16_t>(p[1] | (p[2] & 0x0F) << 8);
    t.y = static_cast<uint16_t>((p[2] >> 4) | p[3] << 4);
    return t;
}

int32_t assoluto(int32_t v) { return v < 0 ? -v : v; }
}

StatoTouchpad touchpad_da_report(const uint8_t *report) {
    StatoTouchpad s;
    s.dito[0] = punto(&report[32]);
    s.dito[1] = punto(&report[36]);
    s.click = (report[9] & 0x02) != 0;
    return s;
}

MovimentoMouse TrackpadMouse::aggiorna(const StatoTouchpad &t, bool inverti) {
    MovimentoMouse m{0, 0, 0, 0};
    const Tocco &d = t.dito[0];

    if (d.attivo) {
        const int32_t x = d.x, y = d.y;
        const bool nuovo = !tocco_ || assoluto(x - x_) > SALTO_MAX || assoluto(y - y_) > SALTO_MAX;
        if (nuovo) {
            rotella_ = d.x < LARGHEZZA_ROTELLA;
            resto_x_ = resto_y_ = accumulo_rotella_ = 0;
        } else if (rotella_) {
            accumulo_rotella_ += y - y_;
            while (accumulo_rotella_ >= PASSO_ROTELLA) {
                m.rotella -= 1;       // dito verso il basso: rotellina verso il basso
                accumulo_rotella_ -= PASSO_ROTELLA;
            }
            while (accumulo_rotella_ <= -PASSO_ROTELLA) {
                m.rotella += 1;
                accumulo_rotella_ += PASSO_ROTELLA;
            }
            if (inverti) m.rotella = -m.rotella;
        } else {
            resto_x_ += (x - x_) * GUADAGNO_16;
            resto_y_ += (y - y_) * GUADAGNO_16;
            m.dx = resto_x_ / 16;
            m.dy = resto_y_ / 16;
            resto_x_ -= m.dx * 16;
            resto_y_ -= m.dy * 16;
        }
        x_ = x;
        y_ = y;
    }
    tocco_ = d.attivo;

    if (t.click && !click_prima_) destro_ = t.dito[1].attivo;   // il tasto si decide all'inizio del click
    click_prima_ = t.click;
    if (t.click) m.tasti = destro_ ? 0x02 : 0x01;
    return m;
}
