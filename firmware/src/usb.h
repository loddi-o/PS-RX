//
// Created by awalol on 2026/3/4.
//

#ifndef DS5_BRIDGE_USB_H
#define DS5_BRIDGE_USB_H

#include <cstdint>

#include "slots.h"

extern uint8_t mute[2]; // 0: SPEAKER(0x02) 1: MIC(0x05)
extern float volume[2]; // 0: SPEAKER(0x02) 1: MIC(0x05)

#ifdef ENABLE_WAKE_HID
// Dynamic config-descriptor selection. The presented descriptor is one atomic
// target: {variant, kbd}. The variant flips when the DualSense connects or
// disconnects: minimal (inert dummy HID — wake still works, no audio or
// gamepad ghost in OS) vs full (audio + gamepad). The kbd flag — the runtime
// web-UI wake-keyboard toggle (Config_body.wake_kbd_enabled) — appends a boot
// keyboard to WHICHEVER variant is up (both or neither, never just one).
bool usb_descriptor_variant_is_full(void);

// TinyUSB HID instance index of the boot keyboard. STABLE at 1 in both
// variants: in full the gamepad is instance 0 (parsed first); in minimal a
// dummy placeholder HID holds instance 0 so the kbd stays instance 1. Kept as a
// function so callers stay decoupled from the constant.
uint8_t usb_kbd_hid_instance(void);

// True while the ENUMERATED configuration carries the boot keyboard (latched
// at swap time). Gate for all kbd runtime behavior — the F15 wake FSM and the
// HID callback routing for instance 1 — NOT the live config value, which can
// differ while a swap is pending.
bool usb_wake_kbd_active(void);

// Request a descriptor change: orchestrator notes the desired target, then
// usb_variant_task() drives a tud_disconnect()/settle/swap/tud_connect()
// bounce on the main loop. Safe to call from any context. No-op if the
// desired target is already active. The task internally refuses to act
// while the host is suspended — preserves wake-from-S3/S5 by avoiding
// USB re-enumeration mid-suspend (a pending change applies after resume).
void usb_request_variant_full(void);
void usb_request_variant_minimal(void);
#if MULTI_SLOT_COUNT > 1
// MULTI: exposed_slots gamepad interfaces (the session's high-water
// controller count, clamped 2..MULTI_SLOT_COUNT), NO audio function. See the
// variant policy in bt.cpp: entered at 2+ pads, exposure grows with each new
// join (one bounce per new player), sticky until every pad disconnects.
void usb_request_variant_multi(uint8_t exposed_slots);
#endif
void usb_request_wake_kbd(bool enabled);

// How many gamepad interfaces the host currently sees (latched active
// variant): MULTI -> MULTI_SLOT_COUNT, FULL -> 1, MINIMAL -> 0.
uint8_t usb_active_gamepad_slots(void);

// Slot <-> HID-instance map for the latched active variant. The keyboard
// (when active) is pinned at instance 1, so tail slots shift by one at
// RUNTIME depending on the kbd state -- always go through these, never
// hardcode. usb_hid_instance_slot returns -1 for the keyboard's instance;
// callers must also bound the result against usb_active_gamepad_slots().
uint8_t usb_slot_hid_instance(uint8_t slot);
int usb_hid_instance_slot(uint8_t instance);

// Seed desired AND active kbd state from the persisted config. Call once at
// boot, after config_load() and before the first tud_connect(), so the first
// enumeration already matches the saved toggle (no boot-time bounce).
void usb_descriptor_init_from_config(void);

// Drive variant-swap state machine. Call from main loop alongside
// wake_task() / btstack hci_run().
void usb_variant_task(void);

// True while a variant swap is in flight (between tud_disconnect() and
// the post-tud_connect() settle). wake.cpp uses this to ignore the
// tud_mount_cb / tud_resume_cb that fire as a consequence of our own
// re-enumeration — otherwise the wake FSM treats them as a host wake-up
// event and starts mashing F15 into the host (-> stray "fic" key spam).
bool usb_variant_swap_in_progress(void);

// Suspend-state plumbing. wake.cpp owns the authoritative suspended
// state; usb_variant_task queries this before starting/continuing a
// swap so we don't yank the bus during S3/S5.
void usb_set_host_suspended(bool suspended);

// True while the USB host has the bus suspended (S3/S4/S5) -- i.e. the PC the
// dongle is plugged into is asleep/off. The WiFi WOL transport reads this to
// gate the PS-button wake trigger: only fire WOL when the host is actually
// down, so a normal PS press during play (Steam menu) never sends a packet.
bool usb_host_suspended(void);
#else
// Without the wake subsystem there is no authoritative suspend tracking, so
// report "not suspended" -- the WiFi PS-button WOL trigger then stays inert
// (web-button WOL is unaffected). Keeps the WiFi build linkable with
// -DENABLE_WAKE_HID=OFF. Same for the wake-keyboard toggle: no dynamic
// descriptors means no keyboard, so the request is a no-op and "active" is
// always false (the web UI hides the toggle via wake_kbd_capable=false).
static inline bool usb_host_suspended(void) { return false; }
static inline bool usb_wake_kbd_active(void) { return false; }
static inline void usb_request_wake_kbd(bool) {}
static inline void usb_descriptor_init_from_config(void) {}
// Non-wake builds have no dynamic descriptors: the (single) gamepad is always
// interface 3 / HID instance 0, and device visibility is handled with
// tud_connect()/tud_disconnect(). Multi-slot requires ENABLE_WAKE_HID
// (enforced in CMakeLists), so the map degenerates to the identity.
static inline uint8_t usb_active_gamepad_slots(void) { return 1; }
static inline uint8_t usb_slot_hid_instance(uint8_t slot) { return slot; }
static inline int usb_hid_instance_slot(uint8_t instance) { return (int) instance; }
#endif

#endif //DS5_BRIDGE_USB_H