//
// Created by awalol on 2026/3/4.
//

#include "audio.h"
#include "bsp/board_api.h"
#include "bt.h"
#include "flash_safety.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "state_mgr.h"
#include "usb.h"
#include "utils.h"
#include "wake.h"
#include <cstdio>
#include <malloc.h> // mallinfo(): boot heap telemetry

#include "config.h"
#include "dse.h"
#include "ota.h"
#include "tier.h"
#include "weblog.h"
#include "wifi_net.h"

#if ENABLE_BATT_LED
#include "battery_led.h"
#endif

// Pico SDK speciifically for waiting on conditions
#include "pico/critical_section.h"
#include "pico/time.h"
#include <cstdint>
#include <cstring>

// S3-wake-wedge leak guard: usbd_edpt_release() to drop a leaked CLAIMED bit on
// the gamepad IN endpoint after a failed report.
#include "device/usbd_pvt.h"

int reportSeqCounter[BT_MAX_SLOTS] = {};
bool spk_active = false;

namespace {
constexpr uint8_t HID_INPUT_REPORT_LEN = 63;
constexpr uint8_t BT_INPUT_REPORT_OFFSET = 3;
// Depth 8 (was 4 single-slot): shared across up to 4 pads' 1 kHz streams.
constexpr uint8_t REALTIME_HID_QUEUE_DEPTH = 8;

struct RealtimeHidReport {
  uint8_t slot; // which gamepad interface this report belongs to
  uint8_t data[HID_INPUT_REPORT_LEN];
};

RealtimeHidReport realtime_hid_queue[REALTIME_HID_QUEUE_DEPTH];
uint8_t realtime_hid_head = 0;
uint8_t realtime_hid_tail = 0;
uint8_t realtime_hid_count = 0;

uint8_t realtime_hid_prev_index(uint8_t index) {
  return index == 0 ? REALTIME_HID_QUEUE_DEPTH - 1 : index - 1;
}

// Neutral/idle DualSense input report: centered sticks, no buttons. Every
// slot's buffer starts from this so the host sees a quiet pad (not garbage)
// before the first BT report lands.
constexpr uint8_t idle_input_report[HID_INPUT_REPORT_LEN] = {
    0x7f, 0x7d, 0x7f, 0x7e, 0x00, 0x00, 0xa7, 0x08, 0x00, 0x00, 0x00,
    0x52, 0x43, 0x30, 0x41, 0x01, 0x00, 0x0e, 0x00, 0xef, 0xff, 0x03,
    0x03, 0x7b, 0x1b, 0x18, 0xf0, 0xcc, 0x9c, 0x60, 0x00, 0xfc, 0x80,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xa7, 0xad, 0x60, 0x00, 0x29, 0x18, 0x00,
    0x53, 0x9f, 0x28, 0x35, 0xa5, 0xa8, 0x0c, 0x8b};
} // namespace

// Latest gamepad input report per slot (bt.cpp and battery_led.cpp read these
// too). Row `slot` feeds HID instance usb_slot_hid_instance(slot).
uint8_t interrupt_in_data[BT_MAX_SLOTS][HID_INPUT_REPORT_LEN];

critical_section_t report_cs;

void realtime_hid_queue_push(uint8_t slot, const uint8_t *report) {
  critical_section_enter_blocking(&report_cs);

  // Keep interrupt_in_data as the latest report for non-HID side consumers
  // such as battery_led_tick() and /api/status.
  memcpy(interrupt_in_data[slot], report, HID_INPUT_REPORT_LEN);

  if (realtime_hid_count == REALTIME_HID_QUEUE_DEPTH) {
    realtime_hid_tail = (realtime_hid_tail + 1) % REALTIME_HID_QUEUE_DEPTH;
    realtime_hid_count--;
  }

  realtime_hid_queue[realtime_hid_head].slot = slot;
  memcpy(realtime_hid_queue[realtime_hid_head].data, report, HID_INPUT_REPORT_LEN);
  realtime_hid_head = (realtime_hid_head + 1) % REALTIME_HID_QUEUE_DEPTH;
  realtime_hid_count++;

  critical_section_exit(&report_cs);
}

