# Camper Tool

Eine benutzerfreundliche, performante Steuerung und Überwachung für Campervans und Wohnmobile auf Basis von ESP32-Mikrocontrollern. Das **Camper Tool** erfasst wichtige Sensordaten (z. B. Tankstände, Gassensoren, Temperaturen, Batteriewerte) und stellt diese über ein modernes Web-Interface sowie MQTT / Home Assistant zur Verfügung.

## 🛠️ Funktionen

* **Wasser- & Tankpegelüberwachung:**

  * Frisch- und Grauwassermessung über ADC (z. B. Votronic Tankelektroden oder kapazitive Votronic FL-Sensoren).

  * Automatische Pegel-Interpolation über anpassbare Kalibrierungs-Matrizen.

* **Gas-Füllstandsmessung:**

  * BLE-Scanning und Auswertung von Bluetooth-Gassensoren (z. B. Mopeka / Gasbank).

  * Konfigurierbarer Füllstopp.

* **Sensoren & Aktorsteuerung:**

  * Messung von Temperaturen/Luftfeuchtigkeit (z. B. DS18B20, DHT11/22) und Spannungsmessung (Starter- & Aufbaubatterie).

  * Schalten von Verbrauchern (z. B. Wasserpumpe, Standheizung, Beleuchtung) über Relais oder MOSFETs.

* **Konnektivität & Integration:**

  * Web-Interface (responsive HTML/JS UI, komprimiert per LittleFS).

  * WLAN-Manager mit automatischem Access-Point-Fallback.

  * MQTT-Anbindung inklusive **Home Assistant Auto-Discovery**.

  * Unterstützung für OTA-Updates (Over-The-Air).

## 📂 Projektstruktur

```
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

```

### Wichtigste Dateien im Überblick

| Datei | Inhalt & Funktion | 
| ----- | ----- | 
| `config.h` | Hardware-Pinbelegungen, Standardwerte, Sensor-Schwellenwerte & Gas-Byte-Zuordnungen | 
| `water_sensor.cpp` | Messung & Aufbereitung der ADC-Werte für Frisch- und Grauwasser (Gleitender Mittelwert & Matrix) | 
| `gas_sensor.cpp` | NimBLE-Scan Routinen, Dekodierung von Bluetooth-Gassensoren, Gerätelisten | 
| `mqtt_ha.cpp` | MQTT-Verbindung, Telemetrie-Publishing und Home Assistant Auto-Discovery | 
| `webserver.cpp` | Async-Webserver, Auslieferung der gzipped Web-UI, REST-API Endpunkte | 
| `wifi_manager.cpp` | WLAN-Verbindungsaufbau, AP-Fallback und Modem-Sleep für stabile WiFi/BLE-Koexistenz | 

## 🚀 Hardware & Pinbelegung

Das System unterstützt den **ESP32-C3 Super Mini** (Standard), den **ESP32-S3** oder den **klassischen ESP32**. Die Pinbelegung wird je nach gewähltem Chip-Typ automatisch zugewiesen.

### Standard-Pinbelegung

| Sensor / Funktion | ESP32-C3 | ESP32-S3 | Klassischer ESP32 | Parameter in `config.h` | 
| ----- | ----- | ----- | ----- | ----- | 
| **Frischwasser** (Votronic) | GPIO0 | GPIO1 | GPIO34 | `ADC_PIN` | 
| **Grauwasser** (Votronic, opt.) | GPIO1 | GPIO2 | GPIO35 | `GREY_ADC_PIN` | 
| **Gas** (BLE) | *Bluetooth* | *Bluetooth* | *Bluetooth* | Eingebautes Bluetooth | 
| **Relais / Aktoren** | Pin frei wählbar | Pin frei wählbar | Pin frei wählbar | In `config.h` definierbar | 

#### Wichtige Hardware-Hinweise:

* **ADC1-Einschränkung:** Beide Analogeingänge für die Tanks müssen zwingend an **ADC1-Pins** angebunden sein. ADC2 kann nicht genutzt werden, solange das WLAN-Modul aktiv ist.

* **Boot-Pins beachten:**

  * GPIO2 beim ESP32-C3 **nicht** belegen (Strapping-/Boot-Pin).

  * Beim ESP32-S3 ist GPIO0 ein Boot-Pin ohne ADC-Funktion, weshalb dort GPIO1/GPIO2 genutzt werden.

* **Leitungsfilter & Schutz:** Vor dem Anschluss die Signalspannung der Tankgeber messen (darf max. \~2,5 V - 3,3 V betragen, sonst Spannungsteiler nutzen). Bei langen Leitungen empfiehlt sich ein abgeschirmtes Kabel.

### Grauwasser-Gebertypen

Im Web-Interface (Tab *Grau* unter „Gebertyp“) konfigurierbar (Werkseinstellung in `config.h`: `GREY_SENSOR_TYPE`):

| Gebertyp | Kalibrierung | Eigenschaften | 
| ----- | ----- | ----- | 
| **Votronic Tankelektrode** | Multi-Punkt Matrix | Analoge Signalverarbeitung wie beim Frischwassertank. | 
| **Votronic Tank-Sensor FL** | Kapazitiv / Linear | Robust gegen Ablagerungen im Abwasser. Kalibrierung über „Leer übernehmen“ / „Voll übernehmen“. | 

## 💻 Software & Entwicklungsumgebung

### Software-Voraussetzungen

