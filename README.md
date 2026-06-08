# ESP8266 D1 Mini Bodenfeuchte-Sensor

Firmware für **Wemos D1 Mini (ESP8266)** mit **Capacitive Soil Moisture Sensor v1.2**: MQTT-Sensor mit Home-Assistant-Discovery, Web-Dashboard (Status + Kalibrierung) und OTA-Updates.

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
pio run -e d1_mini          # Kompilieren
pio run -t upload           # USB-Flash
pio device monitor          # Serial-Log (115200)
```

## Kalibrierung

- **Trocken** = hoher ADC-Wert (Sonde in Luft)
- **Nass** = niedriger ADC-Wert (nur Sonde in Wasser)
- Werte werden im EEPROM gespeichert und überleben Neustarts