bool realtime_hid_queue_pop(uint8_t *slot, uint8_t *report) {
  bool popped = false;

  critical_section_enter_blocking(&report_cs);
  if (realtime_hid_count > 0) {
    *slot = realtime_hid_queue[realtime_hid_tail].slot;
    memcpy(report, realtime_hid_queue[realtime_hid_tail].data, HID_INPUT_REPORT_LEN);
    realtime_hid_tail = (realtime_hid_tail + 1) % REALTIME_HID_QUEUE_DEPTH;
    realtime_hid_count--;
    popped = true;
  }
  critical_section_exit(&report_cs);

  return popped;
}

void realtime_hid_queue_requeue_front(uint8_t slot, const uint8_t *report) {
  critical_section_enter_blocking(&report_cs);

  if (realtime_hid_count == REALTIME_HID_QUEUE_DEPTH) {
    realtime_hid_head = realtime_hid_prev_index(realtime_hid_head);
    realtime_hid_count--;
  }

  realtime_hid_tail = realtime_hid_prev_index(realtime_hid_tail);
  realtime_hid_queue[realtime_hid_tail].slot = slot;
  memcpy(realtime_hid_queue[realtime_hid_tail].data, report, HID_INPUT_REPORT_LEN);
  realtime_hid_count++;

  critical_section_exit(&report_cs);
}

// Reset one slot's USB-facing input buffer to the neutral idle report (see
// slots.h). Called on BT disconnect so a pad that drops mid-press doesn't
// leave its buttons frozen "held" on the host. The realtime-queue push makes
// the 1 kHz path emit the neutral report once; fixed-cadence modes re-send
// the buffer anyway.
void bridge_reset_slot_input(uint8_t slot) {
  if (slot >= BT_MAX_SLOTS) return;
  realtime_hid_queue_push(slot, idle_input_report);
}

// Called twice per main-loop iteration: once early (right after tud_task, to
// drain the realtime queue with minimal latency) and once at the canonical
// bottom-of-loop spot. `drain_only` marks the early call: it services ONLY the
// realtime queue (polling_rate_mode == 2). The non-realtime path (modes 0/1)
// re-sends the same interrupt_in_data unconditionally, so running it on both
// calls would emit the report up to twice per iteration; restrict it to the
// single bottom call to preserve the original modes-0/1 cadence.

// S3-wake-wedge leak guard + throttled logger.
//
// tud_hid_report failing while tud_hid_ready() just returned true in the SAME
// main-loop iteration is an inconsistent TinyUSB state by construction:
// ready => ep_in != 0 && !BUSY, dcd_edpt_xfer on the RP2 port cannot return
// false, the 63-byte report always fits CFG_TUD_HID_EP_BUFSIZE, and nothing
// else runs between the check and the call. The one state that fails
// PERSISTENTLY is a leaked CLAIMED bit on the gamepad IN endpoint's ep_status
// slot (claimed=1, busy=0): tud_hid_ready checks only BUSY, usbd_edpt_claim
// checks both, and only a completed transfer or a bus reset clears CLAIMED. It
// gets planted during the host's post-wake enumeration churn (a report armed
// while the host isn't yet polling the interface) and never clears -> the
// permanent tud_hid_report failure flood + dead pad seen on the S3-wake wedge.
//
// FIX: on a failed send, RELEASE the stale claim so the very next iteration can
// re-claim and succeed. usbd_edpt_release is a no-op unless (claimed && !busy),
// so on a healthy transient miss it does nothing; on the leaked state it drops
// the leak. HW-confirmed a single release clears the leak, so releasing on every
// failed iteration heals it immediately. Belt-and-suspenders guard that makes
// the wedge unreachable regardless of how the claim got planted; the primary
// fix is the distinct-bcdDevice-per-variant change in usb_descriptors.cpp.
// Per-slot gamepad IN endpoints. Slot 0 = 0x84 (FULL gamepad / MINIMAL dummy,
// shared addr); slots 1-3 are the MULTI tail interfaces. Must match the
// endpoint plan in usb_descriptors.cpp.
static constexpr uint8_t GAMEPAD_EP_IN[MULTI_SLOT_COUNT > 4 ? MULTI_SLOT_COUNT : 4] = {
    0x84, 0x88, 0x89, 0x8A};

