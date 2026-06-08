# Prompt: ESP8266 D1 Mini Bodenfeuchte-Sensor (esp8266-d1-soil)

Erstelle ein vollständiges PlatformIO/Arduino-Firmware-Projekt unter:

```
~/Workspaces/esp8266-d1-soil/
```

Ziel: **Wemos D1 Mini (ESP8266)** + **Capacitive Soil Moisture Sensor v1.2** als MQTT-Sensor mit **Home-Assistant-Discovery**, **Web-Dashboard** (Status + Kalibrierung) und **OTA-Updates**.

Architektur und Code-Stil orientieren sich am Referenzprojekt **dc_impulse** im Gardena-Repo:

```
~/Workspaces/Gardena-WT-T-1030-ESP32-Mod/firmware/dc_impulse/
```

Dort wiederverwenden bzw. portieren (ESP8266-angepasst): `network_config`, `web_portal`, `ota_update`, `event_log`, eingebettetes Dashboard-HTML, MQTT/HA-Discovery-Muster. **Keine** Ventil-/Irrigation-Logik übernehmen.

---

## 1. Hardware

### 1.1 Komponenten

| Teil | Details |
|------|---------|
| MCU | Wemos D1 Mini (ESP8266EX) |
| Sensor | Capacitive Soil Moisture Sensor **v1.2** (analog AOUT) |
| Spannung | Sensor **nur an 3,3 V** — nie 5 V am A0-Pin |

### 1.2 Verkabelung

```
Capacitive v1.2          D1 Mini
─────────────────          ───────
VCC  ────────────────────  3,3 V
GND  ────────────────────  GND
AOUT ────────────────────  A0  (einziger ADC-Pin)
```

Optional (empfohlen, in Firmware unterstützen):

```
Sensor VCC ── GPIO14 (D5) ── MOSFET/Transistor ── 3,3 V
```

- `#define SENSOR_POWER_PIN 14` in `config.example.h`
- `#define SENSOR_POWER_ENABLED 1` — Sensor nur kurz vor Messung einschalten (Standard: an)
- Messablauf: Pin HIGH → 100 ms warten → ADC lesen → Pin LOW
- Wenn `SENSOR_POWER_ENABLED 0`: Sensor dauerhaft an 3,3 V, Pin ungenutzt

### 1.3 ADC-Verhalten

- ESP8266 A0: Eingang 0–3,3 V, intern auf 10-Bit (0–1023) gemappt
- Capacitive v1.2: **hoher ADC-Wert = trocken**, **niedriger ADC-Wert = nass** (invertiert)
- Umrechnung in Feuchte %:

```cpp
float moisturePercent(uint16_t raw, uint16_t dryAdc, uint16_t wetAdc) {
  if (dryAdc <= wetAdc) return 0.0f;
  const float pct = (static_cast<float>(dryAdc - raw) / static_cast<float>(dryAdc - wetAdc)) * 100.0f;
  return constrain(pct, 0.0f, 100.0f);
}
```

- ADC-Messung: **Median aus 5 Samples** (Ausreißer filtern), optional gleitender Mittelwert über die letzten 3 Messungen für MQTT-Publish

---

## 2. Projektstruktur

```
esp8266-d1-soil/
├── platformio.ini
├── ota_upload.ini.example
├── .gitignore
├── README.md
├── include/
│   ├── config.example.h      # Vorlage — Nutzer kopiert nach config.h
│   ├── config.h              # .gitignore — lokale Secrets
│   ├── firmware_version.h
│   ├── dashboard_html.h        # PROGMEM HTML/CSS/JS
│   ├── network_config.h
│   ├── web_portal.h
│   ├── ota_update.h
│   ├── event_log.h
│   └── soil_sensor.h
└── src/
    ├── main.cpp
    ├── network_config.cpp
    ├── web_portal.cpp
    ├── ota_update.cpp
    ├── event_log.cpp
    └── soil_sensor.cpp
```

---

## 3. PlatformIO

### 3.1 `platformio.ini`

