/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 HiFiPhile
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"
#include "audio.h"
#include "config.h"
#include "slots.h"

bool ds_mode() {
    if (get_config().controller_mode == 2) {
        return !is_dse;
    }
    return get_config().controller_mode == 0;
}

enum {
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_AUDIO_STREAMING_OUT,
    ITF_NUM_AUDIO_STREAMING_IN,
    ITF_NUM_HID,
    ITF_NUM_TOTAL, // interfaces in the canonical (kbd-less) FULL variant

    // Header (9) + the canonical DualSense interface run (218). The boot
    // keyboard -- a RUNTIME toggle now (Config_body.wake_kbd_enabled), no longer
    // a compile option -- appends one more interface after the gamepad in FULL
    // (interface ITF_NUM_TOTAL) and after the dummy in MINIMAL (interface 1).
    CONFIG_DESC_LEN_BASE = 0x00E3,
    // Keyboard interface adds 25 bytes: 9 (interface) + 9 (HID class) + 7 (EP IN)
    DS5_KBD_ITF_DESC_LEN = 25,
    // Dummy HID interface (MINIMAL placeholder), same 25-byte shape.
    DS5_DUMMY_ITF_DESC_LEN = 25,
    // Tail gamepad interface (MULTI variant, slots 1..N-1): 9 (interface) +
    // 9 (HID class) + 7 (EP IN) + 7 (EP OUT).
    DS5_GAMEPAD_TAIL_LEN = 32,
};

// String Descriptor Index
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

//--------------------------------------------------------------------+
// Shared descriptor fragments
//
// Reusable byte sequences so the FULL and MINIMAL config descriptors compose
// the SAME bytes for the interfaces they share, instead of being two
// hand-counted blobs that can silently drift.
//--------------------------------------------------------------------+

#ifdef ENABLE_WAKE_HID
// Boot-keyboard interface (HID), parameterised by interface number. EP IN 0x87.
// 25 bytes: 9 (interface) + 9 (HID class) + 7 (EP IN). Compiled into the
// *_kbd descriptor arrays; whether one of those is ever presented to the host
// is the RUNTIME choice (Config_body.wake_kbd_enabled via the web UI). The
// default (kbd off) keeps the enumeration a pure DualSense and wakes over the
// LAN (WOL) instead.
#define DS5_KBD_ITF_DESC(kbd_itf) \
    0x09, 0x04, (kbd_itf), 0x00, 0x01, 0x03, 0x01, 0x01, 0x00, \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x2D, 0x00, \
    0x07, 0x05, 0x87, 0x03, 0x08, 0x00, 0x0A
#endif // ENABLE_WAKE_HID

#if defined(ENABLE_WAKE_HID) && MULTI_SLOT_COUNT > 1
// DualSense gamepad HID interface block for the MULTI variant's tail slots
// (1..N-1), parameterised by interface number and endpoint pair. 32 bytes
// (DS5_GAMEPAD_TAIL_LEN): 9 (interface) + 9 (HID class) + 7 (EP IN) +
// 7 (EP OUT) -- same shape and field order as the canonical gamepad block at
// the end of DS5_FULL_ITFS, which stays hand-written/frozen. wDescriptorLength
// defaults to the DS value (0x0111); it and the EP bIntervals are patched at
// descriptor-fetch time in tud_descriptor_configuration_cb(), whose patch
// loop walks tail blocks by this fixed size -- keep the layout in sync.
#define DS5_GAMEPAD_ITF_DESC(itf, ep_in, ep_out) \
    /* Interface: HID gamepad, 2 endpoints */ \
    0x09, 0x04, (itf), 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, \
    /* HID descriptor: bcdHID 1.11, report descriptor length 0x0111 (DS) */ \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x11, 0x01, \
    /* Endpoint IN: interrupt, 64 bytes, bInterval 1 */ \
    0x07, 0x05, (ep_in), 0x03, 0x40, 0x00, 0x01, \
    /* Endpoint OUT: interrupt, 64 bytes, bInterval 1 */ \
    0x07, 0x05, (ep_out), 0x03, 0x40, 0x00, 0x01
#endif // ENABLE_WAKE_HID && MULTI_SLOT_COUNT > 1

#ifdef ENABLE_WAKE_HID
// Inert dummy HID interface used in MINIMAL where the gamepad sits in FULL.
// Holds HID instance 0 so the keyboard stays HID instance 1 across variants
// (the structural "rogue keyboard on wake" fix). Reuses EP IN 0x84; never
// written. wDescriptorLength 21 = sizeof(desc_hid_report_dummy). 25 bytes.
#define DS5_DUMMY_HID_ITF_DESC(dummy_itf) \
    0x09, 0x04, (dummy_itf), 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, \
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x15, 0x00, \
    0x07, 0x05, 0x84, 0x03, 0x40, 0x00, 0x0A
#endif // ENABLE_WAKE_HID

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t desc_device =
{
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
#ifdef ENABLE_WAKE_HID
    .bcdUSB = 0x0210, // USB 2.1 -- required so the host requests BOS (carries our MS OS 2.0 descriptor)
#else
    .bcdUSB = 0x0200,
#endif

    // Per-interface class (0x00 at device level), matching a real DualSense.
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0x054C,
    // .idProduct = 0x0CE6, // DS
    // .idProduct = 0x0DF2, // DSE
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,

    .bNumConfigurations = 0x01
};

#ifdef ENABLE_WAKE_HID
// Defined further down with the variant orchestrator; forward-declared so the
// device-descriptor callback (above that code) can read the active variant.
bool usb_descriptor_variant_is_full(void);
static uint16_t usb_active_bcd_device(void);
#endif

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
    desc_device.idProduct = ds_mode() ? 0x0CE6 : 0x0DF2;
#ifdef ENABLE_WAKE_HID
    // S3-wake wedge fix. Per Microsoft's USB docs, the
    // Windows hub driver CACHES a device's descriptors keyed on
    // {VID, PID, bcdDevice (device release number)}. On resume from S3 Windows
    // reactivates that CACHED config instead of re-reading -- so when our
    // MINIMAL->FULL swap presents a DIFFERENT descriptor under the SAME
    // VID/PID/bcdDevice it slept with, the host keeps its stale MINIMAL view,
    // never re-reads (HW-confirmed: cfgreads never advanced across the wake),
    // and chokes on the topology change -> mount retry-loop + dead pad.
    //   Fix: give every variant a DIFFERENT bcdDevice value. Now any on-wake
    // bounce between variants looks to Windows like a device with a release
    // number it has NOT cached, so it re-queries the full descriptor set and
    // enumerates cleanly (audio interfaces appear). FULL keeps the canonical
    // 0x0100 (the value Windows binds its DualSense driver against); the inert
    // MINIMAL placeholder carries 0x0101 and the multi-controller MULTI
    // variant 0x0102.
    desc_device.bcdDevice = usb_active_bcd_device();
#endif
    return reinterpret_cast<uint8_t const *>(&desc_device);
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//
// Composed from macro fragments so the SAME byte run backs every variant
// that shares an interface (no hand-counted duplicate blobs that can
// silently drift):
//   DS5_CFG_HDR_DESC       -- 9-byte configuration header (len/#itfs vary)
//   DS5_FULL_ITFS          -- canonical DualSense run: audio(0-2)+gamepad(3)
//   DS5_KBD_ITF_DESC       -- optional boot keyboard (runtime toggle)
//   DS5_DUMMY_HID_ITF_DESC -- MINIMAL's inert placeholder
// Kept as STATIC compile-time arrays, deliberately not runtime-assembled:
// the static_asserts lock every wTotalLength against the emitted bytes, and
// the FULL arrays stay ELF-diffable against a real DualSense descriptor
// dump (Windows descriptor bugs are this repo's most expensive class).
//--------------------------------------------------------------------+

#ifdef ENABLE_WAKE_HID
#define DS5_CFG_BMATTRIBUTES 0xE0 // SELF-POWERED + REMOTE-WAKEUP (needed for wake)
#else
#define DS5_CFG_BMATTRIBUTES 0xC0 // SELF-POWERED, NO REMOTE-WAKEUP
#endif

// 9-byte configuration descriptor header.
#define DS5_CFG_HDR_DESC(total_len, num_itfs) \
    0x09,                     /* bLength */ \
    0x02,                     /* bDescriptorType (CONFIGURATION) */ \
    U16_TO_U8S_LE(total_len), /* wTotalLength */ \
    (num_itfs),               /* bNumInterfaces */ \
    0x01,                     /* bConfigurationValue: 1 */ \
    0x00,                     /* iConfiguration: 0 */ \
    DS5_CFG_BMATTRIBUTES,     /* bmAttributes */ \
    0xFA                      /* bMaxPower: 500mA (250 * 2mA) */

