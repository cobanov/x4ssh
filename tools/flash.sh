#!/bin/sh
# Flash x4ssh onto the X4. The first run saves the whole 16 MB flash (CrossPoint, its
# settings and saved Wi-Fi) to backup/, and refuses to flash until that backup exists.
#   tools/flash.sh            back up once, then flash x4ssh
#   tools/flash.sh --restore  write the backup back: CrossPoint exactly as it was
set -e
cd "$(dirname "$0")/.."
ESPTOOL="$HOME/.platformio/penv/bin/esptool"
PORT=${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}
[ -n "$PORT" ] || { echo "X4 not found: connect it over USB-C and wake it (press power)"; exit 1; }
BACKUP=backup/x4-flash-full.bin

if [ "$1" = "--restore" ]; then
  [ -f "$BACKUP" ] || { echo "no $BACKUP"; exit 1; }
  "$ESPTOOL" --chip esp32c3 --port "$PORT" -b 921600 write-flash 0 "$BACKUP"
  exit
fi

if [ ! -f "$BACKUP" ]; then
  mkdir -p backup/chunks
  # ROM loader, 1 MB at a time: esptool's stub dies on this X4's flash at 0x13a000 ("serial data
  # stream stopped") every time; the ROM reads it, slower (~5 min). verify-flash checks the result.
  for i in $(seq 0 15); do
    c=backup/chunks/$i.bin
    [ "$(stat -f %z "$c" 2>/dev/null)" = 1048576 ] && continue
    for try in 1 2 3 4; do
      "$ESPTOOL" --chip esp32c3 --port "$PORT" --no-stub read-flash $((i * 1048576)) 1048576 "$c" >/dev/null 2>&1 &&
        [ "$(stat -f %z "$c")" = 1048576 ] && break
      rm -f "$c"; sleep 1
    done
    [ -f "$c" ] || { echo "chunk $i failed 4 times, not flashing"; exit 1; }
    echo "backup $((i + 1))/16"
  done
  for i in $(seq 0 15); do cat backup/chunks/$i.bin; done > "$BACKUP.part"
  [ "$(stat -f %z "$BACKUP.part")" = 16777216 ] || { echo "backup is not 16 MB, not flashing"; exit 1; }
  "$ESPTOOL" --chip esp32c3 --port "$PORT" verify-flash 0 "$BACKUP.part" || { echo "backup does not match the device, not flashing"; exit 1; }
  mv "$BACKUP.part" "$BACKUP" && rm -r backup/chunks
  shasum -a 256 "$BACKUP" > "$BACKUP.sha256"
  echo "saved $BACKUP"
fi
tools/pio run -t upload --upload-port "$PORT"
