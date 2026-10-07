//
// Created by awalol on 2026/3/5.
//

#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include <cstdint>

void audio_init();
void audio_loop();
void core1_entry();
void set_headset(bool state);
void audio_set_mic_active(bool active);
void mic_add_queue(uint8_t *data);

// PS-RX: il microfono trasmette (un'app lo ha aperto).
bool audio_mic_attivo();

#endif //DS5_BRIDGE_AUDIO_H
