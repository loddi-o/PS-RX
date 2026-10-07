# DS5-Linux-Bridge — User Guide

Operational manual for the firmware: flashing, WiFi setup, pairing, the config
page, Wake-on-LAN, and OS-specific (Linux / Windows) behavior. For building from
source and debugging, see the [README](../README.md).

> **v2.3 beta availability:** Callouts marked **v2.3 beta** describe features
> currently available only in the latest pre-release. Download it from
> [Releases](https://github.com/kungaa/DS5-Linux-Bridge/releases), or enable
> **Include pre-releases** on the config page's Update tab before installing
> over WiFi.

- [Flashing the firmware](#flashing-the-firmware)
- [First-time WiFi setup](#first-time-wifi-setup)
- [Pairing a controller](#pairing-a-controller)
- [Adding a second controller](#adding-a-second-controller)
- [Using multiple controllers at once](#using-multiple-controllers-at-once)
- [The configuration page](#the-configuration-page)
  - [Live status](#live-status)
  - [Paired controllers (bond management)](#paired-controllers-bond-management)
  - [Network settings](#network-settings)
- [Waking your PC](#waking-your-pc)
  - [Wake-on-LAN](#wake-on-lan)
  - [USB wake keyboard](#usb-wake-keyboard)
- [Steam Deck plugin](#steam-deck-plugin)
- [Operating system & driver behavior](#operating-system--driver-behavior)

---

## Flashing the firmware

1. Hold the **BOOTSEL** button on your Raspberry Pi Pico 2 W.
2. Connect it to your PC via USB. It mounts as a drive named `RP2350`.
3. Drag and drop the compiled `.uf2` firmware onto that volume. The board
   reboots into the firmware automatically.

Once the adapter is on your WiFi, later releases can be installed without a
PC at all — see [Updating over WiFi](#updating-over-wifi-ota).

---

## Updating over WiFi (OTA)

After the first flash, the adapter can update itself straight from this
project's GitHub Releases:

1. Open the config page (`http://ds5.local`) → **Update** tab.
2. Click **Install latest**. The adapter goes offline, downloads the newest
   release for its board over HTTPS, verifies its checksum, installs it and
   reboots — about a minute in total. The page shows live progress
   (downloading / verifying / installing).
3. Settings, WiFi credentials and paired controllers are all kept.

To install the v2.3 beta rather than the latest stable release, enable
**Include pre-releases** before clicking **Install latest**.

Safety properties worth knowing:

* The download goes to a spare region of flash; the running firmware is not
  touched until the checksum has been verified. A failed download, WiFi
  hiccup or bad checksum changes **nothing** — the adapter just reboots back
  and reports why in the Update tab.
* The only critical moment is the final **installing** step (~15 s). If power
  is lost exactly then, the adapter won't boot — recover by flashing a `.uf2`
  over USB (BOOTSEL), which always works. So: don't unplug while the status
  says *installing*.
* **Pico W (RP2040) boards cannot use OTA** — their 2 MB flash can't hold the
  staged second image. Pico 2 W and the Waveshare board are supported, each
  installing its own board's release asset.
* The debug/verbose firmware updates onto the *standard* image for the board;
  reflash the debug `.uf2` manually if you need verbose logs again.

---

## First-time WiFi setup

The adapter serves its config page and sends Wake-on-LAN over your home WiFi, so
the first time you run it you tell it which network to join.

> **v2.3 beta:** The WPA3 selection, background retry policy, automatic
> authentication-failure recovery, BOOTSEL recovery gesture, and 2 Hz
> onboarding indicator described below require the latest v2.3 pre-release.

1. On first boot (with no WiFi saved) the adapter starts its own open setup
   network named **`DS5-Setup-XXXX`** (the `XXXX` is unique per adapter).
2. Join it from a phone or laptop and browse to **http://10.55.55.105/**.
3. Pick your home WiFi from the list, choose its security mode, enter its
   password, and save. Leave the default WPA2 choice for WPA2 and WPA2/WPA3
   transition networks. Select **WPA3-only (SAE)** when your router or phone
   hotspot requires WPA3. The adapter reboots and joins your network.

Once it's on your WiFi, the setup network disappears and the config page moves to
**http://ds5.local/** (see [The configuration page](#the-configuration-page)).

> **If it can't join** (the network is away, for example), the adapter keeps its
> saved credentials and retries in the background. The DualSense remains fully
> usable over Bluetooth while WiFi is unavailable. It retries quickly at first
> (after 5, 15, and 30 seconds), then once per minute; while a controller is
> connected, attempts are limited to once per minute to protect the shared
> radio. Authentication failures (usually a wrong password or incompatible
> security settings) use a separate policy: after three consecutive failures,
> with five seconds between attempts, the adapter clears those unusable
> credentials and reopens `DS5-Setup-XXXX` automatically. For any other failure,
> enter setup again by first powering off every connected controller, then
> press and release the Pico's **BOOTSEL** button three times within four
> seconds. The adapter reboots after the third release and enters
> `DS5-Setup-XXXX` for that boot without erasing the old credentials; saving a
> network replaces them. This gesture is the same on Pico W and Pico 2 W. If the
> config page is still reachable, **Reset saved WiFi** on its Network tab also
> deliberately clears the credentials and opens setup mode.

While the setup network is active, the onboard LED blinks continuously at 2 Hz.
This recovery indicator is shown even if the onboard LED was disabled in the
configuration.

Phone hotspots must expose a compatible 2.4 GHz network. If one cannot be
joined, enable its 2.4 GHz or maximum-compatibility mode. Use the onboarding
page's **WPA3-only (SAE)** choice for a WPA3-only hotspot; otherwise leave WPA2
selected. The adapter cannot infer WPA2 versus WPA3 from the SDK's scan result.

The controller itself does **not** need WiFi — pairing and gameplay work over
Bluetooth regardless. WiFi is only for the config page and Wake-on-LAN.

---

## Pairing a controller

1. Put the DualSense into Bluetooth pairing mode: hold **Share + PS** until the
   lightbar double-blinks.
2. The adapter detects, pairs, and connects. The onboard LED goes solid on a
   successful connection.
3. Once connected, the adapter presents the controller (gamepad, audio, haptics)
   to the host.

The adapter remembers controllers it has paired with (up to four). **It only
scans for a new controller when none is remembered.** Once at least one
controller is paired, it stops scanning — a remembered controller reconnects on
its own when you turn it on — so a nearby DualSense in pairing mode won't get
grabbed by your adapter.

If you cold-plug the adapter with no controller around, it stays in pairing mode
until the first controller pairs; after that, turning that controller on
reconnects it automatically.

---

## Adding a second controller

The adapter stops scanning once a controller is remembered, so *pairing* an
additional controller is a deliberate action rather than something that
happens automatically. (Already-paired controllers just reconnect on their own
with a PS press — see [Using multiple controllers](#using-multiple-controllers-at-once)
for how they behave together.)

On the [config page](#the-configuration-page), under **Paired controllers**,
click **Pair new controller**. By default (single-controller mode) this:

1. Disconnects the controller you're currently using — but **keeps its bond**,
   so it still reconnects later.
2. Opens a 30-second pairing window. Put the new controller into **Share + PS**
   pairing mode during that window.
3. The new controller connects and is remembered as an additional bond. The
   previous controller is held off during the window so it can't grab the slot
   back before the new one finishes pairing.

With **Allow multiple controllers simultaneously** enabled (Controller tab),
nothing is disconnected: already-connected controllers **keep playing** while
the window is open, one seat is reserved for the new controller, and it joins
alongside them.

> The config page is reachable whether or not a controller is connected, so you
> can also reach it (and pair) when the adapter is idle.

---

## Using multiple controllers at once

Up to **4 paired controllers** can be connected at the same time (local
co-op). This is **off by default** — out of the box the adapter behaves like
it always has, connecting one controller at a time. Enable **Allow multiple
controllers simultaneously** on the config page's Controller tab to use it.

Once enabled, each controller shows up on the PC as its own DualSense gamepad;
the first controller powered on is player 1, the next player 2, and so on.
Each pad's player LEDs show its number.

What to expect:

- **Joining:** each time a *new* player joins, the adapter briefly re-plugs
  itself (a couple of seconds — existing players' input pauses while the PC
  re-detects the adapter). A pad reconnecting into a seat it (or another pad)
  already used this session does **not** re-plug.
- **Leaving:** a player disconnecting never interrupts the others.
- **Audio & HD haptics are single-controller features.** With two or more
  controllers connected the adapter presents plain gamepads and **no audio
  device** — there isn't enough Bluetooth airtime for even one controller's
  audio stream next to a second controller's input, and HD (audio-based)
  haptics ride that same stream. **Classic rumble and adaptive triggers keep
  working on every controller.**
- **Getting audio / HD haptics back:** the adapter stays in multi-controller
  mode as long as *any* controller from the group is connected, so nobody's
  game is interrupted when others leave. To return to the full
  single-controller experience (audio, mic, HD haptics), power all controllers
  off, then reconnect one. **v2.3 beta:** the config page's Controller tab
  provides a red **Power off all controllers** button while two or more are
  connected. It keeps all controller pairings.
- Turning the toggle back **off** mid-session doesn't disconnect anyone; it
  applies to new connections only (the next second controller is refused).

---

## The configuration page

Once the adapter is on your WiFi (see [First-time WiFi setup](#first-time-wifi-setup)),
it hosts its own configuration web page — no app, no browser API, no internet
needed. The page is reachable **whether or not a controller is connected**, from
any device on the same network.

<p align="center">
  <img src="../assets/webconfig.png" alt="The DS5-Linux-Bridge configuration page, showing the live status card, controller mode and polling-rate selectors, audio-buffer and inactivity-timeout sliders, the tabbed navigation, and the paired-controllers list." width="360">
</p>

1. Open **http://ds5.local/** in any browser.
2. The page has four tabs:
   - **Controller** — controller mode, polling rate, audio buffer length,
     inactivity timeout, auto-disconnect, and the onboard LED.
   - **Paired** — the controllers the adapter remembers (see
     [below](#paired-controllers-bond-management)).
   - **Network** — the adapter's name and WiFi (see
     [Network settings](#network-settings)).
   - **Wake / WOL** — Wake-on-LAN and the USB wake keyboard (see
     [Waking your PC](#waking-your-pc)).
3. Adjust settings and click **Save**. Settings are written to the adapter's
   flash.

> `ds5.local` uses mDNS (Bonjour), which works out of the box on Windows, macOS,
> and most Linux desktops. If your device can't resolve `.local` names, browse to
> the adapter's IP address instead — you can find it in your router's client list,
> or it's printed to the debug UART on boot.

Flash writes from the page (saving settings, renaming/forgetting bonds) are made
**audio-safe**: the audio core is briefly parked during the flash erase/program
so writing while a controller streams audio doesn't corrupt the stream.

### Live status

The top of the page shows a live status card — whether a controller is
connected, its model (DualSense / DualSense Edge), and a battery gauge (percent
plus a charging indicator) — refreshed every few seconds. It's served from a
read-only `GET /api/status` endpoint, so any client (including the
[Steam Deck plugin](#steam-deck-plugin)) can poll it.

### Paired controllers (bond management)

The page lists the controllers the adapter remembers (the Bluetooth link keys it
stores, up to four). For each you can:

- **Rename** it with a short nickname (≤15 chars), stored in the adapter's flash.
- **Forget** it, or **Forget all** — clears the stored link key(s) so the slot is
  freed.
- **Pair new controller** — see [Adding a second controller](#adding-a-second-controller).

Forgetting a controller disconnects it if it's the one currently connected, and
blacklists its Bluetooth address so it can't silently auto-reconnect afterward.
To bring a forgotten controller back, re-pair it explicitly (**Share + PS**),
which clears the blacklist entry on a successful pair. The blacklist persists
across power cycles.

### Network settings

The **Network** tab has two things:

- **Device name** — the name the adapter uses on your network. The default is
  `ds5`, so the page lives at `ds5.local`. If you run more than one adapter,
  give each a unique name (e.g. `ds5-den`) so they don't collide — letters,
  digits, and hyphens only. A rename takes effect after the adapter reboots, and
  you then reach it at `http://<name>.local/`.
- **Reset saved WiFi** — forgets the saved network and reboots into the
  `DS5-Setup-XXXX` onboarding mode so you can join a different WiFi.

**v2.3 beta:** If the saved network is unavailable and this page cannot be
reached, power off all controllers and press and release the Pico's **BOOTSEL**
button three times within four seconds. The adapter reboots after the third
release and enters the setup network for one boot. Its old WiFi credentials
remain saved unless you submit replacements, and the onboard LED blinks
continuously at 2 Hz while setup mode is active. If the network instead rejects
authentication three times, the adapter automatically clears the rejected
credentials and enters this setup mode; those authentication retries are five
seconds apart.

---

## Waking your PC

The adapter can wake your PC from sleep when you turn on or press a button on the
controller. There are two mechanisms, on the **Wake / WOL** tab, and you can use
either or both:

### Wake-on-LAN

The most flexible option — it works even when the PC is **fully off** (S4/S5),
which USB wake can't do. The adapter sends a "magic packet" over WiFi to wake the
PC's network card.

1. On the **Wake / WOL** tab, enter your PC's **MAC address**. Don't know it?
   Type your PC's IP address and click **Find MAC** — the adapter looks it up on
   your network for you.
2. Optionally add a **second target** (e.g. a TV) to wake at the same time.
3. **Save.** From now on, a controller PS-button press wakes the target(s). You
   can also test it with **Wake now**.

Your PC must have Wake-on-LAN enabled in its BIOS/UEFI **and** its OS network
driver. WOL over WiFi requires the PC's wireless card to support it (many do
better over wired Ethernet — the adapter's magic packet reaches both).

### USB wake keyboard

An optional fallback for boards that don't support Wake-on-LAN but *do* wake from
a USB keyboard. When enabled, the adapter adds a tiny keyboard to its USB identity
that types a silent key (F15) to wake the host from sleep (S3) on a controller
press.

It's **off by default** — with it off, the adapter presents only the DualSense
over USB, so anticheat software sees a plain controller. Turning it on trades that
away for the extra USB keyboard. Saving applies immediately (the adapter briefly
re-plugs itself). Wake-on-LAN doesn't need this.

> The DualSense also auto-powers off a few seconds after the host sleeps or shuts
> down, to save the controller's battery. Turning the controller back on wakes the
> host again.

---

## Decky Loader plugin

A [Decky Loader plugin](https://github.com/kungaa/DS5-Linux-Decky) surfaces the
adapter's controller status and settings directly in the Steam Deck Quick Access
Menu. It's a client of the same on-device HTTP API the web page uses, so it works
without any extra firmware. See that repository for installation and usage.

---

## Operating system & driver behavior

### Linux / SteamOS (Bazzite, CachyOS)

- **Native driver integration.** Compatible with the kernel `hid-playstation`
  driver. When the Linux driver is active, the firmware yields LED and button
  control to the OS driver to avoid conflicts.
- **Jack detection.** The DS5's real `HP_DETECT` / `MIC_DETECT` jack bits are
  passed through to the host so `hid-playstation` (kernel ≥6.18) emits
  `SW_HEADPHONE_INSERT` / `SW_MICROPHONE_INSERT`. The ≥6.17 USB-audio mixer quirk
  wires these to the ALSA "Headphone Jack" / "Headset Mic Jack" controls that
  `alsa-ucm-conf` uses to switch between the mono Internal Speaker and the stereo
  Headphones profiles. The firmware also forces the HP_DETECT bit high in the
  report it presents to the host to bias toward the stereo Headphones profile for
  headphone output.

> **Known issue — one-earphone / mono audio on some setups.** Audio routing is
> ultimately decided host-side by PipeWire/ALSA via `alsa-ucm-conf`, and on some
> distros/kernels it lands on a mono profile (audio in one earphone only). This is
> kernel- and UCM-version dependent rather than a firmware fault — stereo
> generally needs a recent kernel (≥6.18) with the jack-detect mixer quirk. Note
> that bleeding-edge / rolling distros (e.g. CachyOS, Arch) can also *regress*
> here: a newer kernel or updated `alsa-ucm-conf` can change the routing behavior
> and break a setup that previously worked. Investigation is ongoing.

### Windows 11

- **v2.3 beta — Audio & layered mute.** Runs driverless. The physical Mute
  button and the Windows Sound-panel mute are independent layers: if both are
  active, clearing either one leaves the microphone muted until the other is
  also cleared. The controller's orange LED and the actual microphone stream
  always follow that combined state. Because the physical button is
  driverless, it does not move the Windows checkmark; that checkmark represents
  only the Windows layer.
- **v2.3 beta — Idle microphone transport.** When no application has the
  microphone stream open, the adapter asks the controller to stop uploading
  Bluetooth microphone frames. Opening a recorder or voice-chat application
  enables upload automatically; ordinary muting keeps the open stream alive
  but silent.
- **HD haptics.** Work out of the box in titles that support them on a wired
  DualSense (e.g. Death Stranding Director's Cut).