// The canonical DualSense interface run (218 bytes): Audio Control (0),
// Audio Streaming OUT (1), Audio Streaming IN (2), gamepad HID (3). This
// order/layout is FROZEN -- it matches a real DualSense, Windows rejects
// non-ascending interface numbers, and audio-at-0/gamepad-at-3 is what the
// Windows DualSense driver expects.
#define DS5_FULL_ITFS \
    /* --- INTERFACE DESCRIPTOR (0.0): Audio Control --- */ \
    0x09, /* bLength */ \
    0x04, /* bDescriptorType (INTERFACE) */ \
    0x00, /* bInterfaceNumber: 0 */ \
    0x00, /* bAlternateSetting: 0 */ \
    0x00, /* bNumEndpoints: 0 */ \
    0x01, /* bInterfaceClass: Audio (0x01) */ \
    0x01, /* bInterfaceSubClass: Audio Control (0x01) */ \
    0x00, /* bInterfaceProtocol: 0x00 */ \
    0x00, /* iInterface: 0 */ \
    \
    /* Class-specific AC Interface Header Descriptor */ \
    0x0A, /* bLength: 10 */ \
    0x24, /* bDescriptorType: CS_INTERFACE (0x24) */ \
    0x01, /* bDescriptorSubtype: Header (0x01) */ \
    0x00, 0x01, /* bcdADC: 1.00 */ \
    0x49, 0x00, /* wTotalLength: 73 (0x0049) */ \
    0x02, /* bInCollection: 2 streaming interfaces */ \
    0x01, /* baInterfaceNr(1): Interface 1 */ \
    0x02, /* baInterfaceNr(2): Interface 2 */ \
    \
    /* Input Terminal Descriptor (Terminal ID 1: USB Streaming → Output to Speaker) */ \
    0x0C, /* bLength: 12 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x02, /* bDescriptorSubtype: Input Terminal */ \
    0x01, /* bTerminalID: 1 */ \
    0x01, 0x01, /* wTerminalType: USB Streaming (0x0101) */ \
    0x06, /* bAssocTerminal: 6 (paired with USB OUT terminal) */ \
    0x04, /* bNrChannels: 4 */ \
    0x33, 0x00, /* wChannelConfig: L/R Front + L/R Surround (0x0033) */ \
    0x00, /* iChannelNames: 0 */ \
    0x00, /* iTerminal: 0 */ \
    \
    /* Feature Unit Descriptor (Unit ID 2 ← from Terminal 1) */ \
    0x0C, /* bLength: 12 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x06, /* bDescriptorSubtype: Feature Unit */ \
    0x02, /* bUnitID: 2 */ \
    0x01, /* bSourceID: 1 */ \
    0x01, /* bControlSize: 1 byte per control */ \
    0x03, /* bmaControls[0]: Master – Mute, Volume */ \
    0x00, 0x00, 0x00, 0x00, 0x00, /* bmaControls[1..4]: No per-channel controls */ \
    \
    /* Output Terminal Descriptor (Terminal ID 3: Speaker ← from Unit 2) */ \
    0x09, /* bLength: 9 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x03, /* bDescriptorSubtype: Output Terminal */ \
    0x03, /* bTerminalID: 3 */ \
    0x01, 0x03, /* wTerminalType: Speaker (0x0301) */ \
    0x04, /* bAssocTerminal: 4 (paired with mic input) */ \
    0x02, /* bSourceID: 2 (Feature Unit) */ \
    0x00, /* iTerminal: 0 */ \
    \
    /* Input Terminal Descriptor (Terminal ID 4: Headset Mic) */ \
    0x0C, /* bLength: 12 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x02, /* bDescriptorSubtype: Input Terminal */ \
    0x04, /* bTerminalID: 4 */ \
    0x02, 0x04, /* wTerminalType: Headset (0x0402) */ \
    0x03, /* bAssocTerminal: 3 (paired with speaker) */ \
    0x02, /* bNrChannels: 2 */ \
    0x03, 0x00, /* wChannelConfig: L/R Front (0x0003) */ \
    0x00, /* iChannelNames: 0 */ \
    0x00, /* iTerminal: 0 */ \
    \
    /* Feature Unit Descriptor (Unit ID 5 ← from Terminal 4) */ \
    0x09, /* bLength: 9 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x06, /* bDescriptorSubtype: Feature Unit */ \
    0x05, /* bUnitID: 5 */ \
    0x04, /* bSourceID: 4 */ \
    0x01, /* bControlSize: 1 */ \
    0x03, /* bmaControls[0]: Master – Mute, Volume */ \
    0x00, /* bmaControls[1]: Ch1 – no controls */ \
    0x00, /* iFeature: 0 */ \
    \
    /* Output Terminal Descriptor (Terminal ID 6: USB Streaming ← from Unit 5) */ \
    0x09, /* bLength: 9 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x03, /* bDescriptorSubtype: Output Terminal */ \
    0x06, /* bTerminalID: 6 */ \
    0x01, 0x01, /* wTerminalType: USB Streaming (0x0101) */ \
    0x01, /* bAssocTerminal: 1 */ \
    0x05, /* bSourceID: 5 */ \
    0x00, /* iTerminal: 0 */ \
    \
    /* --- INTERFACE DESCRIPTOR (1.0): Audio Streaming (OUT - Alternate 0) --- */ \
    0x09, /* bLength */ \
    0x04, /* bDescriptorType (INTERFACE) */ \
    0x01, /* bInterfaceNumber: 1 */ \
    0x00, /* bAlternateSetting: 0 */ \
    0x00, /* bNumEndpoints: 0 */ \
    0x01, /* bInterfaceClass: Audio */ \
    0x02, /* bInterfaceSubClass: Audio Streaming */ \
    0x00, /* bInterfaceProtocol */ \
    0x00, /* iInterface */ \
    \
    /* --- INTERFACE DESCRIPTOR (1.1): Audio Streaming (OUT - Alternate 1) --- */ \
    0x09, /* bLength */ \
    0x04, /* bDescriptorType (INTERFACE) */ \
    0x01, /* bInterfaceNumber: 1 */ \
    0x01, /* bAlternateSetting: 1 */ \
    0x01, /* bNumEndpoints: 1 */ \
    0x01, /* bInterfaceClass: Audio */ \
    0x02, /* bInterfaceSubClass: Audio Streaming */ \
    0x00, /* bInterfaceProtocol */ \
    0x00, /* iInterface */ \
    \
    /* AS General Descriptor (for Interface 1.1) */ \
    0x07, /* bLength: 7 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x01, /* bDescriptorSubtype: AS_GENERAL */ \
    0x01, /* bTerminalLink: connected to Terminal ID 1 */ \
    0x01, /* bDelay: 1 frame */ \
    0x01, 0x00, /* wFormatTag: PCM (0x0001) */ \
    \
    /* Format Type Descriptor (4-channel, 16-bit, 48kHz) */ \
    0x0B, /* bLength: 11 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x02, /* bDescriptorSubtype: FORMAT_TYPE */ \
    0x01, /* bFormatType: TYPE_I */ \
    0x04, /* bNrChannels: 4 */ \
    0x02, /* bSubframeSize: 2 bytes/sample */ \
    0x10, /* bBitResolution: 16 bits */ \
    0x01, /* bSamFreqType: 1 discrete frequency */ \
    0x80, 0xBB, 0x00, /* tSamFreq: 48000 Hz (0x00BB80) */ \
    \
    /* Endpoint Descriptor (Audio OUT: EP1) */ \
    0x09, /* bLength */ \
    0x05, /* bDescriptorType (ENDPOINT) */ \
    0x01, /* bEndpointAddress: OUT EP1 */ \
    0x09, /* bmAttributes: Isochronous, Adaptive */ \
    0x88, 0x01, /* wMaxPacketSize: 392 bytes */ \
    0x01, /* bInterval: 1 */ \
    0x00, /* bRefresh */ \
    0x00, /* bSynchAddress */ \
    \
    /* Class-specific Audio Streaming Endpoint Descriptor (EP1) */ \
    0x07, /* bLength */ \
    0x25, /* bDescriptorType: CS_ENDPOINT */ \
    0x01, /* bDescriptorSubtype: GENERAL */ \
    0x00, /* Attributes: No pitch/sampling freq control */ \
    0x00, /* Lock Delay Units: Undefined */ \
    0x00, 0x00, /* Lock Delay: 0 */ \
    \
    /* --- INTERFACE DESCRIPTOR (2.0): Audio Streaming IN (Alternate 0) --- */ \
    0x09, /* bLength */ \
    0x04, /* bDescriptorType (INTERFACE) */ \
    0x02, /* bInterfaceNumber: 2 */ \
    0x00, /* bAlternateSetting: 0 */ \
    0x00, /* bNumEndpoints: 0 */ \
    0x01, /* bInterfaceClass: Audio */ \
    0x02, /* bInterfaceSubClass: Audio Streaming */ \
    0x00, /* bInterfaceProtocol */ \
    0x00, /* iInterface */ \
    \
    /* --- INTERFACE DESCRIPTOR (2.1): Audio Streaming IN (Alternate 1) --- */ \
    0x09, /* bLength */ \
    0x04, /* bDescriptorType (INTERFACE) */ \
    0x02, /* bInterfaceNumber: 2 */ \
    0x01, /* bAlternateSetting: 1 */ \
    0x01, /* bNumEndpoints: 1 */ \
    0x01, /* bInterfaceClass: Audio */ \
    0x02, /* bInterfaceSubClass: Audio Streaming */ \
    0x00, /* bInterfaceProtocol */ \
    0x00, /* iInterface */ \
    \
    /* AS General Descriptor (for Interface 2.1) */ \
    0x07, /* bLength: 7 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x01, /* bDescriptorSubtype: AS_GENERAL */ \
    0x06, /* bTerminalLink: connected to Terminal ID 6 */ \
    0x01, /* bDelay: 1 frame */ \
    0x01, 0x00, /* wFormatTag: PCM (0x0001) */ \
    \
    /* Format Type Descriptor (2-channel, 16-bit, 48kHz) */ \
    0x0B, /* bLength: 11 */ \
    0x24, /* bDescriptorType: CS_INTERFACE */ \
    0x02, /* bDescriptorSubtype: FORMAT_TYPE */ \
    0x01, /* bFormatType: TYPE_I */ \
    0x02, /* bNrChannels: 2 */ \
    0x02, /* bSubframeSize: 2 */ \
    0x10, /* bBitResolution: 16 */ \
    0x01, /* bSamFreqType: 1 */ \
    0x80, 0xBB, 0x00, /* tSamFreq: 48000 Hz */ \
    \
    /* Endpoint Descriptor (Audio IN: EP2) */ \
    0x09, /* bLength */ \
    0x05, /* bDescriptorType (ENDPOINT) */ \
    0x82, /* bEndpointAddress: IN EP2 */ \
    0x05, /* bmAttributes: Isochronous, Asynchronous */ \
    0xC4, 0x00, /* wMaxPacketSize: 196 bytes (48kHz × 2ch × 2B) */ \
    0x01, /* bInterval: 1 */ \
    0x00, /* bRefresh */ \
    0x00, /* bSynchAddress */ \
    \
    /* Class-specific Audio Streaming Endpoint Descriptor (EP2) */ \
    0x07, /* bLength */ \
    0x25, /* bDescriptorType: CS_ENDPOINT */ \
    0x01, /* bDescriptorSubtype: GENERAL */ \
    0x00, /* Attributes: No controls */ \
    0x00, /* Lock Delay Units */ \
    0x00, 0x00, /* Lock Delay */ \
    \
    /* --- INTERFACE DESCRIPTOR (3.0): HID (DualSense 5 Gamepad + Touchpad) --- */ \
    0x09, /* bLength */ \
    0x04, /* bDescriptorType (INTERFACE) */ \
    0x03, /* bInterfaceNumber: 3 */ \
    0x00, /* bAlternateSetting: 0 */ \
    0x02, /* bNumEndpoints: 2 (IN + OUT) */ \
    0x03, /* bInterfaceClass: HID */ \
    0x00, /* bInterfaceSubClass: None */ \
    0x00, /* bInterfaceProtocol: None */ \
    0x00, /* iInterface */ \
    \
    /* HID Descriptor */ \
    0x09, /* bLength: 9 */ \
    0x21, /* bDescriptorType (HID) */ \
    0x11, 0x01, /* bcdHID: 1.11 */ \
    0x00, /* bCountryCode: Not localized */ \
    0x01, /* bNumDescriptors: 1 report descriptor */ \
    0x22, /* bDescriptorType: Report */ \
    0x11, 0x01, /* wDescriptorLength: 273 (0x0111) DS */ \
    /* 0x85, 0x01, // wDescriptorLength: 389 (0x0185) DSE */ \
    \
    /* Endpoint Descriptor (HID IN: EP4) */ \
    0x07, /* bLength */ \
    0x05, /* bDescriptorType (ENDPOINT) */ \
    0x84, /* bEndpointAddress: IN EP4 */ \
    0x03, /* bmAttributes: Interrupt */ \
    0x40, 0x00, /* wMaxPacketSize: 64 */ \
    0x01, /* bInterval: 1 (polling every 4ms -> 1ms) */ \
    \
    /* Endpoint Descriptor (HID OUT: EP3) */ \
    0x07, /* bLength */ \
    0x05, /* bDescriptorType (ENDPOINT) */ \
    0x03, /* bEndpointAddress: OUT EP3 */ \
    0x03, /* bmAttributes: Interrupt */ \
    0x40, 0x00, /* wMaxPacketSize: 64 */ \
    0x01  /* bInterval: 1 (polling every 4ms -> 1ms) */

