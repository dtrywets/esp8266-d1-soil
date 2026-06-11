#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION_DATE="$(grep '#define FIRMWARE_VERSION_DATE' include/firmware_version.h | sed 's/.*"\([^"]*\)".*/\1/')"
VERSION_NUM="$(grep '#define FIRMWARE_VERSION "' include/firmware_version.h | sed 's/.*"\([^"]*\)".*/\1/')"
VERSION_LABEL="${VERSION_DATE} v ${VERSION_NUM}"

echo "Baue Firmware für Web Flash (${VERSION_LABEL}) …"

PIO="${PLATFORMIO_CORE_DIR:-$HOME/.platformio/penv}/bin/pio"
if [[ ! -x "$PIO" ]]; then
  PIO="pio"
fi

"$PIO" run -e d1_mini
"$PIO" run -e esp32c3

mkdir -p webflash/firmware webflash/firmware/esp32c3
rm -f webflash/firmware/esp32c3.bin
cp .pio/build/d1_mini/firmware.bin webflash/firmware/esp8266.bin

# ESP32-C3 (Arduino): mehrere Partitionen — nicht nur firmware.bin @ 0
cp .pio/build/esp32c3/bootloader.bin webflash/firmware/esp32c3/bootloader.bin
cp .pio/build/esp32c3/partitions.bin webflash/firmware/esp32c3/partitions.bin
cp .pio/build/esp32c3/firmware.bin webflash/firmware/esp32c3/firmware.bin

BOOT_APP0="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
if [[ ! -f "$BOOT_APP0" ]]; then
  echo "Fehler: boot_app0.bin nicht gefunden ($BOOT_APP0)" >&2
  exit 1
fi
cp "$BOOT_APP0" webflash/firmware/esp32c3/boot_app0.bin

python3 - <<PY
import json
from pathlib import Path

manifest_path = Path("webflash/manifest.json")
data = json.loads(manifest_path.read_text(encoding="utf-8"))
data["version"] = "${VERSION_LABEL}"
manifest_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Manifest-Version: {data['version']}")
PY

echo "Fertig:"
ls -lhR webflash/firmware/
echo "Starten: cd webflash && python -m http.server 8765"