```ini
[platformio]
default_envs = d1_mini
extra_configs =
  optional ota_upload.ini

[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
upload_protocol = esptool
monitor_filters = direct
build_flags =
  -std=gnu++17
lib_deps =
  knolleary/PubSubClient
  bblanchon/ArduinoJson

[env:d1_mini_ota]
extends = env:d1_mini
upload_protocol = espota
upload_port = 192.168.1.100
upload_flags =
  --timeout=60
```

### 3.2 `.gitignore`

```
.pio/
include/config.h
ota_upload.ini
compile_commands.json
```

### 3.3 Build-Befehle

- Erstflash: `pio run -t upload`
- OTA: `pio run -e d1_mini_ota -t upload` (nach `ota_upload.ini` aus Example)
- Monitor: `pio device monitor`

---

## 4. Konfiguration (`include/config.example.h`)

```cpp
#pragma once

// Nach include/config.h kopieren. WLAN/MQTT alternativ über Web-Dashboard.

#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"

#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""

#define DEVICE_ID "d1_soil_1"           // unique_id-Präfix, MQTT client id
#define DEVICE_NAME "Bodenfeuchte 1"    // HA-Gerätename
#define MQTT_BASE_TOPIC "d1_soil/1"
#define WIFI_HOSTNAME "D1Soil1"
#define MQTT_DISCOVERY_PREFIX "homeassistant"

#define OTA_PASSWORD ""               // leer = kein Passwort (nur LAN)

// Hardware
#define SENSOR_ADC_PIN A0
#define SENSOR_POWER_PIN 14             // D5, -1 = nicht verwendet
#define SENSOR_POWER_ENABLED 1

// Messintervall
#define MEASURE_INTERVAL_MS 30000       // MQTT-Publish-Intervall
#define MEASURE_WARMUP_MS 100           // Wartezeit nach Sensor-Power-On

// Default-Kalibrierung (wird in NVS überschrieben)
#define DEFAULT_DRY_ADC 800             // in Luft
#define DEFAULT_WET_ADC 300             // in Wasser (nur Sonde)

// Sensor-Name im Web/HA (editierbar, NVS)
#define DEFAULT_SENSOR_LABEL "Bodenfeuchte"
```

`main.cpp` und andere Module: Defaults per `#ifndef` / `#if __has_include("config.h")` wie in dc_impulse.

---

## 5. Persistenz (NVS / Preferences)

### 5.1 Netzwerk — Namespace `d1_soil_net`

Portiert von dc_impulse `network_config.cpp`, aber **eigener Namespace** (nicht `gardena_net`):

| Key | Typ | Inhalt |
|-----|-----|--------|
| `wifi_ok` | bool | WLAN konfiguriert |
| `wifi_ssid` | string | |
| `wifi_pass` | string | |
| `mqtt_host` | string | |
| `mqtt_port` | ushort | |
| `mqtt_user` | string | |
| `mqtt_pass` | string | |

API unverändert:

```cpp
struct NetworkSettings { ... };
void networkConfigLoad(NetworkSettings &settings, const NetworkSettings &defaults);
void networkConfigSave(const NetworkSettings &settings);
bool networkConfigHasStoredWifi();
```

### 5.2 Kalibrierung — Namespace `d1_soil_cal`

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `dry_adc` | uint16 | 800 | Kalibrierung trocken |
| `wet_adc` | uint16 | 300 | Kalibrierung nass |
| `label` | string | „Bodenfeuchte" | Anzeigename (Web + HA) |

```cpp
struct SoilCalibration {
  uint16_t dryAdc = DEFAULT_DRY_ADC;
  uint16_t wetAdc = DEFAULT_WET_ADC;
  String label = DEFAULT_SENSOR_LABEL;
};
void soilCalLoad(SoilCalibration &cal, const SoilCalibration &defaults);
void soilCalSave(const SoilCalibration &cal);
```

Nach Kalibrierungsänderung: MQTT-Werte neu publizieren + HA-Discovery mit neuem Namen aktualisieren.

---

## 6. Modul `soil_sensor`

### 6.1 API

