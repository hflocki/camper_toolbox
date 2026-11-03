> wird hier fortgeführt zukünftig
> https://github.com/hflocki/camper-mqtt/tree/main/firmware/toolbox


> [!WARNING]
> **IN ENTWICKLUNG – kein fertiges Projekt.** Wegen teils mangelnder Hardware nicht selber getestet.

# Camper Toolbox

Ein ESP32 als zentrales **Bluetooth-Gateway** für Campervans und Wohnmobile. Die Camper Toolbox liest Bluetooth-Geräte aus bzw. steuert sie und stellt alles über ein Web-Interface sowie MQTT / Home Assistant (Auto-Discovery) zur Verfügung. Es wird keine zusätzliche Hardware verdrahtet – nur ein ESP32.

## 🛠️ Funktionen

Die Gerätetreiber sind Plugins. Welche davon in die Firmware kommen, legst du in `config.h` bzw. `config_local.h` fest (Zahl = Anzahl der Geräte, `0` = nicht einkompiliert):

| Plugin-Schalter     | Gerät                                     | Bluetooth                | Einstellungen in der Web-UI          |
| ------------------- | ----------------------------------------- | ------------------------ | ------------------------------------ |
| `PLUGIN_DIMES`      | Gas: Rotarex DIMES / SRG WAVE             | passiv (nur mithören)    | MAC, Flaschengröße, Füllstopp        |
| `PLUGIN_MOPEKA_PRO` | Gas: Mopeka Pro (Pro Check, Universal …)  | passiv                   | MAC, Leer-/Voll-Höhe                 |
| `PLUGIN_MOPEKA_STD` | Gas: Mopeka Standard / Std Check          | passiv                   | MAC, Leer-/Voll-Höhe, Propan-Anteil  |
| `PLUGIN_CARAWATER`  | Wasserpumpe CaraWater (ISP 15)            | Verbindung               | MAC                                  |
| `PLUGIN_RVTECH`     | Dachhaube RV Tech (Heki-Motorsteuerung)   | Verbindung, ggf. mit PIN | MAC, PIN                             |
| `PLUGIN_WCS_FRIDGE` | WCS Kompressor-Kühlschrank                | Verbindung, ggf. mit PIN | MAC, PIN, optional UUIDs / Abfrage   |

- Passive Sensoren: beliebig viele. Geräte mit Verbindung: höchstens **3** pro ESP32.
- Web-Interface mit Live-Werten, Geräte-Suche („Suchen“) und Rohdaten-Ansicht.
- WLAN-Manager mit Access-Point-Fallback, MQTT mit **Home Assistant Auto-Discovery**, OTA-Updates.

## 📶 Bluetooth-Geräte einrichten

**MAC-Adresse und PIN werden in der Web-UI eingetragen**, im Einstellungsbereich des jeweiligen Plugins – **nicht** in `config_local.h`. Ein `#define RVTECH_PIN …` o. Ä. hat keine Wirkung.

1. Plugin in `config_local.h` aktivieren (z. B. `#define PLUGIN_RVTECH 1`) und flashen.
2. Web-UI öffnen → Plugin → **„Suchen“** → Gerät aus der Liste wählen (übernimmt die MAC).
3. Falls das Gerät eine **PIN** hat (z. B. Dachhaube, Kühlschrank): PIN eintragen (1–6 Ziffern, auch `000000`). Die PIN ist dieselbe wie in der Hersteller-App.
4. **Handy-App trennen** (App schließen oder Bluetooth am Handy aus). Die Geräte erlauben nur eine Verbindung gleichzeitig. Später kann das Handy über den Schalter **„Bluetooth für App freigeben“** wieder ran.

Der **Status** in der Web-UI zeigt, wo es hakt:

