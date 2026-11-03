# Third-party code

x4ssh's firmware is built from this repository, its submodules and the libraries `platformio.ini` pins. Everything needed
to rebuild it, or to relink it against a changed library, is here: see Build in the README.

| Part | Where | License |
| --- | --- | --- |
| [X4Term](https://github.com/penk/X4Term), the VT100 engine and renderer | `lib/`, `src/` (this repo starts from its history) | None published. See below |
| [LibSSH-ESP32](https://github.com/ewpa/LibSSH-ESP32), libssh for the ESP32 | fetched by PlatformIO | LGPL 2.1 |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | fetched by PlatformIO | Apache 2.0 |
| [freeink-sdk](https://github.com/Free-Ink/freeink-sdk), the BLE keyboard host, battery and board code | `freeink-sdk/` | MIT |
| [open-x4-sdk](https://github.com/open-x4-epaper/community-sdk), input and the display driver | `open-x4-sdk/`, `lib/EInkDisplay/` | MIT |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) | fetched by PlatformIO | LGPL 2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) | fetched by PlatformIO | Apache 2.0 |
| [DejaVu fonts](https://dejavu-fonts.github.io/) | `fonts/`, `design/fonts/`, `site/public/fonts/` | [DejaVu license](design/fonts/LICENSE_DEJAVU) |

## X4Term

X4Term has no license file. Its author has not been asked yet, so its code is here without a grant of rights, and this
repository adds no license of its own. If you are its author and want this changed or taken down, open an issue.
