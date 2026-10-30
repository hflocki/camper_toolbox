# Camper Tool

Eine benutzerfreundliche, performante Steuerung und Überwachung für Campervans und Wohnmobile auf Basis von ESP32-Mikrocontrollern. Das **Camper Tool** erfasst wichtige Sensordaten (z. B. Tankstände, Gassensoren, Temperaturen, Batteriewerte) und stellt diese über ein modernes Web-Interface sowie MQTT / Home Assistant zur Verfügung.

---

## 🛠️ Funktionen

- **Wasser- & Tankpegelüberwachung:**
  * Frisch- und Grauwassermessung über ADC (z. B. Votronic Tankelektroden oder kapazitive Votronic FL-Sensoren).
  * Automatische Pegel-Interpolation über anpassbare Kalibrierungs-Matrizen.
- **Gas-Füllstandsmessung:**
  * BLE-Scanning und Auswertung von Bluetooth-Gassensoren (z. B. Mopeka / Gasbank).
  * Konfigurierbarer Füllstopp.
- **Sensoren & Aktorsteuerung:**
  * Messung von Temperaturen/Luftfeuchtigkeit (z. B. DS18B20, DHT11/22) und Spannungsmessung (Starter- & Aufbaubatterie).
  * Schalten von Verbrauchern (z. B. Wasserpumpe, Standheizung, Beleuchtung) über Relais oder MOSFETs.
- **Konnektivität & Integration:**
  * Web-Interface (responsive HTML/JS UI, komprimiert per LittleFS).
  * WLAN-Manager mit automatischem Access-Point-Fallback.
  * MQTT-Anbindung inklusive **Home Assistant Auto-Discovery**.
  * Unterstützung für OTA-Updates (Over-The-Air).

---

## 📂 Projektstruktur

```text
camper_tool/
├── _arduino/
│   └── camper_tool/
│       ├── camper_tool.ino        # Hauptskript / Haupteinstiegspunkt
│       ├── config.h               # Einstellungen, Pins, Schwellenwerte, Gas-Byte-Zuordnung
│       ├── config_local.h.example # Vorlage für private Zugangsdaten (WLAN/MQTT)
│       ├── webserver.cpp / .h     # Web-UI (gzip), REST-API, LittleFS-Handling
│       ├── water_sensor.cpp / .h  # ADC für Frisch-/Grauwasser, Mittelwert, Matrix-Interpolation
│       ├── gas_sensor.cpp / .h    # NimBLE-Scan, Dekodierung, Füllstopp
│       ├── mqtt_ha.cpp / .h       # MQTT-Client & Home Assistant Discovery
│       ├── wifi_manager.cpp / .h  # WLAN-Verwaltung & AP-Fallback (WiFi/BLE Coex)
│       ├── web/
│       │   └── index.html         # Quelle der Web-Oberfläche (Unkomprimiert)
│       └── data/
│           └── index.html.gz      # Komprimierte Web-UI für LittleFS
└── _platformio/                   # PlatformIO-Umgebung (platformio.ini & Build-Skripte)