// FULL, canonical: audio + gamepad only -- THE pure-DualSense enumeration
// (default, wake_kbd_enabled=0). Non-const: tud_descriptor_configuration_cb
// patches the gamepad's polling bInterval + report-descriptor length in
// place before serving.
uint8_t descriptor_configuration[] = {
    DS5_CFG_HDR_DESC(CONFIG_DESC_LEN_BASE, ITF_NUM_TOTAL),
    DS5_FULL_ITFS,
};

// Lock the hand-computed wTotalLength against the actual emitted bytes. A
// mismatch would silently break enumeration of the trailing interfaces.
static_assert(sizeof(descriptor_configuration) == CONFIG_DESC_LEN_BASE,
              "descriptor_configuration size != CONFIG_DESC_LEN_BASE");

#ifdef ENABLE_WAKE_HID
// FULL + boot keyboard (runtime wake_kbd_enabled=1): the same canonical run
// with the kbd appended as interface 4 -> HID instance 1. The gamepad stays
// interface 3 / HID instance 0, byte-identical to the kbd-less FULL.
uint8_t descriptor_configuration_full_kbd[] = {
    DS5_CFG_HDR_DESC(CONFIG_DESC_LEN_BASE + DS5_KBD_ITF_DESC_LEN,
                     ITF_NUM_TOTAL + 1),
    DS5_FULL_ITFS,
    DS5_KBD_ITF_DESC(ITF_NUM_TOTAL),
};
static_assert(sizeof(descriptor_configuration_full_kbd) ==
                  CONFIG_DESC_LEN_BASE + DS5_KBD_ITF_DESC_LEN,
              "descriptor_configuration_full_kbd size mismatch");
#endif // ENABLE_WAKE_HID

#ifdef ENABLE_WAKE_HID
// Minimal config descriptors used when no DualSense is connected.
// Present no audio function and no real gamepad — so the Windows Sound applet
// and joy.cpl don't show ghost devices — while keeping the dongle enumerated
// and remote-wakeup-capable (needed for wake-from-S3/S5).
//
// The kbd-bearing pair keeps the keyboard at HID instance 1 in BOTH variants.
// TinyUSB numbers HID instances by descriptor parse order, counting only HID
// interfaces; in FULL the gamepad is HID instance 0 and the kbd is instance 1.
// MINIMAL therefore needs exactly ONE HID before the kbd (a dummy placeholder)
// so the kbd stays instance 1 -- without that the kbd became instance 0 in
// MINIMAL and a gamepad report (addressed to instance 0/1) could land on it
// across a swap: the "rogue keyboard on wake". For the same reason the kbd
// must be in BOTH variants of a running pair or NEITHER -- the orchestrator
// swaps variant and kbd as one atomic target (below). The dummy reuses the
// gamepad's IN endpoint 0x84; it is never written to.
//
// Interface layout with the keyboard enabled (wake_kbd_enabled=1):
//       0   dummy HID            (HID instance 0)
//       1   boot keyboard        (HID instance 1)
// Without it (default, pure-DualSense face): a single dummy HID -- no audio,
// no gamepad, but the dongle stays enumerated (some hosts dislike a device
// that enumerates an empty config) and HID instance numbering stays
// consistent across the swap (FULL gamepad = instance 0, MINIMAL dummy =
// instance 0). The alternative (fully un-enumerated) is a HW-tunable fallback
// if a host mishandles the swap-to-FULL.
//
// FULL's interface order is canonical/frozen (matches a real DualSense); the
// dummy lives entirely in MINIMAL. Interface numbers stay ascending, so Windows
// accepts the config (it rejects out-of-order interfaces).
#define CONFIG_DESC_LEN_MINIMAL (9 + DS5_DUMMY_ITF_DESC_LEN)
uint8_t descriptor_configuration_minimal[] = {
    DS5_CFG_HDR_DESC(CONFIG_DESC_LEN_MINIMAL, 1),
    // Interface 0: dummy HID (HID instance 0; mirrors FULL's gamepad slot).
    DS5_DUMMY_HID_ITF_DESC(0),
};
static_assert(sizeof(descriptor_configuration_minimal) == CONFIG_DESC_LEN_MINIMAL,
              "descriptor_configuration_minimal size mismatch");

