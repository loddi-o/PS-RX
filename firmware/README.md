# DS5-Linux-Bridge

A Linux-focused firmware for the Raspberry Pi Pico 2 W that turns it into a
latency-optimized USB-to-Bluetooth bridge for the Sony DualSense (DS5)
controller — reproducing the wired experience (speaker, microphone, and native
HD haptics) over Bluetooth. Windows works too, but the project's priority is
getting it right on Linux / SteamOS (Bazzite).

<p align="center">
  <a href="https://ko-fi.com/mkungaa">☕ <b>Support this project on Ko-fi</b></a>
  &nbsp;·&nbsp;
  <a href="docs/USER_GUIDE.md">📖 User guide</a>
  &nbsp;·&nbsp;
  <a href="https://github.com/kungaa/DS5-Linux-Decky">🎮 Decky Loader plugin</a>
</p>

> **Opinionated Linux fork of [awalol/DS5Dongle](https://github.com/awalol/DS5Dongle).**
> A downstream fork that tracks awalol's excellent upstream firmware and adds
> Linux-specific fixes and tuning. **Not** an official release of that project.
> All upstream work is credited under the MIT License — see
> [License & Acknowledgement](#license--acknowledgement).

---

## Core Features

> **v2.3 beta availability:** Features labeled **v2.3 beta** below are currently
> available only in the latest pre-release. Download it from
> [Releases](../../releases), or enable **Include pre-releases** on the config
> page's Update tab before installing over WiFi. The **BETA** label on the
> existing multi-controller and OTA features describes their maturity; both
> are also included in the stable v2.2.1 release.

- 🎮 **Full wireless controller emulation** — DualSense Bluetooth reports → standard
  USB HID gamepad at up to 1000 Hz. Supports DualSense (DS5) and DualSense Edge
  (DSE), including DSE PS-app profiles.
- 👥 **Multiple controllers (local co-op, opt-in) — BETA** — up to 4 DualSense
  pads connected at once, each as its own USB gamepad with per-player LEDs. Off
  by default (a web-page toggle enables it; the default behavior is unchanged
  single-controller). Audio and HD haptics are single-controller features (not
  enough Bluetooth airtime beside a second pad); classic rumble and adaptive
  triggers work for everyone. **v2.3 beta:** a config-page action can power off
  all connected pads without forgetting their pairings. See the
  [user guide](docs/USER_GUIDE.md#using-multiple-controllers-at-once).
- 📳 **Wireless HD haptics** — recreates the cabled audio-based haptic feedback over
  Bluetooth, streaming the dedicated haptic waveforms to the controller's
  voice-coil actuators. Compatibility mirrors the wired experience.
- 🔊 **Wireless audio (speaker + mic)** — speaker/headphone playback and microphone
  upload over standard USB Audio Class — full quality, no Bluetooth headset-profile
  (HSP) downgrade. **v2.3 beta:** controller microphone upload is disabled while
  no host application has the microphone stream open.
- 🔇 **Layered hardware mic mute — v2.3 BETA** — driverless local mute via the
  physical Mute button and host sound-panel mute remain independent, so
  releasing either cannot override the other. The orange LED follows the
  effective mute state, while active host drivers (e.g. Linux
  `hid-playstation`) retain ownership.
- 🌐 **Web config over WiFi + bond management** — the adapter joins
  your home WiFi and hosts its own configuration page at **http://ds5.local/**
  (no app, no WebHID, any browser). First-run setup is a phone-friendly captive
  portal. Adjust settings and manage remembered controllers, connected or not.
- 📶 **WPA3 and resilient WiFi recovery — v2.3 BETA** — onboarding supports an
  explicit WPA3-only choice. If the saved network is unavailable, gameplay
  continues offline while WiFi retries. Three consecutive authentication
  failures reopen onboarding automatically; for other failures, power
  controllers off and click BOOTSEL three times to reopen it without erasing
  the old network.
- 🎮 **Decky Loader companion plugin** — a [Decky Loader plugin](https://github.com/kungaa/DS5-Linux-Decky)
  surfaces controller status and settings in the Quick Access Menu, talking to
  the same on-device API as the web page.
- ⬆️ **Over-the-air updates — BETA** — one click on the config page installs the
  latest GitHub release over HTTPS (pinned CA roots, SHA-256-verified against
  the release manifest) with no PC involved. The download is staged in spare
  flash, so a failed/interrupted download changes nothing. Pico 2 W and
  Waveshare boards; the Pico W's 2 MB flash is too small. See the
  [user guide](docs/USER_GUIDE.md#updating-over-wifi-ota).
- 🖥️ **Wake-on-LAN** — press the controller's PS button to wake a sleeping or
  fully-off PC (even S4/S5) by sending a magic packet over WiFi. Wakes up to two
  targets (e.g. your PC and a TV), with a one-click "Find MAC" helper.
- 🔌 **Wake from sleep (S3)** — an optional USB wake keyboard (a web toggle) lets a
  controller press wake the host over USB on boards that don't do WiFi WOL. Off by
  default, so the adapter looks like a plain DualSense over USB. The DualSense also
  auto-powers off after the host sleeps/shuts down to save its battery.
- ⚡ **Low-latency performance** — Bluetooth/USB/audio hot paths run from RAM
  (`.time_critical`) to avoid Flash XIP cache thrashing, with tuned scheduling
  and pipelines to minimize latency and eliminate audio stutter.
- 📡 **USB 3.0 RF-noise watchdog** — auto-retries Bluetooth connections stalled by
  2.4 GHz interference from USB 3.0 ports (USB 2.0 ports recommended).
- 🚨 **Visual notifications** — POST LED pattern, solid LED on a live link, a
  once-per-second low-battery blink (≤10%), and, in **v2.3 beta**, a continuous
  2 Hz blink during WiFi onboarding.

📖 **See the [User Guide](docs/USER_GUIDE.md)** for flashing, pairing, the config
page, and OS-specific (Linux / Windows) behavior and troubleshooting.

---

## Quick Start

1. **Flash** — hold **BOOTSEL** on the Pico 2 W, plug it into USB, and drop the
   `.uf2` onto the mounted `RP2350` volume.
2. **Onboard WiFi** — the first time, the adapter opens a setup network named
   **`DS5-Setup-XXXX`**. Join it from your phone or laptop, browse to
   **http://10.55.55.105/**, pick your home WiFi, and enter its password.
   **v2.3 beta** also offers a WPA3-only choice for networks that require it.
   The adapter reboots and joins your network.
3. **Pair** — put the DualSense in pairing mode (hold **Share + PS** until the
   lightbar double-blinks). The adapter detects, pairs, and connects; the onboard
   LED goes solid.
4. **Configure** *(optional)* — once on your WiFi, browse to **http://ds5.local/**
   to change settings, set up Wake by USB or Wake-on-LAN, and manage paired controllers.

**v2.3 beta:** if the saved WiFi is unavailable later, controller operation
continues normally while WiFi retries. To change networks, power off every
controller and press and release the Pico's **BOOTSEL** button three times
within four seconds; the setup network reopens for one boot.

Full details, including onboarding, Wake-on-LAN, adding a second controller, and
per-OS audio notes, are in the **[User Guide](docs/USER_GUIDE.md)**.

---

<details>
<summary><b>What's different in this fork</b> (vs. upstream awalol/DS5Dongle)</summary>

These are the deliberate departures from upstream that define this fork's
*direction* (upstream is actively developed and excellent — this isn't a list of
things it gets wrong):

- 🌐 **Web config over WiFi instead of WebHID.** The adapter joins your home WiFi
  and serves its own config page at `ds5.local` — works in any browser on any OS,
  no WebHID (which never worked in Firefox). Includes bond management.
- 🖥️ **Wake-on-LAN.** Press the controller's PS button to wake a sleeping or
  fully-off PC over the network — including S4/S5, which USB wake can't reach.
- 🎮 **Decky Loader plugin** that drives the same on-device API.
- 🕵️ **Plain-DualSense USB face by default.** Over USB the adapter presents only
  the controller — no keyboard, no network device — so anticheat sees a normal
  DualSense. The optional USB wake keyboard is a web toggle, off unless you turn
  it on.
- 🎚️ **Lightweight boxcar/linear resampler** for haptics and the 512→480 speaker
  resample instead of WDL — cheaper on CPU, tuned to match cabled intensity.
- 🔈 **Volume fully yielded to the host** — held in RAM only, never written to flash.
- 🐧 **Linux-first integration & docs** — `hid-playstation` jack-detection wiring
  and kernel-version-aware stereo-audio notes (see the [User Guide](docs/USER_GUIDE.md)).

</details>

---

## Build Instructions

The build defaults to `Release`. Building `Debug` (`-O0`) causes audio
crackling — only do so when actually debugging.

1. Ensure the Pico SDK is configured and its `tinyusb` library is up to date.
2. Compile with the standard Pico CMake configuration:
   ```bash
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make
   ```
3. Handy CMake toggles: `-DENABLE_WIFI_WOL=OFF` builds a smaller firmware with no
   WiFi config page or Wake-on-LAN; `-DENABLE_BATT_LED=OFF` disables the
   low-battery LED blink; `-DENABLE_WAKE_HID=OFF` drops the wake machinery
   entirely.

### Board targets

The default build targets the **Raspberry Pi Pico 2 W** (RP2350). Two other
boards are supported via CMake options (mutually exclusive):

| Board | Configure with | Notes |
| --- | --- | --- |
| Raspberry Pi Pico 2 W (RP2350) | *(default)* | Full feature set. |
| Waveshare RP2350B-Plus-W | `-DWAVESHARE_RP2350B_PLUS_W_BUILD=ON` | USB-C, 16 MB flash, RM2 wireless. Ships its own board header (the RM2 module is on different GPIOs than the Pico 2 W) and forces boot2 XIP setup for its Puya flash. A convenience script lives at `boards/build_waveshare_rp2350b_plus_w.sh`. |
| Raspberry Pi Pico W (RP2040) | `-DPICO_W_BUILD=ON` | Legacy RP2040; **no audio** (gamepad + haptics only). |

The release builds publish UF2s for the Pico 2 W (standard + verbose) and the
Waveshare board.

---

## Debugging

There is **no USB serial port**. USB-CDC serial was removed because it shares the
USB stack with the audio isochronous endpoints and perturbs the very timing you
need to measure.

Diagnostic output goes to **UART0** instead, which is independent of USB:

- **Pico GP0 (pin 1)** = UART TX → connect to your USB-serial adapter's **RX**
- **Pico GND (pin 3)** → adapter **GND**
- Settings: **115200 baud, 8N1**, no flow control (3.3 V logic — do not feed 5 V
  into GP0)

For louder logs, build the verbose variant with `-DENABLE_VERBOSE=ON` (the CI
also publishes a `ds5-bridge-debug.uf2` built this way).

---

## License & Acknowledgement

This project is licensed under the **GNU General Public License v3.0** — see
[LICENSE](LICENSE).

DS5-Linux-Bridge is a fork and continuation of the original
[DS5Dongle](https://github.com/awalol/DS5Dongle) project created by **awalol**,
which is MIT-licensed. Portions of this source originate from that MIT work;
the original MIT notice is preserved in [LICENSE-MIT](LICENSE-MIT) as that
license requires. The project as a whole is now distributed under GPLv3. This
fork continues to track upstream improvements (such as the RAM-relocation
infrastructure and DualSense Edge profile support).
