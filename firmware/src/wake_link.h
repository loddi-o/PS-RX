//
// wake_link.h -- companion-Pico wake signal (ENABLE_WAKE_LINK, scaffolding).
//
// GOAL: USB-HID host wake WITHOUT breaking this dongle's pure-DualSense
// enumeration. A SECOND Pico (plain Pico 2, no radio needed) plugs into its own
// USB port on the same host and enumerates as an ordinary boot keyboard under
// its own VID:PID -- nothing keyboard-shaped ever appears on the DualSense's
// VID:PID, so anticheats see only a controller. When THIS dongle decides the
// host should wake (controller connected / PS button while the host is
// suspended -- the same chokepoint that fires WOL), it signals the companion
// over a GPIO line; the companion types the F15 wake keystroke + issues USB
// remote-wakeup, exactly what the on-board keyboard (the web-UI "USB wake
// keyboard" toggle) does today.
//
// WIRE PROTOCOL (v0): one line + common ground.
//   - This dongle: WAKE_LINK_GPIO (default GP2), output, idle LOW.
//   - Wake request: the line is driven HIGH for WAKE_LINK_PULSE_MS (100 ms),
//     then returns LOW. The companion treats a rising edge as "wake the host
//     now" and debounces on its side too.
//   - The pulse is suspend-gated and rate-limited on this side (same policy as
//     WOL: only while the host is suspended, once per suspend spell), so the
//     companion may treat every edge as genuine.
// Future extension (v1+): replace the pulse with a UART byte on the same pin
// pair if richer signalling is ever needed (wake reason, controller battery,
// ...). Keep v0 dumb until the companion firmware exists.
//
// THE COMPANION FIRMWARE DOES NOT EXIST YET -- this side only emits the signal.
//

#ifndef DS5_BRIDGE_WAKE_LINK_H
#define DS5_BRIDGE_WAKE_LINK_H

#ifdef ENABLE_WAKE_LINK
// Claim the GPIO and drive it to idle (LOW). Call once at boot (wake_init()).
void wake_link_init(void);

// Start a wake pulse (non-blocking; the line is dropped by wake_link_task()).
// Caller does the suspend-gating / rate-limiting -- this just drives the wire.
void wake_link_pulse(void);

// Service the pulse timer. Call every main-loop iteration (wake_task()).
void wake_link_task(void);
#else
static inline void wake_link_init(void) {}
static inline void wake_link_pulse(void) {}
static inline void wake_link_task(void) {}
#endif // ENABLE_WAKE_LINK

#endif // DS5_BRIDGE_WAKE_LINK_H