#define CONFIG_DESC_LEN_MINIMAL_KBD \
    (9 + DS5_DUMMY_ITF_DESC_LEN + DS5_KBD_ITF_DESC_LEN)
uint8_t descriptor_configuration_minimal_kbd[] = {
    DS5_CFG_HDR_DESC(CONFIG_DESC_LEN_MINIMAL_KBD, 2),
    // Interface 0: dummy HID (HID instance 0).
    DS5_DUMMY_HID_ITF_DESC(0),
    // Interface 1: boot keyboard (HID instance 1).
    DS5_KBD_ITF_DESC(1),
};
static_assert(sizeof(descriptor_configuration_minimal_kbd) == CONFIG_DESC_LEN_MINIMAL_KBD,
              "descriptor_configuration_minimal_kbd size mismatch");

#if MULTI_SLOT_COUNT > 1
// MULTI config descriptors: one gamepad HID interface per slot, NO audio
// function. Entered while 2+ controllers are connected; see
// bt_apply_usb_variant_policy() in bt.cpp. Audio is deliberately ABSENT: the
// BT airtime can't carry even one pad's audio/HD-haptics stream alongside a
// second pad (see tier.h), and a silent-but-enumerated audio device confused
// hosts/users -- so at 2+ pads the host sees pure gamepads, and returning to
// one pad bounces back to FULL (audio reappears; one deliberate re-plug).
// Every gamepad block is macro-emitted (32 bytes, DS5_GAMEPAD_ITF_DESC) --
// slot 0 keeps its FULL endpoints 0x84/0x03; tails use 0x88/0x08, 0x89/0x09,
// 0x8A/0x0A. Interface numbers stay ascending (Windows rejects out-of-order
// interfaces); with the keyboard enabled it sits at interface 1 so it stays
// HID instance 1 (parse order: slot0, kbd, slot1..3 -- the same slot<->
// instance map as FULL+kbd).
#define CONFIG_DESC_LEN_MULTI (9 + MULTI_SLOT_COUNT * DS5_GAMEPAD_TAIL_LEN)
uint8_t descriptor_configuration_multi[] = {
    DS5_CFG_HDR_DESC(CONFIG_DESC_LEN_MULTI, MULTI_SLOT_COUNT),
    DS5_GAMEPAD_ITF_DESC(0, 0x84, 0x03), // slot 0
#if MULTI_SLOT_COUNT >= 2
    DS5_GAMEPAD_ITF_DESC(1, 0x88, 0x08), // slot 1
#endif
#if MULTI_SLOT_COUNT >= 3
    DS5_GAMEPAD_ITF_DESC(2, 0x89, 0x09), // slot 2
#endif
#if MULTI_SLOT_COUNT >= 4
    DS5_GAMEPAD_ITF_DESC(3, 0x8A, 0x0A), // slot 3
#endif
};
static_assert(sizeof(descriptor_configuration_multi) == CONFIG_DESC_LEN_MULTI,
              "descriptor_configuration_multi size mismatch");

#define CONFIG_DESC_LEN_MULTI_KBD \
    (CONFIG_DESC_LEN_MULTI + DS5_KBD_ITF_DESC_LEN)
uint8_t descriptor_configuration_multi_kbd[] = {
    DS5_CFG_HDR_DESC(CONFIG_DESC_LEN_MULTI_KBD, MULTI_SLOT_COUNT + 1),
    DS5_GAMEPAD_ITF_DESC(0, 0x84, 0x03), // slot 0 (HID instance 0)
    DS5_KBD_ITF_DESC(1),                 // keyboard (HID instance 1)
#if MULTI_SLOT_COUNT >= 2
    DS5_GAMEPAD_ITF_DESC(2, 0x88, 0x08), // slot 1 (HID instance 2)
#endif
#if MULTI_SLOT_COUNT >= 3
    DS5_GAMEPAD_ITF_DESC(3, 0x89, 0x09), // slot 2 (HID instance 3)
#endif
#if MULTI_SLOT_COUNT >= 4
    DS5_GAMEPAD_ITF_DESC(4, 0x8A, 0x0A), // slot 3 (HID instance 4)
#endif
};
static_assert(sizeof(descriptor_configuration_multi_kbd) == CONFIG_DESC_LEN_MULTI_KBD,
              "descriptor_configuration_multi_kbd size mismatch");
#endif // MULTI_SLOT_COUNT > 1

// Runtime selector: which descriptor to present on the next GET_CONFIGURATION.
// ONE atomic target -- the variant (controller connected or not) AND whether
// the boot keyboard rides along (web-UI wake toggle) -- so any difference
// between desired and active drives the same disconnect/settle/swap/connect
// bounce, and the kbd can never be present in one variant of a running pair
// but not the other. `active` is latched by the orchestrator at swap time and
// is the ONLY thing the descriptor callbacks read; live config is never
// consulted mid-enumeration.
// Order matters: the suspend-nudge in usb_variant_task() treats a HIGHER
// desired variant as an UP-swap ("more device just appeared") that may resume
// the bus; DOWN-swaps never nudge (they can legitimately pend across S3).
typedef enum {
    DESC_VARIANT_MINIMAL = 0, // no controller: dummy HID (+ kbd if enabled)
    DESC_VARIANT_FULL,        // one controller: audio + gamepad (+ kbd)
    DESC_VARIANT_MULTI,       // 2+ controllers: N gamepads, NO audio (+ kbd)
} desc_variant_t;
typedef struct {
    desc_variant_t variant;
    bool kbd;
    // MULTI only: how many gamepad interfaces to expose (2..MULTI_SLOT_COUNT,
    // the session's high-water controller count -- see the policy in bt.cpp).
    // 0 for the other variants so the plain field compare in
    // desc_target_differs() can't see a stale count.
    uint8_t multi_slots;
} usb_desc_target;
// Field-wise volatile access is enough: all fields are only written from
// main-loop context (BT event handlers, httpd POST handlers, usb_variant_task).
static volatile usb_desc_target active_target  = {DESC_VARIANT_MINIMAL, false, 0};
static volatile usb_desc_target desired_target = {DESC_VARIANT_MINIMAL, false, 0};

bool usb_descriptor_variant_is_full(void) {
    return active_target.variant == DESC_VARIANT_FULL;
}
// Windows caches descriptors keyed on {VID, PID, bcdDevice}; every variant
// gets a distinct value so no swap can be answered from a stale cache (see
// tud_descriptor_device_cb).
static uint16_t usb_active_bcd_device(void) {
    switch (active_target.variant) {
        case DESC_VARIANT_MINIMAL: return 0x0101;
        // Each exposure count is its own cacheable identity (0x0102 for two
        // pads .. 0x0104 for four): a grow bounce (say MULTI-2 -> MULTI-3)
        // changes the topology just like MINIMAL->FULL does, so it needs the
        // same cache-buster.
        case DESC_VARIANT_MULTI:   return (uint16_t)(0x0100 + active_target.multi_slots);
        case DESC_VARIANT_FULL:
        default:                   return 0x0100;
    }
}
// The boot keyboard is HID instance 1 in EVERY variant: in FULL/MULTI the
// slot-0 gamepad is instance 0 (its interface is parsed first), and MINIMAL
// keeps a dummy HID at instance 0 so the kbd stays instance 1. Stable across
// variant swaps -- this is what makes the "rogue keyboard on wake"
// structurally impossible.
uint8_t usb_kbd_hid_instance(void) { return 1; }
// True while the ENUMERATED configuration carries the boot keyboard. This is
// the gate for all kbd runtime behavior (F15 FSM, HID callback routing) -- NOT
// the config value, which may already differ while a swap is still pending.
bool usb_wake_kbd_active(void) { return active_target.kbd; }

// How many gamepad interfaces the HOST currently sees (per the LATCHED active
// variant): MULTI exposes the latched high-water count, FULL exactly one,
// MINIMAL none.
uint8_t usb_active_gamepad_slots(void) {
    switch (active_target.variant) {
#if MULTI_SLOT_COUNT > 1
        case DESC_VARIANT_MULTI:   return active_target.multi_slots;
#endif
        case DESC_VARIANT_FULL:    return 1;
        case DESC_VARIANT_MINIMAL:
        default:                   return 0;
    }
}

