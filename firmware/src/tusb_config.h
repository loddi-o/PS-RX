/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- CLASS -------------//
#define CFG_TUD_AUDIO             1
// HID class instance count: one per controller slot, plus one for the wake
// keyboard. The keyboard is a RUNTIME web-UI toggle (Config_body.
// wake_kbd_enabled), so its instance must exist in every build -- slot-0
// gamepad/dummy at instance 0, boot keyboard at instance 1 when the
// kbd-bearing descriptor variants are enumerated, tail gamepads after (see
// the slot<->instance map in usb_descriptors.cpp). Instances not enumerated
// by the active variant sit idle (~130 B of buffers each -- cheap, and
// TinyUSB could not change the count at runtime anyway). At MULTI_SLOT_COUNT
// 1 this is the original value 2.
#ifndef MULTI_SLOT_COUNT
#define MULTI_SLOT_COUNT 1
#endif
#define CFG_TUD_HID               (MULTI_SLOT_COUNT + 1)
#define CFG_TUD_CDC               0
// No CDC-NCM: the config web UI is served over WiFi (ENABLE_WIFI_WOL), keeping
// the USB face DualSense-only. Diagnostics go over UART0 (GP0 TX, 115200 8N1),
// never USB-CDC -- USB serial perturbed the very USB timing we needed to measure.
#define CFG_TUD_NCM               0
#define CFG_TUD_ECM_RNDIS         0
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#ifdef ENABLE_WAKE_HID
#define CFG_TUD_VENDOR            1 // routes MS OS 2.0 vendor request to tud_vendor_control_xfer_cb
#else
#define CFG_TUD_VENDOR            0
#endif

// HID buffer size Should be sufficient to hold ID (if any) + Data
#define CFG_TUD_HID_EP_BUFSIZE    64

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX              4

// 16bit data in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX      2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX              16

// Microphone (IN/TX) path: 2-channel, 16-bit (UCM expects HWChannels=2 for both Mic and Headset devices)
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX              2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX      2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_TX              16

// UAC1 Full-Speed endpoint size
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE            48000
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT     TUD_AUDIO_EP_SIZE(false, CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN      TUD_AUDIO_EP_SIZE(false, CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX          CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX           CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ       (3 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ        (16 * CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)

// Enable OUT EP (speaker) and IN EP (mic)
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                 1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                  1

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