| Status                    | Bedeutung / was tun                                                                                                  |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| `Nicht erreichbar`        | Gerät nicht in Reichweite oder noch mit dem Handy verbunden.                                                         |
| `Kopplung fehlgeschlagen` | PIN falsch – oder das Gerät ist noch an ein Handy gekoppelt. PIN prüfen; ggf. die Kopplung im Gerät/App zurücksetzen. |
| `PIN nötig?`              | Das Gerät verlangt eine Kopplung, aber es ist keine (oder eine falsche) PIN eingetragen.                             |
| `Falsches Gerät?`         | Die erwarteten Bluetooth-Dienste wurden nicht gefunden – andere MAC gewählt?                                         |
| `Warte auf Daten`         | Verbunden, aber noch keine Werte. Kühlschrank: „Letzte Nachricht“ und „Abonniert“ ansehen, ggf. Abfragebefehl setzen. |
| `Verbunden`               | Alles in Ordnung.                                                                                                    |

Gibt es Probleme, zeigt die Live-Ansicht zusätzlich **„Letzter BT-Fehler“** (z. B. „PIN falsch“, „Vom Gerät getrennt“, „Keine Daten erhalten“).

### Fehlersuche per Serial Monitor

Für eine Fehlermeldung bitte den Serial Monitor (115200 Baud) vom Einschalten an mitschneiden, bis das Problem auftritt (beim Kühlschrank mindestens 3 Minuten). Dort steht pro Gerät u. a.:

- `Connecting to … (type …)` / `Reason 0x…: …` – Verbindungsaufbau und Grund jeder Trennung
- `Security: encrypted …, authenticated …, bonded …` – Ergebnis der Kopplung mit PIN
- `GATT table:` – alle Dienste und Characteristics des Geräts mit ihren Eigenschaften
- `Subscribed …` / `Subscribe … FAILED` – welche Characteristics Daten liefern dürfen
- `RX … bytes: …` – die ersten 20 empfangenen Pakete jeder Verbindung als Hex und Text

Den Log als Text (nicht als Foto) an den Entwickler schicken. MAC-Adressen dürfen geschwärzt werden.

## 📂 Projektstruktur

```
camper_toolbox/
├── .github/workflows/release.yml  # Baut die Firmware und legt bei einem Tag ein Release an
├── _arduino/camper_toolbox/
│   ├── camper_toolbox.ino         # Einstiegspunkt
│   ├── config.h                   # Plugin-Auswahl, Standardwerte, BLE-Parameter
│   ├── config_local.h.example     # Vorlage für eigene Einstellungen (nicht in Git)
│   ├── src/core/                  # WLAN, MQTT/HA, Webserver, Einstellungen, BLE-Scan, BLE-Verbindung
│   ├── src/plugins/               # Gerätetreiber (DIMES, Mopeka, CaraWater, RV Tech, WCS)
│   ├── web/index.html             # Quelle der Web-Oberfläche
│   └── data/index.html.gz         # Komprimierte Web-UI für LittleFS
└── doc/                           # Screenshots
```

## 🚀 Hardware

Unterstützt werden **ESP32-C3** (z. B. Super Mini), **ESP32-S3** und der **klassische ESP32**. Es wird nichts angeschlossen – alle Geräte laufen über das eingebaute Bluetooth. Stromversorgung per USB oder 5-V-Wandler.

## 💻 Software & Entwicklungsumgebung

### Software-Voraussetzungen

