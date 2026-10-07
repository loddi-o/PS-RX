//
// Bandwidth-tier policy. See tier.h for the tier table and rationale.
//

#include "tier.h"

#include "bt.h"

uint8_t tier_audio_slot() {
#if BT_MAX_SLOTS == 1
    return 0;
#else
    const int s = bt_lowest_connected_slot();
    return s < 0 ? 0 : (uint8_t) s;
#endif
}

// PS-RX: il ciclo principale lo alza quando il controller dell'audio e' un DualShock 4 o ha l'audio
// spento nelle impostazioni (src/psrx/audio_pad.cpp).
volatile bool psrx_audio_bloccato = false;

bool tier_audio_allowed() {
#if BT_MAX_SLOTS == 1
    return !psrx_audio_bloccato;
#else
    return bt_connected_count() <= 1 && !psrx_audio_bloccato;
#endif
}
