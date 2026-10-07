//
// PS-RX - traduzione fra il report del DualSense e quello di un controller Xbox 360 (XInput).
//
// Funzioni pure (nessun accesso all'hardware): le prova test/test_logica.cpp sul PC.
//
// Report XInput in ingresso (20 byte, verso il PC):
//   [0] 0x00 tipo   [1] 0x14 lunghezza
//   [2] su 0x01, giu' 0x02, sinistra 0x04, destra 0x08, Start 0x10, Back 0x20, L3 0x40, R3 0x80
//   [3] LB 0x01, RB 0x02, Guide 0x04, A 0x10, B 0x20, X 0x40, Y 0x80
//   [4] LT  [5] RT  [6..13] LX, LY, RX, RY (int16, Y positivo in alto)  [14..19] zero
// Report XInput in uscita (dal PC):
//   vibrazione  00 08 00 <motore sinistro, forte> <motore destro, debole> 00 00 00
//   LED         01 03 <schema>   (ignorato: il colore lo decide PS-RX)
//

#pragma once

#include <cstdint>

constexpr uint8_t XINPUT_REPORT_LEN = 20;

// ds: report USB del DualSense senza Report ID (63 byte, come interrupt_in_data).
// click_back: il click del touchpad vale come Back (no quando il touchpad fa da mouse).
void xinput_da_dualsense(const uint8_t *ds, uint8_t *x, bool click_back);

// true se il report in uscita e' un comando di vibrazione; forte = motore sinistro, debole = destro.
bool xinput_vibrazione(const uint8_t *dati, uint32_t len, uint8_t *forte, uint8_t *debole);
