//
// Multi-slot constants shared across modules (BT, state manager, USB bridge).
//
// MULTI_SLOT_COUNT comes from CMake. A slot is a *connection* seat assigned at
// connect time (session order: lowest free slot wins), not a bond: bonds are
// BD_ADDR + link-key credentials and carry no slot number.
//

#ifndef DS5_BRIDGE_SLOTS_H
#define DS5_BRIDGE_SLOTS_H

#ifndef MULTI_SLOT_COUNT
#define MULTI_SLOT_COUNT 1
#endif

#define BT_MAX_SLOTS MULTI_SLOT_COUNT

// The slot bound to the singletons that exist once per composite device: the
// DS/DSE USB identity (PID / report descriptor), the DSE profile machinery,
// and the battery LED. Audio is NOT bound here -- it follows
// tier_audio_slot() (the lowest connected slot; see tier.h).
#define BT_USB_SLOT 0

#ifdef __cplusplus
// Reset one slot's USB-facing input buffer to the neutral idle report
// (defined in main.cpp). Called on BT disconnect so a pad that drops
// mid-press doesn't leave its buttons frozen "held" on the host.
void bridge_reset_slot_input(uint8_t slot);
#endif

#endif // DS5_BRIDGE_SLOTS_H