static void handle_hid_report_failure(uint8_t slot) {
  // Drop a possibly-leaked CLAIMED bit so the next send can proceed. No-op on a
  // healthy endpoint (release requires claimed && !busy).
  usbd_edpt_release(0, GAMEPAD_EP_IN[slot]);

  static uint64_t last_log_us = 0;
  static uint32_t suppressed = 0;
  const uint64_t now = time_us_64();
  if (now - last_log_us > 1000000) {
    if (suppressed) {
      printf("[USBHID] tud_hid_report error (+%lu suppressed in last window)\n",
             (unsigned long)suppressed);
    } else {
      printf("[USBHID] tud_hid_report error\n");
    }
    last_log_us = now;
    suppressed = 0;
  } else {
    suppressed++;
  }
}

void interrupt_loop(bool drain_only = false) {
  // Only variants that expose gamepad interfaces emit gamepad reports: in
  // MINIMAL instance 0 is an inert dummy HID. (The keyboard's instance can
  // never receive a gamepad report regardless -- see the slot<->instance map
  // in usb_descriptors.cpp. This guard just avoids pushing reports at the
  // dummy / before any controller is connected.)
  const uint8_t exposed = usb_active_gamepad_slots();
  if (exposed == 0)
    return;

  // TODO: Refactor for better code reuse
  if (get_config().polling_rate_mode != 2) {
    if (drain_only)
      return; // non-realtime: only emit from the bottom-of-loop call
    // Fixed-cadence mode: re-send every exposed slot's latest buffer; empty
    // slots keep reporting their neutral idle state.
    for (uint8_t slot = 0; slot < exposed; slot++) {
      const uint8_t inst = usb_slot_hid_instance(slot);
      if (!tud_hid_n_ready(inst))
        continue;
      if (!tud_hid_n_report(inst, 0x01, interrupt_in_data[slot], HID_INPUT_REPORT_LEN)) {
        handle_hid_report_failure(slot);
      }
    }
    return;
  }

  uint8_t slot;
  uint8_t safe_report[HID_INPUT_REPORT_LEN];
  const bool should_send = realtime_hid_queue_pop(&slot, safe_report);

  // Only send to TinyUSB if we actually grabbed fresh data
  if (should_send) {
    if (slot >= exposed)
      return; // slot not exposed by the active variant (e.g. mid-swap); drop
    const uint8_t inst = usb_slot_hid_instance(slot);
    if (!tud_hid_n_ready(inst)) {
      realtime_hid_queue_requeue_front(slot, safe_report);
      return;
    }
    if (!tud_hid_n_report(inst, 0x01, safe_report, HID_INPUT_REPORT_LEN)) {
      handle_hid_report_failure(slot);
      realtime_hid_queue_requeue_front(slot, safe_report);
    }
  }
}

// Push one slot's cached output state to its controller as a BT 0x31 report.
// RAM-resident: on the controller->host output path. Kept out of flash so a
// core1 Opus-decode XIP-cache eviction can't add a flash-refetch stall here.
void __not_in_flash_func(state_push_slot_to_bt)(uint8_t slot) {
  uint8_t outputData[78]{};
  outputData[0] = 0x31;
  outputData[1] = reportSeqCounter[slot] << 4;
  reportSeqCounter[slot] = (reportSeqCounter[slot] + 1) & 0x0F;
  outputData[2] = 0x10;
  state_get(slot, outputData + 3, sizeof(SetStateData));
  bt_write(slot, outputData, sizeof(outputData));
}

