# Bodenfeuchte Soil Sensor (ESP8266 / ESP32-C3)

Firmware für **Wemos D1 Mini (ESP8266)** oder **ESP32-C3** mit **Capacitive Soil Moisture Sensor v1.2**: MQTT-Sensor mit Home-Assistant-Discovery, Web-Dashboard (Status + Kalibrierung) und OTA-Updates.

## Web Flash (Browser, USB)

Im Ordner `webflash/` liegt ein Portal auf Basis von [ESP Web Tools](https://esphome.github.io/esp-web-tools/) — Chip-Erkennung **ESP8266** oder **ESP32-C3**, passende Firmware automatisch. **WLAN** lässt sich direkt im Browser einrichten ([Improv Wi-Fi](https://www.improv-wifi.com/serial/)): nach dem Flash oder separat über „WLAN einrichten“.

**Online (GitHub Pages):** [dtrywets.github.io/esp8266-d1-soil](https://dtrywets.github.io/esp8266-d1-soil/) — wird bei jedem Push auf `main` per GitHub Actions gebaut und veröffentlicht (Firmware-Bins inklusive).

**Lokal:**

```bash
./scripts/build-webflash.sh          # .bin-Dateien bauen & Manifest aktualisieren
cd webflash && python -m http.server 8765
# http://localhost:8765 — Chrome/Edge, USB-Kabel
```

### GitHub Pages einmalig aktivieren

1. Repository auf GitHub öffnen → **Settings** → **Pages**
2. **Build and deployment** → **Source:** `GitHub Actions`
3. Nach dem nächsten Push auf `main` (oder manuell: Actions → „GitHub Pages (Web Flash)“ → **Run workflow**) ist die Seite erreichbar.

Voraussetzung im Browser: **Chrome** oder **Edge** (Web Serial API). Nach erfolgreicher WLAN-Einrichtung öffnet der Browser das Geräte-Dashboard; MQTT bleibt optional im Dashboard konfigurierbar.

## Hardware

| Teil | Anschluss |
|------|-----------|
| Sensor VCC | 3,3 V (**niemals 5 V am A0**) |
| Sensor GND | GND |
| Sensor AOUT | A0 |

Optional: Sensor-Versorgung über GPIO14 (D5) per MOSFET/Transistor — in `config.h` mit `SENSOR_POWER_ENABLED` steuerbar.

## Schnellstart

1. `include/config.example.h` nach `include/config.h` kopieren und anpassen
2. Verkabelung prüfen (3,3 V!)
3. Erstflash: `pio run -t upload`
4. Einrichtung:
   - AP-Modus: WLAN `D1Soil-XXXX` (letzte 4 Hex der Chip-ID)
   - Station: `http://D1Soil1.local/` oder IP aus dem Serial-Log
5. Im Dashboard WLAN und MQTT konfigurieren
6. Kalibrierung: trocken (Luft) und nass (Sonde in Wasser) einstellen
7. Home Assistant erkennt den Sensor automatisch per MQTT Discovery

## OTA-Update

```bash
cp ota_upload.ini.example ota_upload.ini
# IP in ota_upload.ini eintragen
pio run -e d1_mini_ota -t upload
```

Alternativ: `.bin` über das Web-Dashboard (Tab „Firmware“) hochladen.

## MQTT-Topics

| Topic | Beschreibung |
|-------|--------------|
| `{MQTT_BASE_TOPIC}/moisture` | Feuchte in % (1 Dezimalstelle, retain) |
| `{MQTT_BASE_TOPIC}/moisture_raw` | Roh-ADC |
| `{MQTT_BASE_TOPIC}/status` | `online` / `offline` (LWT) |
| `{MQTT_BASE_TOPIC}/calibration/dry` | Trocken-Kalibrierung |
| `{MQTT_BASE_TOPIC}/calibration/wet` | Nass-Kalibrierung |
| `{MQTT_BASE_TOPIC}/measure/cmd` | `MEASURE` → Sofortmessung |

## Build

```bash
pio run -e d1_mini          # ESP8266 D1 Mini
pio run -e esp32c3          # ESP32-C3 (GPIO4 = ADC)
pio run -t upload           # USB-Flash (default_env)
pio device monitor          # Serial-Log (115200)
./scripts/build-webflash.sh # Firmware für webflash/
```

### ESP32-C3

- Board-Defaults: `include/config.esp32c3.defaults.h` (aktiv mit `-DCONFIG_BOARD_ESP32C3`)
- Optional: `include/config.esp32c3.h.example` → `config.esp32c3.h`
- ADC: 12 Bit (0–4095), Standard-Kalibrierung trocken/nass: 3200 / 1200
- AP-Name: `C3Soil-XXXX`

## Kalibrierung

- **Trocken** = hoher ADC-Wert (Sonde in Luft)
- **Nass** = niedriger ADC-Wert (nur Sonde in Wasser)
- Werte werden im EEPROM gespeichert und überleben Neustarts