- [Arduino IDE](https://www.arduino.cc/en/software) (Version 2.2.1 oder neuer empfohlen)

- **Board-Paket:** `esp32` von Espressif (Version 3.x, getestet mit `3.3.9`)

### Benötigte Bibliotheken (über den Bibliotheksverwalter)

| Library                            | Autor           | Getestete Version |
| ---------------------------------- | --------------- | ----------------- |
| **PubSubClient**                   | Nick O'Leary    | `2.8`             |
| **ArduinoJson**                    | Benoit Blanchon | `7.4.2`           |
| **ESP Async WebServer**            | ESP32Async      | `3.12.1`          |
| **Async TCP**                      | ESP32Async      | `3.5.0`           |
| **NimBLE-Arduino**                 | h2zero          | `2.5.1`           |

## ⚙️ Vorbereitung der Arduino IDE

### 1. Lokale Konfiguration anlegen

1. Navigiere in den Sketch-Ordner `_arduino/camper_toolbox/`.

2. Kopiere die Datei `config_local.h.example` und benenne die Kopie um in `config_local.h`.

3. Trage dort deine WLAN-Zugangsdaten, Passwörter und MQTT-Broker-Daten ein.
   _(Diese Datei wird von Git ignoriert und verhindert das versehentliche Hochladen von Zugangsdaten)._

### 2. Board-Einstellungen in der Arduino IDE

Wähle dein angeschlossenes Board unter `Werkzeuge` -> `Board` aus und passe folgende Optionen an:

| Einstellung          | Wert                                                             | Wichtiger Hinweis                                                        |
| -------------------- | ---------------------------------------------------------------- | ------------------------------------------------------------------------ |
| **Board**            | `ESP32C3 Dev Module` / `ESP32S3 Dev Module` / `ESP32 Dev Module` | Je nach Chip.                                                            |
| **Partition Scheme** | ⚠️ **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**          | **Zwingend erforderlich!** (WiFi + BLE benötigen \~1,43 MB Flash).       |
| **USB CDC On Boot**  | `Enabled`                                                        | Nur aktivieren, wenn der Serial Monitor über den nativen USB-Port läuft. |
| **PSRAM**            | Passend zum Board (z.B. `OPI PSRAM`)                             | Falsche PSRAM-Einstellungen führen zum Bootloop.                         |

## 🔌 LittleFS-Plugin installieren (Arduino IDE 2.x)

Um die HTML-Dateien des Web-Interfaces in den Flash-Speicher des ESP32 zu laden, wird das Plugin `arduino-littlefs-upload` benötigt.

1. Lade von GitHub unter Releases die Datei `arduino-littlefs-upload-x.y.z.vsix` herunter (nicht entpacken).

2. Erstelle den Plugin-Ordner (falls nicht vorhanden) und verschiebe die `.vsix`-Datei hinein:
   - **Windows:** `C:\Users\<Benutzername>\.arduinoIDE\plugins\`

   - **macOS / Linux:** `~/.arduinoIDE/plugins/`

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
   git clone https://github.com/hflocki/camper_toolbox.git

   ```

2. **Projekt öffnen:** Öffne `_arduino/camper_toolbox/camper_toolbox.ino` in der Arduino IDE.

3. **Firmware hochladen:** Board und COM-Port auswählen, danach auf **Hochladen** drücken.

4. **LittleFS hochladen:**
   - Schließe den Seriellen Monitor (Port darf nicht blockiert sein).

   - Drücke `Cmd + Shift + P` (bzw. `Strg + Shift + P`).

   - Führe **Upload LittleFS to Pico/ESP8266/ESP32** aus.

5. **Erster Start:**
   - Nach dem Neustart öffnet der ESP32 einen Access Point namens `Camper-Toolbox-XXXXXX` (oder den in `config.h`/`config_local.h` eingestellten Namen).

   - Verbinde dich mit dem WLAN und rufe im Browser `http://192.168.4.1` auf, um das System zu konfigurieren.

> **Änderungen an der Web-Oberfläche:**
> Wenn die Datei `web/index.html` bearbeitet wurde, muss diese vor dem LittleFS-Upload neu komprimiert werden:
>
> ```
> cd _arduino/camper_toolbox
> gzip -9 -n -c web/index.html > data/index.html.gz
>
> ```

## 📦 Fertige Firmware & Updates (ohne Arduino IDE)

Unter **[Releases](https://github.com/hflocki/camper_toolbox/releases)** liegt für jeden Chip fertige Firmware in zwei Varianten. Die enthaltenen Plugins werden in der Web-UI ein- bzw. ausgeschaltet (nicht benötigte am besten ausschalten).

| Datei                                             | Wofür                                                  |
| ------------------------------------------------- | ------------------------------------------------------ |
| `camper_toolbox-<chip>-<variante>-vX.Y.Z-full.bin` | **Erstinstallation** per USB, an Adresse `0x0` flashen |
| `camper_toolbox-<chip>-<variante>-vX.Y.Z-ota.bin`  | **Update** über die Web-UI                             |
| `index.html.gz`                                   | Web-Oberfläche, über die Web-UI hochladen              |

`<chip>` ist `esp32c3` (z. B. C3 Super Mini), `esp32s3` oder `esp32` (klassischer ESP32).

| Variante            | Enthalten                                                                    |
| ------------------- | ---------------------------------------------------------------------------- |
| `all`               | Gas (DIMES, Mopeka Pro, Mopeka Standard), Wasserpumpe, 1 Dachhaube, Kühlschrank |
| `dimes-heki-fridge` | Gas DIMES, **bis zu 2 Dachhauben** (RV Tech), Kühlschrank                    |

Ein ESP32 hält höchstens **3 Bluetooth-Verbindungen** (Pumpe, Dachhauben, Kühlschrank zusammen). Deshalb gibt es keine Variante mit Pumpe **und** zwei Dachhauben **und** Kühlschrank – dafür zwei ESP32 nehmen oder selbst bauen (`config_local.h`, z. B. `#define PLUGIN_RVTECH 2`).

Bei zwei Dachhauben erscheinen „Dachhaube“ und „Dachhaube 2“ als eigene Plugins mit eigener MAC und PIN, in Home Assistant als getrennte Geräte. Wer nur eine hat, schaltet „Dachhaube 2“ in der Web-UI aus.

**Erstinstallation:** `-full.bin` z. B. mit dem [ESP Web Flasher](https://espressif.github.io/esptool-js/) an Adresse `0x0` flashen. Danach mit dem WLAN `Camper-Toolbox-XXXXXX` verbinden, `http://192.168.4.1` öffnen, `index.html.gz` hochladen und WLAN / MQTT einrichten.

**Update:** Web-UI → Firmware-Upload → `-ota.bin` wählen. Danach startet das Gerät neu.

> **Einstellungen bleiben erhalten.** Das Update ersetzt nur das Programm. WLAN, MQTT, Login, Plugin-Einstellungen (MAC, PIN …) und Bluetooth-Kopplungen liegen in eigenen Bereichen des Flash und werden nicht angefasst. Nur ein Flash mit „Erase All Flash“ oder ein Werksreset löscht sie.

In Release-Builds ist der Netzwerk-Upload aus der Arduino IDE abgeschaltet; Updates laufen dort über die Web-UI (mit Web-Login, falls gesetzt).

### Release erstellen (für Entwickler)

Ein Tag startet den GitHub-Workflow `.github/workflows/release.yml`, der alle Chips baut und das Release anlegt:

```
git tag v0.1.1
git push origin v0.1.1
```

Die Versionsnummer aus dem Tag erscheint in der Web-UI und in Home Assistant.

## 🧪 Nutzung & Wartung

- **Serial Monitor:** Bei einer Baudrate von `115200` gibt das System Boot-Informationen, IP-Adressen, BLE-Scans und Sensordaten aus.

- **Home Assistant:** Sofern MQTT aktiviert ist, erstellt das Tool via Auto-Discovery automatisch alle Sensoren, Switche und Messwerte in Home Assistant.

- **OTA-Updates:** Nach dem ersten Flashen per USB können zukünftige Updates bequem über WLAN eingespielt werden (`camper-tool` erscheint als Netzwerkport in der Arduino IDE, siehe `OTA_HOSTNAME`).

## 🤝 Beitragen

Beiträge, Ideen und Verbesserungsvorschläge sind herzlich willkommen!

1. Forke das Repository

2. Erstelle einen Feature-Branch (`git checkout -b feature/NeuesFeature`)

3. Committe deine Änderungen (`git commit -m 'Neues Feature hinzugefügt'`)

4. Pushe den Branch (`git push origin feature/NeuesFeature`)

5. Öffne einen **Pull Request**

## 📄 Lizenz

Dieses Projekt steht unter der MIT License.