* [Arduino IDE](https://www.arduino.cc/en/software) (Version 2.2.1 oder neuer empfohlen) oder [PlatformIO](https://platformio.org/)

* **Board-Paket:** `esp32` von Espressif (Version 3.x, getestet mit `3.3.9`)

### Benötigte Bibliotheken (über den Bibliotheksverwalter)

| Library | Autor | Getestete Version | 
| ----- | ----- | ----- | 
| **PubSubClient** | Nick O'Leary | `2.8` | 
| **ArduinoJson** | Benoit Blanchon | `7.4.2` | 
| **ESP Async WebServer** | ESP32Async | `3.12.1` | 
| **Async TCP** | ESP32Async | `3.5.0` | 
| **NimBLE-Arduino** | h2zero | `2.5.1` | 
| **OneWire** *(optional)* | Paul Stoffregen | Standard | 
| **DallasTemperature** *(optional)* | Miles Burton | Standard | 

## ⚙️ Vorbereitung der Arduino IDE

### 1. Lokale Konfiguration anlegen

1. Navigiere in den Sketch-Ordner `_arduino/camper_tool/`.

2. Kopiere die Datei `config_local.h.example` und benenne die Kopie um in `config_local.h`.

3. Trage dort deine WLAN-Zugangsdaten, Passwörter und MQTT-Broker-Daten ein.
   *(Diese Datei wird von Git ignoriert und verhindert das versehentliche Hochladen von Zugangsdaten).*

### 2. Board-Einstellungen in der Arduino IDE

Wähle dein angeschlossenes Board unter `Werkzeuge` -> `Board` aus und passe folgende Optionen an:

| Einstellung | Wert | Wichtiger Hinweis | 
| ----- | ----- | ----- | 
| **Board** | `ESP32C3 Dev Module` / `ESP32S3 Dev Module` / `ESP32 Dev Module` | Je nach Chip. | 
| **Partition Scheme** | ⚠️ **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** | **Zwingend erforderlich!** (WiFi + BLE benötigen \~1,43 MB Flash). | 
| **USB CDC On Boot** | `Enabled` | Nur aktivieren, wenn der Serial Monitor über den nativen USB-Port läuft. | 
| **PSRAM** | Passend zum Board (z.B. `OPI PSRAM`) | Falsche PSRAM-Einstellungen führen zum Bootloop. | 

## 🔌 LittleFS-Plugin installieren (Arduino IDE 2.x)

Um die HTML-Dateien des Web-Interfaces in den Flash-Speicher des ESP32 zu laden, wird das Plugin `arduino-littlefs-upload` benötigt.

1. Lade von GitHub unter Releases die Datei `arduino-littlefs-upload-x.y.z.vsix` herunter (nicht entpacken).

2. Erstelle den Plugin-Ordner (falls nicht vorhanden) und verschiebe die `.vsix`-Datei hinein:

   * **Windows:** `C:\Users\<Benutzername>\.arduinoIDE\plugins\`

   * **macOS / Linux:** `~/.arduinoIDE/plugins/`

**Beispiel macOS/Linux Terminal:**

```
mkdir -p ~/.arduinoIDE/plugins
mv ~/Downloads/arduino-littlefs-upload-*.vsix ~/.arduinoIDE/plugins/

```

3. Starte die Arduino IDE komplett neu.

4. Über `Cmd + Shift + P` (Mac) bzw. `Strg + Shift + P` (Windows/Linux) steht nun das Werkzeug **Upload LittleFS to Pico/ESP8266/ESP32** zur Verfügung.

## 🚀 Flashen & Erstinbetriebnahme

1. **Repository klonen:**

   ```
   git clone https://github.com/hflocki/camper_tool.git
   
   ```

2. **Projekt öffnen:** Öffne `_arduino/camper_tool/camper_tool.ino` in der Arduino IDE.

3. **Firmware hochladen:** Board und COM-Port auswählen, danach auf **Hochladen** drücken.

4. **LittleFS hochladen:**

   * Schließe den Seriellen Monitor (Port darf nicht blockiert sein).

   * Drücke `Cmd + Shift + P` (bzw. `Strg + Shift + P`).

   * Führe **Upload LittleFS to Pico/ESP8266/ESP32** aus.

5. **Erster Start:**

   * Nach dem Neustart öffnet der ESP32 einen Access Point namens `Camper-Tool-XXXXXX` (oder den in `config.h`/`config_local.h` eingestellten Namen).

   * Verbinde dich mit dem WLAN und rufe im Browser `http://192.168.4.1` auf, um das System zu konfigurieren.

> **Änderungen an der Web-Oberfläche:**
> Wenn die Datei `web/index.html` bearbeitet wurde, muss diese vor dem LittleFS-Upload neu komprimiert werden:
>
> ```
> cd _arduino/camper_tool
> gzip -9 -n -c web/index.html > data/index.html.gz
> 
> ```
>
> *(Bei Nutzung von PlatformIO wird dieser Schritt über `_platformio/gzip_web.py` automatisch ausgeführt).*

## 🧪 Nutzung & Wartung

* **Serial Monitor:** Bei einer Baudrate von `115200` gibt das System Boot-Informationen, IP-Adressen, BLE-Scans und Sensordaten aus.

* **Home Assistant:** Sofern MQTT aktiviert ist, erstellt das Tool via Auto-Discovery automatisch alle Sensoren, Switche und Messwerte in Home Assistant.

* **OTA-Updates:** Nach dem ersten Flashen per USB können zukünftige Updates bequem über WLAN eingespielt werden (`camper-tool` erscheint als Netzwerkkort in der Arduino IDE).

## 🤝 Beitragen

Beiträge, Ideen und Verbesserungsvorschläge sind herzlich willkommen!

1. Forke das Repository

2. Erstelle einen Feature-Branch (`git checkout -b feature/NeuesFeature`)

3. Committe deine Änderungen (`git commit -m 'Neues Feature hinzugefügt'`)

4. Pushe den Branch (`git push origin feature/NeuesFeature`)

5. Öffne einen **Pull Request**

## 📄 Lizenz

Dieses Projekt steht unter der [MIT License](LICENSE).
