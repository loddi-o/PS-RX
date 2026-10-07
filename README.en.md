# PS-RX

English · **[Italiano](README.md)**

A Bluetooth receiver for **DualSense**, **DualSense Edge** and **DualShock 4** controllers, built on a
**Raspberry Pi Pico 2 W**. It connects up to **4 controllers** to your PC (Windows, Linux, SteamOS) as if
they were wired, with low latency, audio and microphone. You set it up with a Windows app, a Decky plugin
for SteamOS or a web page. No drivers to install.

## Features

- **Three modes**, chosen in the app:
  - **PlayStation**: the PC sees DualSense controllers, with gyro, touchpad, rumble, audio and microphone.
    The DualShock 4 also shows up as a DualSense.
  - **Xbox**: the PC sees Xbox 360 controllers (XInput), for games that don't recognize PlayStation
    controllers.
  - **Steam Controller 2026** (experimental): each controller becomes a new Steam Controller, with two
    trackpads and gyro. Works only while Steam is running.
- **Controller audio and microphone** (speaker and headphone jack), with one controller connected.
- **Touchpad as mouse**, with the left strip working as a scroll wheel.
- **Light bar color by slot**, like on PS5 (1 blue, 2 red, 3 green, 4 pink).
- **Controllers turn off**:
  - when the PC shuts down or goes to sleep;
  - after inactivity;
  - with the Share/Create + Options + L2 + R2 combination.
- **Wake the PC** by pressing PS: over USB from sleep, or with **Wake-on-LAN** when it's off.
- **WiFi only for Wake-on-LAN**: it turns off as soon as a controller connects, so Bluetooth gets the whole
  radio. You can scan for networks and test a network (password, address, internet).
- **Updates from GitHub** for the firmware, the app and the plugin, right from the app.
- **Notifications**: controller connected (slot, model, battery) and low battery.
- **English and Italian**: the system language is used; anything other than Italian gets English.

## What you need

- A **Raspberry Pi Pico 2 W** (the one with WiFi). The Pico 2 without "W" and the original Pico won't work.
- A **USB data cable** (some cables only charge).
- Optional, for Wake-on-LAN: a **2.4 GHz WiFi** network. The Pico 2 W can't see 5 GHz networks.

## Installation

