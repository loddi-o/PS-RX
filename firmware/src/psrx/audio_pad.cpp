//
// PS-RX - audio e microfono secondo il controller che li usa (vedi audio_pad.h).
//

#include "audio_pad.h"

#include "audio.h"
#include "bt.h"
#include "config.h"
#include "log_psrx.h"
#include "tier.h"

extern volatile bool psrx_audio_bloccato;   // tier.cpp

void audio_pad_task() {
    const uint8_t posto = tier_audio_slot();
    BtStatus st{};
    bt_get_status(posto, &st);
    bool audio = true;
    bool microfono = true;
    if (st.connected) {
        const PadImpostazioni *p = config_pad(st.addr);
        if (bt_slot_ds4(posto)) audio = microfono = false;
        if (p && p->audio_spento) audio = false;
        if (p && p->microfono_spento) microfono = false;
    }
    // In modalita' Xbox e Steam l'USB non ha audio: il controller resta senza flusso audio (piu' banda all'input).
    const bool bloccato = !audio || get_config().psrx_modalita != PSRX_MODALITA_PS;
    if (bloccato != psrx_audio_bloccato) {
        psrx_audio_bloccato = bloccato;
        psrx_log("audio del controller nel posto %u: %s", posto + 1, bloccato ? "spento" : "attivo");
    }
    // Microfono: l'host lo apre quando un'app lo usa; se per questo controller e' spento, si richiude
    // subito (il controller smette di trasmetterlo e non occupa banda).
    if ((!microfono || bloccato) && audio_mic_attivo()) audio_set_mic_active(false);
}
