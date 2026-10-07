//
// PS-RX - traduzione DualSense -> XInput (vedi xinput.h).
//

#include "xinput.h"

#include <cstring>

namespace {

// Byte del DualSense (0..255, centro 128) -> asse XInput (-32768..32767). 258 = 32768/127 circa:
// gli estremi arrivano al fondo scala, il centro resta 0.
int16_t asse(uint8_t v, bool inverti) {
    int32_t a = (static_cast<int32_t>(v) - 128) * 258;
    if (inverti) a = -a;
    if (a > 32767) a = 32767;
    if (a < -32768) a = -32768;
    return static_cast<int16_t>(a);
}

void scrivi16(uint8_t *p, int16_t v) {
    const uint16_t u = static_cast<uint16_t>(v);
    p[0] = static_cast<uint8_t>(u & 0xFF);
    p[1] = static_cast<uint8_t>(u >> 8);
}

// Croce del DualSense (0 su, 1 su-destra, ... 7 su-sinistra, 8 nessuna) -> bit XInput.
constexpr uint8_t CROCE[9] = {0x01, 0x09, 0x08, 0x0A, 0x02, 0x06, 0x04, 0x05, 0x00};

} // namespace

void xinput_da_dualsense(const uint8_t *ds, uint8_t *x, bool click_back) {
    memset(x, 0, XINPUT_REPORT_LEN);
    x[1] = XINPUT_REPORT_LEN;
    const uint8_t b0 = ds[7], b1 = ds[8], b2 = ds[9];
    const uint8_t croce = b0 & 0x0F;
    uint8_t t0 = croce < 9 ? CROCE[croce] : 0;
    if (b1 & 0x20) t0 |= 0x10;                              // Options -> Start
    if ((b1 & 0x10) || (click_back && (b2 & 0x02))) t0 |= 0x20;  // Create (o click del touchpad) -> Back
    if (b1 & 0x40) t0 |= 0x40;                              // L3
    if (b1 & 0x80) t0 |= 0x80;                              // R3
    uint8_t t1 = 0;
    if (b1 & 0x01) t1 |= 0x01;                              // L1 -> LB
    if (b1 & 0x02) t1 |= 0x02;                              // R1 -> RB
    if (b2 & 0x01) t1 |= 0x04;                              // PS -> Guide
    if (b0 & 0x20) t1 |= 0x10;                              // croce -> A
    if (b0 & 0x40) t1 |= 0x20;                              // cerchio -> B
    if (b0 & 0x10) t1 |= 0x40;                              // quadrato -> X
    if (b0 & 0x80) t1 |= 0x80;                              // triangolo -> Y
    x[2] = t0;
    x[3] = t1;
    x[4] = ds[4];                                           // L2 -> LT
    x[5] = ds[5];                                           // R2 -> RT
    scrivi16(x + 6, asse(ds[0], false));
    scrivi16(x + 8, asse(ds[1], true));                     // Y del DualSense: 0 in alto
    scrivi16(x + 10, asse(ds[2], false));
    scrivi16(x + 12, asse(ds[3], true));
}

bool xinput_vibrazione(const uint8_t *dati, uint32_t len, uint8_t *forte, uint8_t *debole) {
    if (len < 5 || dati[0] != 0x00 || dati[1] != 0x08) return false;
    *forte = dati[3];
    *debole = dati[4];
    return true;
}