```cpp
struct SoilReading {
  uint16_t rawAdc;
  float moisturePercent;
  uint32_t measuredAtMs;
  bool valid;
};

void soilSensorBegin();
SoilReading soilSensorMeasure();          // synchron, blockiert ~200 ms
const SoilReading &soilSensorLastReading();
uint32_t soilSensorLastMeasureMs();
void soilSensorSetCalibration(const SoilCalibration &cal);
const SoilCalibration &soilSensorCalibration();
```

### 6.2 Messlogik

1. Optional: `SENSOR_POWER_PIN` HIGH
2. `delay(MEASURE_WARMUP_MS)`
3. 5× `analogRead(A0)`, Median bilden
4. Optional: Pin LOW
5. Feuchte % aus Kalibrierung berechnen
6. Ergebnis cachen

---

## 7. MQTT & Home Assistant

### 7.1 Client

- `PubSubClient` über `WiFiClient`
- Client-ID: `DEVICE_ID`
- LWT auf `{MQTT_BASE_TOPIC}/status` → Payload `offline` (retain), bei Connect `online`
- Reconnect mit Backoff wie dc_impulse
- `webPortalConnectMqtt()` / `webPortalSetMqttConnected()` Pattern beibehalten

### 7.2 Topics

| Topic | Richtung | Payload | retain |
|-------|----------|---------|--------|
| `{MQTT_BASE_TOPIC}/status` | pub | `online` / `offline` | ja |
| `{MQTT_BASE_TOPIC}/moisture` | pub | `42.5` (1 Dezimalstelle) | ja |
| `{MQTT_BASE_TOPIC}/moisture_raw` | pub | `512` | nein |
| `{MQTT_BASE_TOPIC}/calibration/dry/set` | sub | `800` | — |
| `{MQTT_BASE_TOPIC}/calibration/wet/set` | sub | `300` | — |
| `{MQTT_BASE_TOPIC}/calibration/dry` | pub | aktueller Wert | ja |
| `{MQTT_BASE_TOPIC}/calibration/wet` | pub | aktueller Wert | ja |
| `{MQTT_BASE_TOPIC}/measure/cmd` | sub | `MEASURE` → Sofortmessung | — |

Publish-Intervall: `MEASURE_INTERVAL_MS`, plus Sofortmessung auf MQTT-Command und nach Kalibrierung.

### 7.3 Home-Assistant MQTT Discovery

Discovery-Prefix: `MQTT_DISCOVERY_PREFIX` (default `homeassistant`).

**Sensor Feuchte:**

```
Topic: homeassistant/sensor/{DEVICE_ID}_moisture/config
```

```json
{
  "name": "{label}",
  "unique_id": "{DEVICE_ID}_moisture",
  "state_topic": "{MQTT_BASE_TOPIC}/moisture",
  "unit_of_measurement": "%",
  "device_class": "moisture",
  "state_class": "measurement",
  "icon": "mdi:water-percent",
  "availability_topic": "{MQTT_BASE_TOPIC}/status",
  "device": {
    "identifiers": ["{DEVICE_ID}"],
    "name": "{DEVICE_NAME}",
    "manufacturer": "DIY",
    "model": "ESP8266 D1 Soil Sensor"
  }
}
```

**Sensor Roh-ADC (diagnostisch, in HA standardmäßig deaktiviert):**

```
homeassistant/sensor/{DEVICE_ID}_moisture_raw/config
```

`entity_category`: `"diagnostic"`, kein `device_class`, state_topic: `.../moisture_raw`.

**Number-Entities für Kalibrierung (optional, aber erwünscht):**

- `{DEVICE_ID}_cal_dry` → command/state `{MQTT_BASE_TOPIC}/calibration/dry`
- `{DEVICE_ID}_cal_wet` → command/state `{MQTT_BASE_TOPIC}/calibration/wet`
- min/max: 0–1023, step 1, mode `box`

Discovery bei MQTT-Connect einmal publizieren (retain). Bei Label-Änderung Discovery neu senden.

---

## 8. Web-Portal

Portiert von dc_impulse, ESP8266-Libraries:

- `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`, `DNSServer`, `ArduinoOTA`

### 8.1 WLAN-Provisioning