void __not_in_flash_func(state_push_to_bt)() {
  // Skip only while audio frames are actually flowing (they carry the state
  // themselves); see the matching condition in tud_hid_set_report_cb.
  if (spk_active && tier_audio_allowed()) {
    return;
  }
  state_push_slot_to_bt(tier_audio_slot());
}

// RAM-resident: this is THE per-packet controller input handler -- it runs for
// every BT input report. The SDK BT data path is already RAM-relocated (see the
// relocate_to_ram() list in CMakeLists.txt); this is the application tail of
// that path. Keeping it out of flash XIP means a core1 Opus-decode cache thrash
// can't stall the next incoming report's processing (the outlier-tail mechanism).
void __not_in_flash_func(on_bt_data)(uint8_t slot, CHANNEL_TYPE channel, uint8_t *data, uint16_t len) {
  // printf("[Main] BT data callback: slot=%u channel=%u len=%u\n", slot, channel, len);
  if (channel == INTERRUPT && len > 2 && data[1] == 0x31) {
    // Audio-path concerns (mic frames, mute button, headset jack) belong to
    // the audio slot only; other pads' reports skip straight to the input
    // bridge below.
    const bool is_audio_slot = (slot == tier_audio_slot());
    if (data[2] >> 1 & 1) {
      if (is_audio_slot && tier_audio_allowed()) {
        mic_add_queue(data + 4);
      }
      return;
    }
    if (len < BT_INPUT_REPORT_OFFSET + HID_INPUT_REPORT_LEN) {
      return;
    }

    // Mute button detection (data[12] corresponds to byte 9 of input data)
    if (is_audio_slot && !g_host_hid_manages_mute) {
      static bool prev_mute_pressed = false;
      bool mute_pressed = (data[12] & 0x04) != 0;
      if (mute_pressed && !prev_mute_pressed) {
        state_toggle_local_mute();
        state_push_to_bt();
      }
      prev_mute_pressed = mute_pressed;
    }

    // Track actual DS5 jack state separately — interrupt_in_data[53]
    // has its HP_DETECT bit forced high for host UCM routing and cannot
    // be used as the previous-state comparison here.
    if (is_audio_slot) {
      static uint8_t last_jack_state =
          0xFF; // sentinel: force set_headset on first report
      const uint8_t cur_jack_state = data[56] & 1;
      if (cur_jack_state != last_jack_state) {
        set_headset(cur_jack_state);
        last_jack_state = cur_jack_state;
      }
    }

    // Wake-on-PS must observe every BT input report regardless of polling
    // mode: the wake feature has its own state to maintain (button-byte
    // diff for edge detection) and short-circuiting it on non-2 polling
    // modes silently breaks wake while the host is suspended. Any pad's PS
    // press may wake the host.
    // Wake path: USB remote-wakeup (wake.cpp) AND, on ENABLE_WIFI_WOL builds, a
    // companion Wake-on-LAN packet fired from the same request_host_wake()
    // chokepoint inside wake.cpp. Both run when the host is suspended -- USB wake
    // covers S3, WOL covers S4/S5 (indistinguishable over USB; see wake.cpp).
    wake_on_bt_input(data + 3, len - 3);

    // interrupt_in_data[slot][53] = dualsense_input_report.status[1]:
    //   bit 0 = HP_DETECT  (headphones plugged into DS5 3.5mm jack)
    //   bit 1 = MIC_DETECT (headset mic plugged into DS5 3.5mm jack)
    // hid-playstation (≥6.18) reads these and emits SW_HEADPHONE_INSERT /
    // SW_MICROPHONE_INSERT input events. The USB audio mixer quirk (≥6.17)
    // wires those to "Headphone Jack" / "Headset Mic Jack" ALSA controls,
    // which alsa-ucm-conf uses to switch between mono Internal Speaker and
    // stereo Headphones profiles. We pass the DS5's real values through
    // unchanged — the DS5 hardware jack sensor is authoritative.
    // (realtime_hid_queue_push also refreshes interrupt_in_data[slot], so
    // both polling paths keep the latest-report row current.)
    realtime_hid_queue_push(slot, data + BT_INPUT_REPORT_OFFSET);
#if ENABLE_BATT_LED
    if (slot == BT_USB_SLOT) {
      battery_led_note_report();
    }
#endif
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
#ifdef ENABLE_WAKE_HID
  // Route the keyboard instance to the wake keyboard only while the kbd is
  // actually in the ENUMERATED configuration (usb_wake_kbd_active(), latched
  // at swap time -- not the config value, which may differ while a swap is
  // pending). With the kbd inactive its instance number belongs to a gamepad
  // slot (see the slot<->instance map in usb_descriptors.cpp).
  if (usb_wake_kbd_active() && itf == usb_kbd_hid_instance()) {
    if (reqlen >= 8) {
      memset(buffer, 0, 8);
      return 8;
    }
    return 0;
  }
#endif
  (void)report_type;

  // MINIMAL's instance 0 is the inert dummy HID, NOT a gamepad. Don't route
  // its GET_REPORT into the BT feature path (which would query a controller
  // that isn't connected). Return 0 (STALL); the host never reads it.
  // usb_hid_instance_slot() also rejects instances beyond the active
  // variant's exposure (and the kbd instance).
  const int mapped = usb_hid_instance_slot(itf);
  if (mapped < 0 || mapped >= (int) usb_active_gamepad_slots()) {
    return 0;
  }
  const uint8_t slot = (uint8_t) mapped;

  BtStatus st;
  bt_get_status(slot, &st);
  if (!st.connected) {
    // Empty slot: serve a plausible blob so hid-playstation's bind-time
    // probes (calibration 0x05, firmware 0x20, pairing 0x09) don't stall the
    // interface — a stalled probe fails the driver bind and the slot stays
    // dead until re-enumeration. Prefer a live pad's cache, else the RAM
    // snapshot from the last pad that completed its feature exchange (covers
    // the all-pads-off-during-host-sleep resume window). Caveat: a pad that
    // connects AFTER enumeration inherits the placeholder IMU calibration
    // until the next re-enumeration.
    std::vector<uint8_t> ph;
    if (!bt_feature_cached_any(report_id, ph)) {
      bt_feature_snapshot_get(report_id, ph);
    }
    if (ph.size() <= 1) {
      return 0;
    }
    uint16_t n = (uint16_t)(ph.size() - 1);
    if (n > reqlen) n = reqlen;
    memcpy(buffer, ph.data() + 1, n);
    if (report_id == 0x09 && n >= 6) {
      // Pairing info carries the controller MAC, which hosts use as the
      // device's unique id — make each empty slot's MAC distinct.
      buffer[0] ^= (uint8_t)(slot + 1);
    }
    return n;
  }

  // DSE profiles: while the unlock + prefetch is still in progress, return 0
  // (NAK) for profile reads so the PS app retries rather than caching an
  // empty snapshot. Still kick off the background BT fetch. (The DSE profile
  // machinery serves the USB-identity slot only.)
  if (slot == BT_USB_SLOT && dse_is_profile_report(report_id) && !dse_profiles_ready()) {
    get_feature_data(slot, report_id, reqlen);
    return 0;
  }

  std::vector<uint8_t> feature_data = get_feature_data(slot, report_id, reqlen);
  if (feature_data.empty()) {
    return 0;
  }

  // feature_data is cached verbatim from L2CAP 0xA3 control packets whose size
  // can run up to the BT MTU; `buffer` is TinyUSB's control buffer capped at
  // CFG_TUD_HID_EP_BUFSIZE (64 B). Clamp so a hostile/buggy controller answering
  // a GET with an over-length report can't overflow the buffer.
  uint16_t n = (uint16_t)(feature_data.size() - 1);
  if (n > reqlen) n = reqlen;
  memcpy(buffer, feature_data.data() + 1, n);
  return n;
}

bool tud_audio_set_itf_cb(uint8_t rhport,
                          tusb_control_request_t const *p_request) {
  (void)rhport;
  uint8_t const itf = tu_u16_low(p_request->wIndex); // wInterface
  uint8_t const alt = tu_u16_low(p_request->wValue); // bAlternateSetting

  if (itf == 1) {
    printf("[AUDIO] Set interface Speaker to alternate setting %d\n", alt);
    spk_active = alt;
  } else if (itf == 2) {
    printf("[AUDIO] Set interface Mic to alternate setting %d\n", alt);
    audio_set_mic_active(alt != 0);
  }

  return true;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
#ifdef ENABLE_WAKE_HID
  // Same enumerated-state gate as tud_hid_get_report_cb above.
  if (usb_wake_kbd_active() && itf == usb_kbd_hid_instance()) {
    // Drop keyboard SET_REPORT (host LED state).
    return;
  }
#endif
  (void)report_type;

  // MINIMAL's instance 0 is the inert dummy HID; ignore any report to it, and
  // reject instances beyond the active variant's exposure.
  const int mapped = usb_hid_instance_slot(itf);
  if (mapped < 0 || mapped >= (int) usb_active_gamepad_slots()) {
    return;
  }
  const uint8_t slot = (uint8_t) mapped;

  // A zero-length interrupt-OUT transfer (host quirk / fuzzing) would read
  // buffer[0] OOB below, and state_update(slot, buffer + 1, bufsize - 1) would
  // underflow bufsize to 65535 (which is > 47, so state_update's own length
  // check would NOT reject it). Guard both here.
  if (bufsize == 0) return;

  // INTERRUPT OUT
  if (report_id == 0) {
    switch (buffer[0]) {
    case 0x02: {
      state_update(slot, buffer + 1, bufsize - 1);
      // When the headset/speaker is active, output reports for the audio slot
      // normally piggyback on the audio frame path, so we defer (break) here
      // to avoid double-send. But a rumble-bearing SetStateData
      // (UseRumbleNotHaptics flags set) must go out NOW, or rumble lags/drops
      // a frame while audio is streaming. Non-audio slots have no frame to
      // piggyback on and always send immediately.
      // (Ported from upstream awalol/DS5Dongle 07ecbb3, issue #182.)
      bool send_now = ((buffer[1] >> 1) & 1) ||  // UseRumbleNotHaptics
                      ((buffer[39] >> 3) & 1);   // UseRumbleNotHaptics2
      // Piggybacking only works while audio frames are actually flowing:
      // spk_active is host-side stream state, tier_audio_allowed() is our
      // emission gate (false at 2+ pads, including the pre-swap window where
      // the host still streams at a FULL face we've already muted).
      if (!send_now && slot == tier_audio_slot() && spk_active && tier_audio_allowed()) {
        break;
      }
      state_push_slot_to_bt(slot);
      break;
    }
    }
  }
  if (report_id == 0x80 ||
      // DSE: Write Profile Block
      report_id == 0x60 || report_id == 0x62 || report_id == 0x61) {
    set_feature_data(slot, report_id, const_cast<uint8_t *>(buffer), bufsize);
    return;
  }
}

int main() {
#if SYS_CLOCK_KHZ != 150000
  // Overclock path. 200 MHz is stable on RP2350 at the SDK-default 1.10V core
  // voltage (this is RP Pi's own 200 MHz operating point), so we do NOT raise
  // vreg for it -- less heat, less stress. Only a more aggressive overclock
  // (>200 MHz) needs the voltage bump; 1.20V is good well past 300 MHz. The
  // vreg settle delay only matters when we actually changed the voltage.
#if SYS_CLOCK_KHZ > 200000
  vreg_set_voltage(VREG_VOLTAGE_1_20);
  sleep_ms(1000);
#endif
  set_sys_clock_khz(SYS_CLOCK_KHZ, true);
#endif

  board_init();
  // Prepare the /api/log stdio mirror (registered disabled; attached below
  // once config_load() tells us whether the persisted toggle is on).
  weblog_init();
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE,
                                 .speed = TUSB_SPEED_FULL};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  sleep_ms(150);
  tud_disconnect();
  board_init_after_tusb();

  // Bracket cyw43_arch_init() with TLV bank-header dumps: it runs BTstack's
  // setup_tlv(), which formats the link-key bank on a unit that never had one
  // (issue #2). "pre-init INVALID -> post-init valid" on UART proves the
  // format landed; "INVALID" on both is the pre-fix failure signature.
  flash_safety_log_btstack_bank("pre-init");
  if (cyw43_arch_init()) {
    printf("Failed to initialize CYW43\n");
    return 1;
  }
  flash_safety_log_btstack_bank("post-init");

  // Load persisted config from flash BEFORE wifi_net_init(): it reads the
  // stored WiFi credentials (STA vs AP onboarding) and the mDNS hostname, so
  // the saved values must be in place first.
  config_load();

#ifdef ENABLE_OTA
  // OTA boot mode: the web UI armed a watchdog-scratch flag and rebooted.
  // Divert HERE -- after cyw43/config are up but BEFORE BT/audio/USB ever
  // start -- into the stripped updater (same skip-everything pattern as AP
  // onboarding below): heap stays free for TLS, core1 is never launched so
  // flash writes need no lockout, and the radio has no audio to contend
  // with. Never returns; every path ends in a reset. See ota.h.
  if (ota_boot_pending()) {
    ota_mode_main();
  }
  // Normal boot: latch the result of any just-finished OTA attempt so the
  // web UI can report it (/api/ota/status).
  ota_boot_capture_result();
#endif

  // Attach the /api/log stdio mirror per the persisted toggle. Prints between
  // board_init() and here are not captured when enabling -- acceptable: the
  // toggle persists across reboots, so a boot AFTER enabling captures
  // everything from this point (config, WiFi join, BT bring-up, [MEM]).
  weblog_set_enabled(get_config().weblog_enabled != 0);

  // Seed the USB descriptor target with the persisted wake-keyboard toggle
  // BEFORE the first tud_connect() below, so the initial enumeration already
  // carries (or omits) the keyboard -- no cosmetic re-plug right after boot.
  usb_descriptor_init_from_config();

  // Bring up the config web server (no-op with ENABLE_WIFI_WOL off). The SDK's
  // cyw43_arch_init() already brought lwIP up (CYW43_LWIP=1), so
  // wifi_net_init() must NOT re-init it. Diagnostics print to UART0 (GP0 TX,
  // 115200 8N1), not USB.
  wifi_net_init();  // Wi-Fi/WOL transport (ENABLE_WIFI_WOL)

  // Power-On Self Test (POST) LED pattern: 3 rapid flashes to confirm
  // successful CPU overclocking and CYW43 Bluetooth module initialization.
  for (int i = 0; i < 6; i++) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, i % 2 == 0);
    sleep_ms(80);
  }
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

