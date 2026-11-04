# x4ssh

SSH terminal for the Xteink X4, held sideways, typed on with a BLE keyboard. Part of the cyberdeck.

76x22 on the 800x480 e-ink, inside a margin that keeps text off the bezel, with a status bar along the
bottom: `wifi: <ssid> <signal>%` on the left, `<battery>% | <clock>` on the right. Built on [penk/X4Term](https://github.com/penk/X4Term)'s VT100 engine and renderer,
[LibSSH-ESP32](https://github.com/ewpa/LibSSH-ESP32) for SSH and
[freeink-sdk](https://github.com/Free-Ink/freeink-sdk)'s `BleKeyboardHost` (the BLE stack CrossPoint uses) for the keyboard.
Licenses: [THIRD_PARTY.md](THIRD_PARTY.md).

https://x4ssh.cobanov.dev

## Install

Needs CrossPoint on the X4 and a Bluetooth LE keyboard.

1. Copy `firmware.bin` from the [site](https://x4ssh.cobanov.dev) or a [release](https://github.com/cobanov/x4ssh/releases) to the SD card's root
2. Put the card in the X4
3. Open Settings → System → SD Card Firmware Update

`crosspoint` at the prompt goes back. The SD update itself has not been tried on a device yet; a USB flash works (below).
The X4 Pro and Classic are untested.

## Two firmwares

`crosspoint-x4ssh.bin`, on every [release](https://github.com/cobanov/x4ssh/releases), is CrossPoint 1.6.5 with
`tools/crosspoint-dualboot.patch` on it. With it, Power wakes CrossPoint and Down + Power wakes x4ssh, no keyboard needed.
Without it `crosspoint` is one way: the way back is the SD update or USB.

Install it first, the same way as `firmware.bin`, then install `firmware.bin` and type `crosspoint` once. Tried on a device
over USB only (`esptool write-flash 0x650000 crosspoint-x4ssh.bin`, with x4ssh in the first slot). CrossPoint's own updates
write the slot x4ssh sits in.

```
git clone --branch 1.6.5 --recurse-submodules https://github.com/crosspoint-reader/crosspoint-reader
cd crosspoint-reader && git apply ../x4ssh/tools/crosspoint-dualboot.patch
../x4ssh/tools/pio run -e gh_release
```

Both projects rebuild the same PlatformIO core, so never build them at once.

## Build and flash

```
git submodule update --init --recursive
tools/flash.sh            # first run saves the whole flash to backup/ (CrossPoint + its settings), then flashes
tools/flash.sh --restore  # back to CrossPoint, exactly as it was
```

The backup goes through the ROM loader: esptool's stub dies on this X4's flash at `0x13a000` every time.

Build with `tools/pio run`, not plain `pio`: the firmware rebuilds the Arduino core with CrossPoint's
heap settings (BLE controller in flash, trimmed NimBLE pools), which needs PlatformIO Core 6.1.19
(Core 6.2's SCons breaks pioarduino's core builder). The first build compiles ESP-IDF and takes a few
minutes; later builds are cached. Heap with Wi-Fi and the keyboard up: ~55 KB free (12 KB on the
prebuilt core).

`backup/` holds CrossPoint's NVS, saved Wi-Fi passwords included: keep it local.

## Use

Boots into a local prompt:

```
x4> wifi                 ssid + password, kept in NVS
x4> pair                 pair a BLE keyboard (or Confirm on an empty prompt, before the first one)
x4> key                  this device's ed25519 public key, for authorized_keys
x4> ssh me@nuc           user@host[:port]
x4> ssh                  the last host again
x4> forget <host>        drop a saved host key
x4> stat                 keyboard, wifi, heap, battery
x4> tz UTC-3             the clock's zone as a POSIX TZ string, kept in NVS (UTC until set)
x4> crosspoint           boot the firmware x4ssh was installed from
x4> sleep                turn off; the power button wakes it
```

- The key is generated on the device on first boot and never leaves it. Password login is the fallback.
  It and the Wi-Fi password sit unencrypted in flash: whoever holds the device can read them over USB.
- Host keys: trust on first use, saved in `/littlefs/known_hosts`; a changed key always refuses.
- Everything drawn is mirrored to USB serial, and USB serial is also a keyboard: `pio device monitor --eol CR`
  drives the device from a Mac.
- Buttons: arrows, Confirm = Enter, Back = Esc, Confirm + Back = redraw. Back, held: a page of every button, key
  and command with the wifi and keyboard state, for when there is no keyboard; any key closes it. Power, held: sleep.
  With [both firmwares](#two-firmwares): Power wakes CrossPoint, Down + Power wakes x4ssh.
  Asleep, the e-ink keeps the sleeping snail, "press power to wake" and `sleeping since <time>` with the battery.
- The snail: on boot it sweeps a dish while wifi connects, then puts it down. When text scrolls up to it, it moves
  to the middle of the status bar. Art, logos, tokens and rules are in `design/`; `design/mascot/build.py` rebuilds
  them and `include/mascot.h`.
  Wi-Fi and Bluetooth are shut down before deep sleep, as ESP-IDF requires.
- BLE only: the ESP32-C3 has no Classic Bluetooth, so keyboards like Apple's Magic Keyboard can't connect.
  Tested: M7 (pairs, reconnects on boot).
- Draws ASCII, Latin-1, box drawing (light, heavy, double), DEC line drawing and block elements; no Turkish ş ğ ı İ yet.
- Arrow keys follow DECCKM; cursor-position and device-attribute queries are answered over SSH.

## Site

`site/` is x4ssh.cobanov.dev: `public/` the page, `hero/` the live panel (Stageshow kit), `film.sh` its frames through the
real renderer, `tiles.py` the dither and the snail from the kit. `cd site && npx wrangler deploy` builds the hero, copies
the last `firmware.bin` into `/dl/` and ships it. Rerun `film.sh` when anything drawn on the panel changes.

## Test

```
test/run.sh     # key mapping, drawn glyphs and the mascot through the real renderer, on the host
tools/pio run   # firmware
```