// Slot <-> HID-instance map. TinyUSB numbers HID instances by descriptor
// parse order, counting only HID interfaces. The keyboard, WHEN ACTIVE, is
// pinned at instance 1 (see usb_kbd_hid_instance); tail gamepads therefore
// shift by one depending on the RUNTIME kbd state -- unlike a compile-time
// map, this must read the latched active_target:
//   kbd on : slot0=inst0, kbd=inst1, slot1..3 = inst2..4
//   kbd off: slot0=inst0,            slot1..3 = inst1..3
uint8_t usb_slot_hid_instance(uint8_t slot) {
    if (slot == 0) return 0;
    return active_target.kbd ? (uint8_t)(slot + 1) : slot;
}
// Inverse map: -1 for the keyboard's instance (when active) and for
// out-of-range instances. Callers must additionally bound the result against
// usb_active_gamepad_slots() (an instance can exist in CFG_TUD_HID without
// being enumerated by the active variant).
int usb_hid_instance_slot(uint8_t instance) {
    if (instance == 0) {
        // Instance 0 is the slot-0 gamepad in FULL/MULTI but the inert dummy
        // in MINIMAL; the exposure bound (usb_active_gamepad_slots()==0)
        // handles the MINIMAL case at the caller.
        return 0;
    }
    if (active_target.kbd) {
        if (instance == 1) return -1; // the keyboard
        return (int) instance - 1;
    }
    return (int) instance;
}

//--------------------------------------------------------------------+
// Variant swap orchestrator
//--------------------------------------------------------------------+
// State machine that drives a USB re-enumeration when the desired
// descriptor variant differs from the active one. Runs from the main
// loop via usb_variant_task().
//
// Sequence:
//   IDLE     — desired == active, nothing to do.
//   DISCONNECTING — called tud_disconnect(); wait SETTLE_US so the host
//                   sees the disconnect cleanly before we present a
//                   different descriptor.
//   CONNECTING    — latched active_target = desired_target, called
//                   tud_connect(); wait for host re-enumeration to settle,
//                   then back to IDLE.
//
// Refuses to start or continue a swap while the host is suspended: a
// re-enumeration mid-suspend defeats the whole point of ENABLE_WAKE_HID
// (the dongle needs to be enumerated when the host wakes so remote-
// wakeup can fire). A kbd toggle requested during suspend therefore stays
// pending in desired_target and applies after resume.

#include "pico/time.h"
#include "wake.h"

static volatile bool host_suspended_flag = false;

typedef enum {
    SWAP_IDLE,
    SWAP_DISCONNECTING,
    SWAP_CONNECTING,
} swap_state_t;

static swap_state_t  swap_state         = SWAP_IDLE;
static uint64_t      swap_state_entered = 0;
static constexpr uint64_t SWAP_DISCONNECT_SETTLE_US = 500000;  // 500 ms
static constexpr uint64_t SWAP_CONNECT_SETTLE_US    = 1500000; // 1500 ms

// Defensive serialization against the HOST's own re-enumeration on wake:
// avoid starting our tud_disconnect() while the host is mid re-enumeration
// (the full re-mount many hosts perform on wake from S3), so two USB
// reconfiguration cycles don't interleave. (This is hygiene, NOT the wedge
// fix -- the actual S3-wake wedge was the host reusing its cached descriptor
// across the MINIMAL->FULL swap; that is solved by the distinct bcdDevice per
// variant in tud_descriptor_device_cb.) A swap may only START out of IDLE when
//   (a) we are CONFIGURED (tud_mounted(): a host bus reset clears it until
//       SET_CONFIGURATION arrives, so this alone keeps the bounce out of the
//       middle of any enumeration), and
//   (b) no resume/mount event landed within the last SWAP_HOST_SETTLE_MS --
//       each event re-stamps the window, so a wake that resolves as
//       resume-then-re-enumerate keeps pushing the swap out until the host is
//       actually done.
// The stamp is a single 32-bit ms word (written by usb_set_host_suspended(false)
// from the TinyUSB resume/mount callbacks, read here) so it can't tear.
static constexpr uint32_t SWAP_HOST_SETTLE_MS = 1000;
static volatile uint32_t last_bus_up_ms = 0;

void usb_request_variant_full(void) {
    desired_target.variant = DESC_VARIANT_FULL;
    desired_target.multi_slots = 0;
}
void usb_request_variant_minimal(void) {
    desired_target.variant = DESC_VARIANT_MINIMAL;
    desired_target.multi_slots = 0;
}
#if MULTI_SLOT_COUNT > 1
void usb_request_variant_multi(uint8_t exposed_slots) {
    if (exposed_slots < 2) exposed_slots = 2;
    if (exposed_slots > MULTI_SLOT_COUNT) exposed_slots = MULTI_SLOT_COUNT;
    desired_target.variant = DESC_VARIANT_MULTI;
    desired_target.multi_slots = exposed_slots;
}
#endif
// Web-UI wake-keyboard toggle: ask for the kbd to (dis)appear. Applied live by
// usb_variant_task() through the same bounce a variant change uses; a no-op if
// the enumerated state already matches.
void usb_request_wake_kbd(bool enabled) { desired_target.kbd = enabled; }
// One-time boot init, called after config_load() and BEFORE the first
// tud_connect(): seed BOTH desired and active with the persisted kbd choice so
// the very first enumeration already matches the config (no cosmetic bounce a
// few seconds after boot).
void usb_descriptor_init_from_config(void) {
    const bool kbd = get_config().wake_kbd_enabled != 0;
    desired_target.kbd = kbd;
    active_target.kbd  = kbd;
}
void usb_set_host_suspended(bool s) {
    // Bus coming (back) up -- resume or mount. Re-stamp the host-settle
    // window so usb_variant_task defers any pending swap (gate (b) above).
    if (!s) last_bus_up_ms = (uint32_t)(time_us_64() / 1000);
    host_suspended_flag = s;
}
bool usb_host_suspended(void)          { return host_suspended_flag; }
bool usb_variant_swap_in_progress(void) { return swap_state != SWAP_IDLE; }

static bool desc_target_differs(void) {
    return desired_target.variant != active_target.variant ||
           desired_target.kbd != active_target.kbd ||
           desired_target.multi_slots != active_target.multi_slots;
}

// Cold-boot autosuspend recovery (issue #4). While the gate below holds a
// pending UP-swap (MINIMAL->FULL) shut, periodically re-issue a USB bus resume
// to coax the host into re-mounting us (which fires tud_resume_cb/tud_mount_cb
// -> clears the gate). Rate-limited so we don't spam resume signaling.
//
// CRITICAL: only the MINIMAL->FULL direction is nudged. The DOWN-swap
// (FULL->MINIMAL, requested when the controller disconnects) can legitimately
// be pending while the host is in a genuine S3 suspend -- a DS5 that powers
// itself off after the host sleeps leaves desired=MINIMAL, active=FULL. Forcing
// a resume there would wake the sleeping host, the exact thing the gate exists
// to prevent. The UP-swap only ever happens right after a controller connects,
// which is precisely when we DO want the bus back up so the gamepad appears.
static constexpr uint64_t SWAP_GATE_RESUME_RETRY_US = 1000000; // 1 s
static uint64_t swap_gate_last_resume_us = 0;

