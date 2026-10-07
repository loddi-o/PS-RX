//
// PS-RX - audio e microfono secondo il controller che li usa (vedi audio_pad.h).
//

#include "audio_pad.h"

#include "audio.h"
#include "bt.h"
#include "config.h"
#include "log_psrx.h"
#include "tier.h"
#include "usb.h"

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
    // Forma USB: con un solo controller che non usa l'audio (spento nelle impostazioni, o DualShock 4) il
    // ricevitore si presenta senza la funzione audio, cosi' Windows toglie altoparlante e microfono.
    bool senza_audio = false;
    if (get_config().psrx_modalita == PSRX_MODALITA_PS && bt_connected_count() == 1) {
        for (uint8_t k = 0; k < BT_MAX_SLOTS; k++) {
            BtStatus s{};
            bt_get_status(k, &s);
            if (!s.connected) continue;
            const PadImpostazioni *imp = config_pad(s.addr);
            senza_audio = bt_slot_ds4(k) || (imp && imp->audio_spento);
            break;
        }
    }
    usb_request_senza_audio(senza_audio);

    // Microfono: l'host lo apre quando un'app lo usa; se per questo controller e' spento, si richiude
    // subito (il controller smette di trasmetterlo e non occupa banda).
    if ((!microfono || bloccato) && audio_mic_attivo()) audio_set_mic_active(false);
}