All files are in the latest [release](https://github.com/loddi-o/PS-RX/releases/latest).

### Windows

1. Download and run `PS-RX-Setup-X.Y.Z.exe`.
   - The installer uses the Windows language and adds PS-RX to the Start menu.
   - The exe is not signed: if Windows SmartScreen blocks it, choose "More info" → "Run anyway".
2. Open PS-RX and choose **"Set up a new receiver…"**.
3. Connect the Pico. The app downloads the latest firmware, copies it to the Pico and waits for the
   receiver to be ready. For a used Pico, or one with another firmware, see
   [Installing the firmware on the Pico](#installing-the-firmware-on-the-pico).

### SteamOS / Steam Deck (Decky plugin)

1. Install [Decky Loader](https://decky.xyz).
2. Download `ps-rx-decky-X.Y.Z.zip`. In Decky, open Settings → Developer → **"Install plugin from ZIP"**.
3. Open PS-RX from the quick access menu (`…` button). For a new Pico use **"Install PS-RX on the Pico"**
   in the "New receiver" section.

### Linux and other systems (web page)

Open `ps-rx-X.Y.Z.html` in Chrome or Edge (WebUSB). You download the firmware yourself and load it from the
page. On a new Pico, copy `ps-rx-firmware-X.Y.Z.uf2` to the "RP2350" drive that appears when you plug it
in with BOOTSEL held.

## Installing the firmware on the Pico

The firmware is installed with the Pico in **BOOTSEL mode**. In this mode the Pico shows up on your PC as a
USB drive called **"RP2350"**, and you just copy the `.uf2` file onto it. BOOTSEL mode lives in the chip's
read-only memory: it can't be erased, so it's always there and works whatever firmware is installed.

### Which Pico do I have?

Connect the Pico to the PC with a data cable and see what appears:

| What you see | What it means |
|---|---|
| A drive named **"RP2350"** | The Pico is in BOOTSEL: it's new, its memory is empty, or you held BOOTSEL. Go to scenario 1. |
| A drive named **"RPI-RP2"** | It's a first-generation Pico or Pico W (RP2040): **not compatible**, you need a Pico 2 W. |
| The PS-RX receiver in the app | PS-RX is already installed: go to scenario 3. |
| Nothing else | The Pico has another firmware (scenario 2), or the cable is charge-only: try another one. |

### Scenario 1: new Pico or empty memory

A brand-new Pico 2 W is empty and starts in BOOTSEL by itself: no need to press anything.

- **Windows:** open the PS-RX app and connect the Pico. The app finds the "RP2350" drive, opens "Set up a new
  receiver" by itself and installs the latest firmware. You can also open it yourself with the "Set up a new
  receiver…" button.
- **SteamOS:** in the Decky plugin, "New receiver" section, press **"Install PS-RX on the Pico"**.
- **By hand (any system):** copy `ps-rx-firmware-X.Y.Z.uf2` to the "RP2350" drive. The Pico writes the
  firmware, the drive disappears, and a few seconds later the Pico restarts as a PS-RX receiver.

Without internet, the app and the plugin use the firmware bundled with them.

### Scenario 2: Pico with another firmware

The Pico has MicroPython, CircuitPython or another project's firmware, so it won't start in BOOTSEL by itself.

1. **Save what you need first.** For example, your MicroPython scripts: installing PS-RX overwrites the old
   firmware.
2. Unplug the Pico.
3. **Hold the BOOTSEL button** (the white button on the board) and, while holding it, plug the USB cable back in.
4. Release the button when the "RP2350" drive appears.
5. Continue as in **scenario 1**.

Data left in memory by the old firmware won't cause problems: PS-RX checks its own settings with a
checksum and, if it doesn't find them, starts with the defaults.

### Scenario 3: Pico that already has PS-RX

- **Update:** System → Updates (GitHub) → "Check for updates", then install the new firmware. The firmware goes
  over USB, no BOOTSEL needed.
- **Reinstall** (for example if an update was interrupted): open "Set up a new receiver…" with the receiver
  connected. The app restarts it in BOOTSEL and reinstalls the latest firmware.

Either way, paired controllers, WiFi networks and settings are kept. The controllers must be off.

If the receiver stops responding, you can always recover it with the BOOTSEL button, as in scenario 2.

> **Warning:** while the Windows app is open, *any* Pico 2 connected in BOOTSEL gets PS-RX installed
> automatically. If you're programming a Pico for another project, close the PS-RX app first (also from
> the icon near the clock).

## First steps

1. **Pair a controller:** press "Pair a new controller", then hold **Create + PS** (DualSense) or
   **Share + PS** (DualShock 4) until the light flashes quickly. After that, pressing PS is enough.
2. Choose the **mode** in System. Changing it reconnects the receiver's USB.
3. Each paired controller has its own settings: audio, microphone, polling rate (up to 1000 Hz),
   touchpad as mouse.

Changes are saved automatically when the controllers are off. The receiver writes to its memory only with
no controllers connected, so the input is never disturbed.

## Waking the PC

When you press PS on a paired controller, the receiver tries to wake the PC.

- **From sleep, over USB.** "Wake the PC from sleep over USB" is on by default: the receiver adds a small
  USB keyboard that presses a key. It's the most reliable way. If an anti-cheat flags it, you can turn it
  off.
- **From power-off, or as a fallback: Wake-on-LAN.** In Network:
  1. save the 2.4 GHz WiFi network and use **Test the network** (Test in the plugin and the page);
  2. enter the MAC of the PC's **wired** network adapter (for example `12:34:56:78:9A:BC`);
  3. enable Wake-on-LAN in the BIOS and on the network adapter.

  The receiver's USB port must stay powered while the PC is off (a BIOS option like "USB power in S5").

While the receiver is trying to wake the PC, the Pico's LED blinks (unless "Receiver LED off" is on), and
the controller works normally.

## Good to know

- **Slots:** by default the PC sees only the controllers that are on, and when a new one connects the others
  pause for about 1 s. With **"Always 4 gamepads on USB"** the PC always sees 4 gamepads and nothing
  pauses.
- **Audio:**
  - works with **one** controller connected only: with two or more, Bluetooth can't keep up;
  - the DualShock 4 has no audio.
- **WiFi:** it's off while you play. It turns on only with no controllers connected, for Wake-on-LAN and
  for scanning or testing networks.
- **One program at a time:** the app, the plugin and the page share the same USB channel. If one can't
  find the receiver, close the others.
- **Steam Controller mode:** it's experimental. Steam may offer controller firmware updates; they have no
  effect.
- **Xbox mode:** no gyro and no audio; the touchpad works only as a mouse.

## Troubleshooting

| Problem | What to do |
|---|---|
| Receiver not found | Close the app, plugin or page open elsewhere. Try another cable or port. |
| "unknown command" | The firmware is older than the app: update it from System → Updates (GitHub). |
| The network doesn't show up when scanning | It's 5 GHz: use the router's 2.4 GHz name. |
| The PC doesn't wake from sleep | Check that "Wake the PC from sleep over USB" is on and that the port stays powered. In Device Manager, on the receiver's HID keyboard, enable "Allow this device to wake the computer". |
| The PC doesn't turn on from power-off | Use Network → Test the network. Check the MAC, Wake-on-LAN in the BIOS and USB power in S5. |
| The controller won't pair | Turn it off (hold PS for 10 s), press "Pair", then hold Create/Share + PS until it flashes quickly. |

## For developers

The firmware is based on [DS5-Linux-Bridge](https://github.com/kungaa/ds5-linux-bridge) v2.3.0-beta.1
(`firmware/BASE_DLB.txt`). Where things are:

- PS-RX code: `firmware/src/psrx/`;
- Windows app (PySide6) and Python library: `app/`;
- Decky plugin: `app/decky/`;
- WebUSB page: `web/`.

The build tools (PowerShell) are in `strumenti/`. The technical notes are in [`CLAUDE.md`](CLAUDE.md), in
Italian.

## License and thanks

PS-RX is released under **GPLv3** (see `firmware/LICENSE`), like DS5-Linux-Bridge, which it's based on.

Thanks to:
- DS5-Linux-Bridge and DS5Dongle, for the firmware base;
- SDL3, for the controller documentation;
- openpuck and sc2-research, for the protocol of the new Steam Controller.

PS-RX is not affiliated with Sony, Microsoft or Valve. DualSense, DualShock, Xbox and Steam are trademarks
of their respective owners.