void usb_variant_task(void) {
    if (host_suspended_flag) {
        // Never re-enumerate during host suspend. But if an UP-swap (a pad
        // just connected/joined: MINIMAL->FULL/MULTI, FULL->MULTI, or a MULTI
        // exposure grow) is pending and the host has us suspended, keep
        // nudging the bus back up so the gate can clear -- otherwise a host
        // that suspends MINIMAL and never re-mounts strands the controller in
        // MINIMAL forever (issue #4). DOWN-swaps never nudge: they can
        // legitimately pend across a genuine S3 (pads powered off after the
        // host slept) and a resume there would wake the sleeping host.
        if (desired_target.variant > active_target.variant ||
            desired_target.multi_slots > active_target.multi_slots) {
            const uint64_t now = time_us_64();
            if (now - swap_gate_last_resume_us >= SWAP_GATE_RESUME_RETRY_US) {
                swap_gate_last_resume_us = now;
                wake_request_bus_resume();
            }
        }
        return;
    }
    const uint64_t now = time_us_64();
    switch (swap_state) {
        case SWAP_IDLE:
            if (desc_target_differs()) {
                // Serialize behind the host's own (re-)enumeration -- see the
                // SWAP_HOST_SETTLE_MS comment above. Unconfigured means the
                // host is between its bus reset and SET_CONFIGURATION right
                // now; a fresh resume/mount means it may be about to start
                // one (S3-wake re-mount). Bouncing the bus in either window
                // wedges the gamepad IN endpoint.
                if (!tud_mounted()) return;
                if ((uint32_t)(now / 1000) - last_bus_up_ms < SWAP_HOST_SETTLE_MS) return;
                wake_reset_for_variant_swap();
                tud_disconnect();
                swap_state = SWAP_DISCONNECTING;
                swap_state_entered = now;
            }
            return;
        case SWAP_DISCONNECTING:
            if (now - swap_state_entered < SWAP_DISCONNECT_SETTLE_US) return;
            // Latch the whole target atomically w.r.t. enumeration: the bus is
            // down, so the descriptor callbacks can't observe a half-updated
            // target. The variants enumerate under DISTINCT bcdDevice values
            // (see tud_descriptor_device_cb) so the host's descriptor cache --
            // keyed on VID/PID/bcdDevice -- can't survive this swap: on the
            // next connect it must re-read, which is what makes the on-wake
            // MINIMAL->FULL swap enumerate cleanly instead of reusing a stale
            // cached MINIMAL.
            active_target.variant     = desired_target.variant;
            active_target.kbd         = desired_target.kbd;
            active_target.multi_slots = desired_target.multi_slots;
            // Audio alt-setting state resets with the bus: variants without an
            // audio function (MINIMAL/MULTI) never receive the SET_INTERFACE
            // that would clear these, and a stale spk_active=true wedges the
            // output-report piggyback path in main.cpp (reports deferred to an
            // audio frame that never flows). The host re-opens the streams
            // after enumerating an audio-bearing variant.
            {
                extern bool spk_active; // defined in main.cpp
                spk_active = false;
                audio_set_mic_active(false);
            }
            tud_connect();
            swap_state = SWAP_CONNECTING;
            swap_state_entered = now;
            return;
        case SWAP_CONNECTING:
            if (now - swap_state_entered < SWAP_CONNECT_SETTLE_US) return;
            swap_state = SWAP_IDLE;
            return;
    }
}
#endif // ENABLE_WAKE_HID

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; // for multiple configurations
#ifdef ENABLE_WAKE_HID
    // Reads ONLY the latched active_target (set at swap time); never the live
    // config, which could change mid-enumeration.
    if (active_target.variant == DESC_VARIANT_MINIMAL) {
        return active_target.kbd ? descriptor_configuration_minimal_kbd
                                 : descriptor_configuration_minimal;
    }
    uint8_t *desc_full;
#if MULTI_SLOT_COUNT > 1
    if (active_target.variant == DESC_VARIANT_MULTI) {
        desc_full = active_target.kbd ? descriptor_configuration_multi_kbd
                                      : descriptor_configuration_multi;
    } else
#endif
    {
        desc_full = active_target.kbd ? descriptor_configuration_full_kbd
                                      : descriptor_configuration;
    }
#else
    uint8_t *desc_full = descriptor_configuration;
#endif
    auto bInterval = 0x01;
    switch (get_config().polling_rate_mode) {
        case 0:
            bInterval = 0x04;
            break;
        case 1:
            bInterval = 0x02;
            break;
        case 2:
            bInterval = 0x01;
            break;
    }
    const uint8_t report_len_lo = ds_mode()
        ? 0x11  // DS report desc low byte (0x0111 = 273)
        : 0x85; // DSE report desc low byte (0x0185 = 389)
    // Per 32-byte gamepad block, counting from its END:
    //   end-1  = EP OUT bInterval, end-8 = EP IN bInterval,
    //   end-16 = HID wDescriptorLength low byte.
    // (The canonical FULL block and the DS5_GAMEPAD_ITF_DESC macro share this
    // exact layout.)
#if defined(ENABLE_WAKE_HID) && MULTI_SLOT_COUNT > 1
    if (active_target.variant == DESC_VARIANT_MULTI) {
        // MULTI: header(9) + uniform 32-byte gamepad blocks, with the 25-byte
        // keyboard interface (when enumerated) inserted after slot 0's block.
        // The static array carries every compiled slot; TRUNCATE the served
        // portion to the latched high-water exposure (multi_slots) by
        // patching the header's wTotalLength + bNumInterfaces -- TinyUSB
        // serves exactly wTotalLength bytes, so trailing blocks simply don't
        // exist for the host. Ghost pads that were never connected this
        // session are thus never enumerated.
        const uint8_t exposed = active_target.multi_slots;
        const uint16_t kbd_len = active_target.kbd ? DS5_KBD_ITF_DESC_LEN : 0;
        const uint16_t total = 9 + (uint16_t)(exposed * DS5_GAMEPAD_TAIL_LEN) + kbd_len;
        desc_full[2] = (uint8_t)(total & 0xFF);        // wTotalLength lo
        desc_full[3] = (uint8_t)(total >> 8);          // wTotalLength hi
        desc_full[4] = (uint8_t)(exposed + (active_target.kbd ? 1 : 0)); // bNumInterfaces
        for (int k = 0; k < exposed; k++) {
            const uint16_t kbd_off =
                (active_target.kbd && k >= 1) ? DS5_KBD_ITF_DESC_LEN : 0;
            const uint16_t end = 9 + (uint16_t)((k + 1) * DS5_GAMEPAD_TAIL_LEN)
                                 + kbd_off;
            desc_full[end - 1] = bInterval;
            desc_full[end - 8] = bInterval;
            desc_full[end - 16] = report_len_lo;
        }
        return desc_full;
    }
#endif
    // FULL: patch offsets are relative to the canonical run (header + 218
    // bytes); they land on the same bytes in both FULL arrays because the kbd
    // is appended strictly AFTER the gamepad interface.
    constexpr auto offset = CONFIG_DESC_LEN_BASE;
    desc_full[offset - 1] = bInterval;
    desc_full[offset - 8] = bInterval;
    desc_full[offset - 16] = report_len_lo;
    return desc_full;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report_ds[] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05, // Usage (Game Pad)
    0xA1, 0x01, // Collection (Application)
    0x85, 0x01, //   Report ID (1)
    0x09, 0x30, //   Usage (X)
    0x09, 0x31, //   Usage (Y)
    0x09, 0x32, //   Usage (Z)
    0x09, 0x35, //   Usage (Rz)
    0x09, 0x33, //   Usage (Rx)
    0x09, 0x34, //   Usage (Ry)
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x06, //   Report Count (6)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20, //   Usage (0x20)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39, //   Usage (Hat switch)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x07, //   Logical Maximum (7)
    0x35, 0x00, //   Physical Minimum (0)
    0x46, 0x3B, 0x01, //   Physical Maximum (315)
    0x65, 0x14, //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04, //   Report Size (4)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x42, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00, //   Unit (None)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (0x01)
    0x29, 0x0F, //   Usage Maximum (0x0F)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x0F, //   Report Count (15)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x21, //   Usage (0x21)
    0x95, 0x0D, //   Report Count (13)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x22, //   Usage (0x22)
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x34, //   Report Count (52)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x02, //   Report ID (2)
    0x09, 0x23, //   Usage (0x23)
    0x95, 0x2F, //   Report Count (47)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x05, //   Report ID (5)
    0x09, 0x33, //   Usage (0x33)
    0x95, 0x28, //   Report Count (40)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x08, //   Report ID (8)
    0x09, 0x34, //   Usage (0x34)
    0x95, 0x2F, //   Report Count (47)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x09, //   Report ID (9)
    0x09, 0x24, //   Usage (0x24)
    0x95, 0x13, //   Report Count (19)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x0A, //   Report ID (10)
    0x09, 0x25, //   Usage (0x25)
    0x95, 0x1A, //   Report Count (26)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    // Report IDs 11/12 (usages 0x41/0x42, 41-byte feature reports) removed: a
    // genuine DualSense / DualSense Edge jumps 0x0A -> 0x20 here (confirmed
    // against real-device descriptor dumps). These were inherited from
    // upstream's base descriptor and never serviced by the firmware; removing
    // them makes the report-ID set match real hardware exactly. -16 bytes each.
    0x85, 0x20, //   Report ID (32)
    0x09, 0x26, //   Usage (0x26)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x21, //   Report ID (33)
    0x09, 0x27, //   Usage (0x27)
    0x95, 0x04, //   Report Count (4)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x22, //   Report ID (34)
    0x09, 0x40, //   Usage (0x40)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x80, //   Report ID (-128)
    0x09, 0x28, //   Usage (0x28)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x81, //   Report ID (-127)
    0x09, 0x29, //   Usage (0x29)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x82, //   Report ID (-126)
    0x09, 0x2A, //   Usage (0x2A)
    0x95, 0x09, //   Report Count (9)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x83, //   Report ID (-125)
    0x09, 0x2B, //   Usage (0x2B)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x84, //   Report ID (-124)
    0x09, 0x2C, //   Usage (0x2C)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x85, //   Report ID (-123)
    0x09, 0x2D, //   Usage (0x2D)
    0x95, 0x02, //   Report Count (2)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA0, //   Report ID (-96)
    0x09, 0x2E, //   Usage (0x2E)
    0x95, 0x01, //   Report Count (1)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xE0, //   Report ID (-32)
    0x09, 0x2F, //   Usage (0x2F)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF0, //   Report ID (-16)
    0x09, 0x30, //   Usage (0x30)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1, //   Report ID (-15)
    0x09, 0x31, //   Usage (0x31)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2, //   Report ID (-14)
    0x09, 0x32, //   Usage (0x32)
    0x95, 0x0F, //   Report Count (15)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF4, //   Report ID (-12)
    0x09, 0x35, //   Usage (0x35)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF5, //   Report ID (-11)
    0x09, 0x36, //   Usage (0x36)
    0x95, 0x03, //   Report Count (3)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    // Report IDs 0xF6-0xF9 (vendor usages 0x37-0x3A) were the old WebHID config
    // command channel; removed (config is served over the WiFi web page now).
    0xC0, // End Collection
    // 273 bytes
};
static_assert(sizeof(desc_hid_report_ds) == 0x0111);