#if ENABLE_BATT_LED
  battery_led_init();
#endif

  if (watchdog_caused_reboot()) {
    printf("Rebooted by Watchdog!\n");
    // 当崩溃重启以后，闪三下灯
    for (int i = 0; i < 6; i++) {
      if (i % 2 == 0) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
      } else {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
      }
      sleep_ms(500);
    }
  } else {
    printf("Clean boot\n");
  }

  {
    extern char __StackLimit[], __bss_end__[];
    printf("[MEM] heap region %d bytes (bss_end %p..stacklimit %p), malloc used %d\n",
           (int) (__StackLimit - __bss_end__), (void *) __bss_end__,
           (void *) __StackLimit, mallinfo().uordblks);
  }

  // Initialize the critical section for the report buffer
  critical_section_init(&report_cs);

  // Seed every slot's input buffer with the neutral idle report so the host
  // sees centered sticks (not zeros) before the first BT report arrives.
  for (int slot = 0; slot < BT_MAX_SLOTS; slot++) {
    memcpy(interrupt_in_data[slot], idle_input_report, sizeof(idle_input_report));
  }

  wake_init();

  // WiFi onboarding (AP + captive portal) is a dedicated setup mode: no
  // controller, no audio. Crucially, BT classic page-scan/inquiry contends with
  // the SoftAP on the single shared CYW43 radio -- with BT up, the AP beacons
  // but never admits a station (observed: stas=0, client loops DHCP forever).
  // So in AP mode we skip BT + audio entirely, handing the radio to the AP (and
  // freeing ~110 KB of heap). Normal STA operation brings BT/audio up as usual.
  const bool ap_onboarding = wifi_net_in_ap_mode();
  if (!ap_onboarding) {
    bt_init();
    bt_register_data_callback(on_bt_data);

    audio_init();
    state_init();
  } else {
    printf("[BOOT] AP onboarding mode: skipping BT + audio (radio handed to SoftAP)\n");
  }