- Kein gespeichertes WLAN → **AP-Modus**: SSID `D1Soil-XXXX` (letzte 4 Hex der MAC)
- Captive DNS (`DNSServer`) für Setup
- Nach POST `/api/config` → NVS speichern → Reboot
- Station-Modus: mDNS `{WIFI_HOSTNAME}.local`, Port 80

### 8.2 REST-API

| Methode | Pfad | Beschreibung |
|---------|------|--------------|
| GET | `/` | Dashboard HTML (PROGMEM) |
| GET | `/api/status` | Gerätestatus (s. u.) |
| GET | `/api/config` | Netzwerk (Passwörter nur `*_set: true`) |
| POST | `/api/config` | Netzwerk speichern + Reboot |
| GET | `/api/wifi/scan` | WLAN-Scan |
| POST | `/api/restart` | Neustart |
| GET | `/api/soil` | Aktuelle Messung + Kalibrierung |
| POST | `/api/soil/measure` | Sofortmessung |
| POST | `/api/soil/calibration` | `{ "dry_adc": 800, "wet_adc": 300, "label": "..." }` |
| POST | `/api/soil/calibrate/dry` | Aktuellen ADC als trocken speichern |
| POST | `/api/soil/calibrate/wet` | Aktuellen ADC als nass speichern |
| POST | `/api/firmware` | `.bin`-Upload (wie dc_impulse) |

**`/api/status` JSON-Felder:**

```json
{
  "device_name": "Bodenfeuchte 1",
  "firmware_version": "2026-06-08 v 0.1",
  "firmware_version_date": "2026-06-08",
  "firmware_version_number": "0.1",
  "ap_mode": false,
  "ap_ssid": "",
  "wifi_connected": true,
  "mqtt_connected": true,
  "ip": "192.168.1.42",
  "hostname": "D1Soil1",
  "mdns": "D1Soil1.local",
  "uptime_sec": 3600,
  "rssi": -65,
  "arduino_ota_active": true,
  "arduino_ota_port": 8266,
  "chip_model": "ESP8266",
  "mac": "AA:BB:CC:DD:EE:FF",
  "moisture_percent": 42.5,
  "moisture_raw": 512,
  "cal_dry_adc": 800,
  "cal_wet_adc": 300,
  "sensor_label": "Bodenfeuchte",
  "last_measure_sec_ago": 12
}
```

### 8.3 Dashboard-UI (`dashboard_html.h`)

Design wie dc_impulse: system-ui, light/dark, mobile-first, deutsche Texte, keine externen CDN-Abhängigkeiten.

**Tabs:**