uint8_t const desc_hid_report_dse[] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05, // Usage (Game Pad)
    0xA1, 0x01, // Collection (Application)
    0x85, 0x01, //   Report ID (1)
    0x09, 0x30, //   Usage (X)
    0x09, 0x31, //   Usage (Y)
    0x09, 0x32, //   Usage (Z)
    0x09, 0x35, //   Usage (Rz)
    0x09, 0x33, //   Usage (Rx)
    0x09, 0x34, //   Usage (Ry)
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x06, //   Report Count (6)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20, //   Usage (0x20)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39, //   Usage (Hat switch)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x07, //   Logical Maximum (7)
    0x35, 0x00, //   Physical Minimum (0)
    0x46, 0x3B, 0x01, //   Physical Maximum (315)
    0x65, 0x14, //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04, //   Report Size (4)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x42, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00, //   Unit (None)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (0x01)
    0x29, 0x0F, //   Usage Maximum (0x0F)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x0F, //   Report Count (15)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x21, //   Usage (0x21)
    0x95, 0x0D, //   Report Count (13)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x22, //   Usage (0x22)
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x34, //   Report Count (52)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x02, //   Report ID (2)
    0x09, 0x23, //   Usage (0x23)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x05, //   Report ID (5)
    0x09, 0x33, //   Usage (0x33)
    0x95, 0x28, //   Report Count (40)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x08, //   Report ID (8)
    0x09, 0x34, //   Usage (0x34)
    0x95, 0x2F, //   Report Count (47)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x09, //   Report ID (9)
    0x09, 0x24, //   Usage (0x24)
    0x95, 0x13, //   Report Count (19)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x0A, //   Report ID (10)
    0x09, 0x25, //   Usage (0x25)
    0x95, 0x1A, //   Report Count (26)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    // Report IDs 11/12 (usages 0x41/0x42, 41-byte feature reports) removed: a
    // genuine DualSense / DualSense Edge jumps 0x0A -> 0x20 here (confirmed
    // against real-device descriptor dumps). These were inherited from
    // upstream's base descriptor and never serviced by the firmware; removing
    // them makes the report-ID set match real hardware exactly. -16 bytes each.
    0x85, 0x20, //   Report ID (32)
    0x09, 0x26, //   Usage (0x26)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x21, //   Report ID (33)
    0x09, 0x27, //   Usage (0x27)
    0x95, 0x04, //   Report Count (4)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x22, //   Report ID (34)
    0x09, 0x40, //   Usage (0x40)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x80, //   Report ID (-128)
    0x09, 0x28, //   Usage (0x28)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x81, //   Report ID (-127)
    0x09, 0x29, //   Usage (0x29)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x82, //   Report ID (-126)
    0x09, 0x2A, //   Usage (0x2A)
    0x95, 0x09, //   Report Count (9)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x83, //   Report ID (-125)
    0x09, 0x2B, //   Usage (0x2B)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x84, //   Report ID (-124)
    0x09, 0x2C, //   Usage (0x2C)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x85, //   Report ID (-123)
    0x09, 0x2D, //   Usage (0x2D)
    0x95, 0x02, //   Report Count (2)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA0, //   Report ID (-96)
    0x09, 0x2E, //   Usage (0x2E)
    0x95, 0x01, //   Report Count (1)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xE0, //   Report ID (-32)
    0x09, 0x2F, //   Usage (0x2F)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF0, //   Report ID (-16)
    0x09, 0x30, //   Usage (0x30)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1, //   Report ID (-15)
    0x09, 0x31, //   Usage (0x31)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2, //   Report ID (-14)
    0x09, 0x32, //   Usage (0x32)
    0x95, 0x34, //   Report Count (52)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF4, //   Report ID (-12)
    0x09, 0x35, //   Usage (0x35)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF5, //   Report ID (-11)
    0x09, 0x36, //   Usage (0x36)
    0x95, 0x03, //   Report Count (3)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x60, //   Report ID (96)
    0x09, 0x41, //   Usage (0x41)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x61, //   Report ID (97)
    0x09, 0x42, //   Usage (0x42)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x62, //   Report ID (98)
    0x09, 0x43, //   Usage (0x43)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x63, //   Report ID (99)
    0x09, 0x44, //   Usage (0x44)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x64, //   Report ID (100)
    0x09, 0x45, //   Usage (0x45)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x65, //   Report ID (101)
    0x09, 0x46, //   Usage (0x46)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x68, //   Report ID (104)
    0x09, 0x47, //   Usage (0x47)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x70, //   Report ID (112)
    0x09, 0x48, //   Usage (0x48)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x71, //   Report ID (113)
    0x09, 0x49, //   Usage (0x49)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x72, //   Report ID (114)
    0x09, 0x4A, //   Usage (0x4A)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x73, //   Report ID (115)
    0x09, 0x4B, //   Usage (0x4B)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x74, //   Report ID (116)
    0x09, 0x4C, //   Usage (0x4C)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x75, //   Report ID (117)
    0x09, 0x4D, //   Usage (0x4D)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x76, //   Report ID (118)
    0x09, 0x4E, //   Usage (0x4E)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x77, //   Report ID (119)
    0x09, 0x4F, //   Usage (0x4F)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x78, //   Report ID (120)
    0x09, 0x50, //   Usage (0x50)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x79, //   Report ID (121)
    0x09, 0x51, //   Usage (0x51)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x7A, //   Report ID (122)
    0x09, 0x52, //   Usage (0x52)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x7B, //   Report ID (123)
    0x09, 0x53, //   Usage (0x53)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    // Report IDs 0xF6-0xF9 (vendor usages 0x37-0x3A) were the old WebHID config
    // command channel; removed (config is served over the WiFi web page now).
    0xC0, // End Collection
    // 389 bytes
};
static_assert(sizeof(desc_hid_report_dse) == 0x0185);

#ifdef ENABLE_WAKE_HID
// 41-byte boot-keyboard report descriptor (modifier byte + reserved + 6 keycodes,
// no Report ID -- boot protocol forbids one and avoids collision with the gamepad's Report ID 1).
uint8_t const desc_hid_report_kbd[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,       //   Usage Minimum (Left Control)
    0x29, 0xE7,       //   Usage Maximum (Right GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data,Var,Abs) -- modifier byte
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Const) -- reserved byte
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0x65,       //   Usage Maximum (101)
    0x81, 0x00,       //   Input (Data,Array) -- 6 keycodes
    0xC0              // End Collection
};
_Static_assert(sizeof(desc_hid_report_kbd) == 45, "keyboard report descriptor length must match wDescriptorLength in config descriptor");

// Dummy HID report descriptor for the MINIMAL variant's placeholder interface.
// Its ONLY purpose is to occupy HID instance 0 in MINIMAL exactly as the
// gamepad does in FULL, so the keyboard is HID instance 1 in BOTH variants and
// its instance index never changes across a variant swap. That stability is
// what makes the "rogue keyboard on wake" structurally impossible: a gamepad
// report addressed to instance 0 can never reach the keyboard (instance 1).
//
// Vendor-defined usage page (0xFF00) so no OS binds it to a keyboard/mouse/
// gamepad driver -- it appears as an inert generic HID node with one input
// report we never send. 21 bytes.
uint8_t const desc_hid_report_dummy[] = {
    0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,       // Usage (Vendor Usage 1)
    0xA1, 0x01,       // Collection (Application)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x01,       //   Report Count (1)
    0x09, 0x01,       //   Usage (Vendor Usage 1)
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0xC0              // End Collection
};
_Static_assert(sizeof(desc_hid_report_dummy) == 21, "dummy report descriptor length must match wDescriptorLength in minimal config descriptor");
#endif

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
#ifdef ENABLE_WAKE_HID
    // With the boot keyboard enumerated, HID instance indices are STABLE
    // across variants:
    //   instance 1 = boot keyboard (both variants)
    //   instance 0 = real gamepad in FULL, inert dummy HID in MINIMAL
    // Gate on the ENUMERATED state (active_target.kbd), not the config value:
    // with the kbd off, instance 1 is idle and the host never asks for it.
    if (usb_wake_kbd_active() && itf == usb_kbd_hid_instance())
        return desc_hid_report_kbd;
    // Instance 0 in MINIMAL is the dummy placeholder; serve its descriptor.
    if (active_target.variant == DESC_VARIANT_MINIMAL) return desc_hid_report_dummy;