#ifdef ENABLE_WAKE_HID
  // Enumerate immediately as the MINIMAL variant (inert HID placeholder, plus
  // the boot keyboard if the runtime toggle is on), even before any controller
  // connects. tusb_init() left us tud_disconnect()'d; without this the dongle
  // would stay invisible to the host on a cold plug-in until the first
  // controller connection flipped it to FULL -- and the device must be
  // enumerated before the host suspends for USB remote-wakeup to work.
  // active_variant/desired_variant are already MINIMAL.
  tud_connect();
#endif

  watchdog_enable(1000, true);

  // Onboarding loop: a stripped main loop with BT/audio/HID skipped (they were
  // never initialised in AP mode). Pump only the radio/lwIP (cyw43_arch_poll +
  // wifi_net_task drive the SoftAP RX, DHCP/DNS servers, scan, captive portal)
  // plus tud_task to keep USB alive, and feed the watchdog. The device leaves
  // this loop by rebooting into STA mode once the user provisions (wifi_net_task
  // fires the deferred watchdog_reboot).
  if (ap_onboarding) {
    while (1) {
      watchdog_update();
      cyw43_arch_poll();
      tud_task();
      wifi_net_task();
      sleep_us(250);
    }
  }

  while (1) {
    watchdog_update();
    cyw43_arch_poll();
    bt_connection_watchdog_tick();
    bt_blacklist_persist_if_dirty();
    bt_pump();
    tud_task();
    interrupt_loop(true); // early: drain the realtime HID queue only
    wake_task();
#ifdef ENABLE_WAKE_HID
    usb_variant_task();
#endif
    // Service lwIP for the config web server (no-op unless the WiFi transport
    // is built). Cheap; not in the audio hot path. wifi_net_task pumps the
    // WiFi link state + lwIP timers.
    wifi_net_task();
    // Emit the HID input report BEFORE servicing audio. audio_loop() drains the
    // mic-decode FIFO and does a tud_audio_write() that can be large; running it
    // first delayed the input report within each iteration. Prioritizing the
    // report here trims the per-iteration jitter on the 1 kHz polling path (the
    // early interrupt_loop(true) above only drains the realtime queue for mode 2).
    interrupt_loop();
    audio_loop();
    // DSE Edge profile snapshot prefetch/unlock state machine.
    dse_task();
#if ENABLE_BATT_LED
    battery_led_tick();
#endif
    // Yield only when the hot paths are idle; otherwise keep draining USB/BT.
    if (!tud_audio_available() && !bt_send_pending()) {
      sleep_us(250);
    } else {
      tight_loop_contents();
    }
  }
}