1. **Status** (Standard-Tab)
   - Große Anzeige: Feuchte % (z. B. „42 %")
   - Fortschrittsbalken 0–100 % (grün → gelb → rot)
   - Roh-ADC, letzte Messung (vor X s), Kalibrierungswerte
   - Button „Jetzt messen" → POST `/api/soil/measure`
   - Auto-Refresh alle 5 s

2. **Kalibrierung**
   - Erklärtext (kurz): trocken = in Luft, nass = Sonde in Wasser
   - Live-Rohwert (aktualisiert bei Refresh)
   - Buttons: **„Aktuellen Wert als TROCKEN speichern"** / **„… als NASS speichern"**
   - Manuelle Eingabe: dry_adc, wet_adc (number inputs)
   - Sensor-Label (Text)
   - Button „Kalibrierung speichern"
   - Vorschau: berechnete Feuchte % aus aktuellem Rohwert + Kalibrierung

3. **Einstellungen**
   - WLAN + MQTT (wie dc_impulse)
   - WLAN-Scan, Speichern & Neustart

4. **Firmware**
   - Versionsanzeige
   - `.bin`-Upload via Web
   - Hinweis: `pio run -e d1_mini_ota -t upload`

Header-Zeile: `WLAN 192.168.x.x · MQTT · Uptime`

AP-Hinweis-Banner wenn im Setup-Modus.

---

## 9. OTA

- **ArduinoOTA** auf Port **8266** (ESP8266-Default, nicht 3232)
- `ota_update.cpp` portieren: `otaUpdateOnWifiConnected()`, `otaUpdateLoop()`
- mDNS: `MDNS.addService("arduino", "tcp", 8266)` bzw. `ArduinoOTA.setMdnsEnabled(true)` — nur einmal `MDNS.begin()`
- PlatformIO-Env `d1_mini_ota` mit `upload_protocol = espota`
- Web-Firmware-Upload zusätzlich ( wie dc_impulse `/api/firmware`)

---

## 10. Logging (`event_log`)

Minimal wie dc_impulse — Ausgabe auf `Serial` (115200):

```cpp
logSys / logSysf    // [SYS]
logWeb / logWebf    // [WEB]
logMqtt / logMqttf  // [MQTT]
```

Kein Ringbuffer/Persistenz nötig.

---

## 11. `main.cpp` — Ablauf

```
setup():
  Serial.begin(115200)
  event_log / soil_sensor / network_config laden
  webPortalBegin(defaults)
  (kein blocking wait — alles non-blocking in loop)

loop():
  webPortalLoop()          // WiFi, HTTP, DNS, mDNS
  otaUpdateLoop()
  mqttLoop()               // connect, subscribe, discovery, publish
  soilMeasureTask()        // non-blocking: wenn Intervall abgelaufen → messen
```

MQTT-Loop: bei `webPortalRequestMqttReconnect()` Flag → disconnect + reconnect.

---

## 12. README.md (Deutsch)

Kurze Anleitung:

1. `config.example.h` → `config.h`
2. Verkabelung (3,3 V!)
3. `pio run -t upload`
4. AP `D1Soil-XXXX` oder Dashboard unter `http://d1soil1.local/`
5. WLAN/MQTT konfigurieren
6. Kalibrierung: trocken/nass einstellen
7. Home Assistant: MQTT Discovery automatisch
8. OTA-Update

---

## 13. Qualitätsanforderungen

- [ ] Kompiliert fehlerfrei: `pio run -e d1_mini`
- [ ] Keine Secrets in Git (`config.h` gitignored)
- [ ] Web-Dashboard funktioniert in AP- und STA-Modus
- [ ] Kalibrierung überlebt Reboot (NVS)
- [ ] MQTT Discovery erzeugt `moisture`-Sensor in HA
- [ ] Feuchte % plausibel (0 = trocken, 100 = nass)
- [ ] Sensor-Power-Puls spart Strom (wenn aktiviert)
- [ ] Code kommentiert auf Deutsch (nur wo nötig)
- [ ] Keine Cursor-/KI-Meta-Kommentare
- [ ] Commit-Message-Vorschlag auf Deutsch

---

## 14. Explizit NICHT im Scope

- Mehrere Sensoren / Bus
- Deep Sleep (spätere Erweiterung)
- Ventilsteuerung / Bewässerungslogik
- TLS/MQTT over SSL
- ESP32-Code oder `-DCONFIG_ESP32_*` Flags
- Abhängigkeit von dc_impulse als Submodule — Code **kopieren und anpassen**, nicht importieren

---

## 15. Referenzdateien ( zum Portieren )

Aus dc_impulse diese Dateien als Vorlage lesen und ESP8266-anpassen:

| Datei | Anpassungen |
|-------|-------------|
| `src/network_config.cpp` | Namespace `d1_soil_net` |
| `src/web_portal.cpp` | ESP8266-Header, Soil-API statt Zone-API, AP-Name `D1Soil-` |
| `src/ota_update.cpp` | Port 8266, ESP8266mDNS |
| `src/event_log.cpp` | 1:1 |
| `include/dashboard_html.h` | Neues UI (4 Tabs, s. Abschnitt 8.3) |
| `src/main.cpp` | Nur MQTT/Soil, keine Ventile |

---

## 16. Erste Version

`firmware_version.h`:

```cpp
#define FIRMWARE_VERSION_DATE "2026-06-08"
#define FIRMWARE_VERSION "0.1"
#define FIRMWARE_VERSION_LABEL FIRMWARE_VERSION_DATE " v " FIRMWARE_VERSION
```

Beginne mit **funktionierendem MVP** (Messung + Web + MQTT + Kalibrierung). Keine vorzeitige Optimierung.