#endif
    (void) itf;
    if (ds_mode()) {
        return desc_hid_report_ds;
    }
    return desc_hid_report_dse;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
static char const *string_desc_arr[] =
{
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "Sony Interactive Entertainment", // 1: Manufacturer
    NULL, // 2: Product
    NULL, // 3: Serials will use unique ID if possible
};

static uint16_t _desc_str[60 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    size_t chr_count;

    if (ds_mode()) {
        string_desc_arr[2] = "DualSense Wireless Controller";
    }else {
        string_desc_arr[2] = "DualSense Edge Wireless Controller";
    }

    switch (index) {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;

        case STRID_SERIAL:
            chr_count = board_usb_get_serial(_desc_str + 1, 32);
            break;

        default:
            // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
            // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

            if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

            const char *str = string_desc_arr[index];
            if (str == nullptr) return NULL; // unused/placeholder slot

            // Cap at max char
            chr_count = strlen(str);
            size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
            if (chr_count > max_count) chr_count = max_count;

            // Convert ASCII string into UTF-16
            for (size_t i = 0; i < chr_count; i++) {
                _desc_str[1 + i] = str[i];
            }
            break;
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}

#ifdef ENABLE_WAKE_HID
//--------------------------------------------------------------------+
// Microsoft OS 2.0 descriptors (carried via BOS).
//
// Why this is here: the dongle is a composite device with USB Audio Class
// interfaces. By default Windows audio engine policy keeps USB audio devices
// at D0 even during system S3, blocking selective-suspend for the whole
// composite. Without selective-suspend the device never enters USB suspend,
// so tud_remote_wakeup() never works -- breaking wake-on-PS.
//
// MS OS 2.0 lets us tell Windows "yes, please selective-suspend this audio
// function": we set the registry property "SelectiveSuspendEnabled" = 1 on
// the audio function (interface 0). This causes Windows to write
//   HKLM\SYSTEM\CurrentControlSet\Enum\USB\<VID&PID>\<instance>
//        \Device Parameters\SelectiveSuspendEnabled = 1
// at enumeration time, opting our audio function in to selective suspend
// without breaking haptics.
//
// Reference: "Microsoft OS 2.0 Descriptors Specification".
//--------------------------------------------------------------------+

#define MS_OS_20_VENDOR_CODE 0x01

// Component sizes of the MS OS 2.0 descriptor set (so lengths are computed, not
// hand-counted as the whole set grows):
#define MS_OS_20_SET_HEADER_LEN    10
#define MS_OS_20_CONFIG_SUBSET_LEN 8
#define MS_OS_20_FUNC_SUBSET_HDR_LEN 8
// Registry-property feature: 10 fixed + 48 name + 4 data = 62.
#define MS_OS_20_REG_PROP_LEN      62
// Audio function subset = its header + the SelectiveSuspendEnabled reg property.
#define MS_OS_20_AUDIO_FUNC_LEN    (MS_OS_20_FUNC_SUBSET_HDR_LEN + MS_OS_20_REG_PROP_LEN)
// ONE set, served in both variants. Its single function subset targets
// interface 0: the audio function in FULL (where the SelectiveSuspend property
// is needed for wake), the inert dummy HID in MINIMAL (where a registry
// property on a HID function is harmless). NO WinUSB compat-id anywhere -- a
// WinUSB tag on itf 0 would make Windows prefer WinUSB over the audio class
// driver in FULL and bang it (Code 28).

#define MS_OS_20_CONFIG_SUBSET_TOTAL_LEN \
    (MS_OS_20_CONFIG_SUBSET_LEN + MS_OS_20_AUDIO_FUNC_LEN)

#define MS_OS_20_DESC_LEN \
    (MS_OS_20_SET_HEADER_LEN + MS_OS_20_CONFIG_SUBSET_TOTAL_LEN)

#define BOS_TOTAL_LEN        (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

// The BOS platform-capability descriptor embeds wMSOSDescriptorSetTotalLength,
// which Windows uses as wLength for the follow-up vendor request. It MUST equal
// the Set-Header wTotalLength of the set actually returned, or Windows rejects
// the whole MS OS 2.0 set -- and since this is a USB 2.1 device (BOS required),
// the composite parent fails to start (Code 10).
uint8_t const desc_bos[] = {
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, MS_OS_20_VENDOR_CODE)
};

uint8_t const *tud_descriptor_bos_cb(void) {
    return desc_bos;
}

// Audio function subset (identical in both variants): groups interfaces 0-2 as
// one function and sets SelectiveSuspendEnabled=1 on it (needed for wake). This
// does NOT bind a driver -- the audio class driver claims interface 0 by class.
#define MS_OS_20_AUDIO_SUBSET \
    /* --- Function Subset for the Audio function (8 bytes) --- */ \
    /* Audio Control is interface 0; AudioStreaming OUT/IN are 1/2 -- this */ \
    /* subset covers all three because they belong to the same function. */ \
    U16_TO_U8S_LE(0x0008),                  /* wLength */ \
    U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION),  /* wDescriptorType */ \
    0x00,                                   /* bFirstInterface (audio control) */ \
    0x00,                                   /* bReserved */ \
    U16_TO_U8S_LE(MS_OS_20_AUDIO_FUNC_LEN), /* wSubsetLength (this subset + its reg property) */ \
    /* --- Feature: Registry Property "SelectiveSuspendEnabled" = 1 (62 bytes) --- */ \
    U16_TO_U8S_LE(0x003E),                  /* wLength = 62 */ \
    U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),    /* wDescriptorType */ \
    U16_TO_U8S_LE(0x0004),                  /* wPropertyDataType = REG_DWORD_LITTLE_ENDIAN */ \
    U16_TO_U8S_LE(48),                      /* wPropertyNameLength = 48 bytes (24 UTF-16 chars) */ \
    /* PropertyName "SelectiveSuspendEnabled\0" UTF-16LE (48 bytes) */ \
    'S',0, 'e',0, 'l',0, 'e',0, 'c',0, 't',0, 'i',0, 'v',0, \
    'e',0, 'S',0, 'u',0, 's',0, 'p',0, 'e',0, 'n',0, 'd',0, \
    'E',0, 'n',0, 'a',0, 'b',0, 'l',0, 'e',0, 'd',0,  0,0, \
    U16_TO_U8S_LE(0x0004),                  /* wPropertyDataLength = 4 bytes */ \
    U32_TO_U8S_LE(0x00000001)               /* PropertyData = 1 (enabled) */

// MS OS 2.0 Set Header + Configuration Subset header, parameterised by the set's
// total length and config-subset total length (which differ between variants).
#define MS_OS_20_SET_AND_CONFIG_HEADER(desc_len, cfg_subset_len) \
    /* --- Set Header (10 bytes) --- */ \
    U16_TO_U8S_LE(0x000A),                  /* wLength */ \
    U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR),   /* wDescriptorType */ \
    U32_TO_U8S_LE(0x06030000),              /* dwWindowsVersion = Win 8.1+ */ \
    U16_TO_U8S_LE(desc_len),                /* wTotalLength */ \
    /* --- Configuration Subset (8 bytes) --- */ \
    U16_TO_U8S_LE(0x0008),                  /* wLength */ \
    U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION),  /* wDescriptorType */ \
    0x00,                                   /* bConfigurationValue (config index, 0) */ \
    0x00,                                   /* bReserved */ \
    U16_TO_U8S_LE(cfg_subset_len)           /* wTotalLength of this subset */

// The one MS OS 2.0 set: audio subset only, NO WinUSB tag (see note above).
uint8_t const desc_ms_os_20[] = {
    MS_OS_20_SET_AND_CONFIG_HEADER(MS_OS_20_DESC_LEN,
                                   MS_OS_20_CONFIG_SUBSET_TOTAL_LEN),
    MS_OS_20_AUDIO_SUBSET,
};
TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN,
                 "MS OS 2.0 descriptor length mismatch");

// Vendor-class control transfer hook. Windows reads BOS, sees the MS OS 2.0
// platform capability, then issues this vendor request to fetch the
// descriptor set itself.
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    if (stage != CONTROL_STAGE_SETUP) return true;
    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR) return false;
    if (request->bRequest == MS_OS_20_VENDOR_CODE && request->wIndex == 7) {
        // wIndex == 7 -> MS_OS_20_DESCRIPTOR_INDEX
        return tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_ms_os_20,
                                sizeof(desc_ms_os_20));
    }
    return false;
}
#endif // ENABLE_WAKE_HID
