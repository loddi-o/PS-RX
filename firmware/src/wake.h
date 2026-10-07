//
// Created by awalol on 2026/4/30.
//

#ifndef DS5_BRIDGE_WAKE_H
#define DS5_BRIDGE_WAKE_H

#include <cstdint>

#ifdef ENABLE_WAKE_HID
void wake_init(void);
void wake_on_bt_input(const uint8_t *hid_input, uint16_t len);
void wake_on_bt_connect(void);
void wake_on_bt_disconnect(void);
void wake_task(void);
void wake_reset_for_variant_swap(void);
// Cold-boot autosuspend recovery (issue #4): re-issue a USB bus resume without
// advancing the wake-FSM. Called by usb_variant_task() while a variant swap is
// desired but gated by host_suspended. Returns true if a resume was issued.
bool wake_request_bus_resume(void);
#else
static inline void wake_init(void) {}
static inline void wake_on_bt_input(const uint8_t *, uint16_t) {}
static inline void wake_on_bt_connect(void) {}
static inline void wake_on_bt_disconnect(void) {}
static inline void wake_task(void) {}
static inline void wake_reset_for_variant_swap(void) {}
static inline bool wake_request_bus_resume(void) { return false; }
#endif

#endif //DS5_BRIDGE_WAKE_H
