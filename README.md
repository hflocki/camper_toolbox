# Camper Tool (Arduino)

Eine benutzerfreundliche Steuerung und Überwachung für Campervans und Wohnmobile auf Basis von Arduino/ESP-Mikrocontrollern. Das **Camper Tool** hilft dabei, wichtige Sensordaten (z. B. Temperaturen, Tankstände, Batteriespannungen) zu erfassen und Aktoren (z. B. Lichter, Pumpen, Relais) bequem zu steuern.

---

## 🛠️ Funktionen

* **Sensorauswertung:**
  * Temperatur- und Luftfeuchtigkeitsmessung (z. B. Innenraum, Außenbereich, Kühlschrank)
  * Überwachung der Bordspannung (Starter- und Aufbaubatterie)
  * Messung von Tankständen (Frischwasser / Abwasser)
* **Aktorsteuerung:**
  * Schalten von Verbrauchern über Relais / MOSFETs (z. B. Wasserpumpe, Standheizung, Beleuchtung)
* **Konnektivität & Display:**
  * Anzeige der Systemwerte über ein lokales Display (z. B. Nextion, LCD/OLED) oder serielle Schnittstelle
  * Vorbereitung zur Anbindung an Smart-Home-Systeme (z. B. Home Assistant) oder Smartphone-Apps via Bluetooth/WLAN *(je nach Hardware-Setup)*

---

## 📂 Projektstruktur

```text
_arduino/camper_tool/
├── camper_tool.ino      # Main Sketch / Haupteinstiegspunkt
├── config.h             # Einstellungen, Pin-Belegungen & Schwellenwerte
└── src/                 # Zusatzmodule & Bibliotheken (Sensoren, Displays, Relais)
```

---

## 🚀 Hardware & Voraussetzungen

### Empfohlene Hardware
* **Mikrocontroller:** Arduino Mega 2560, ESP32 oder ESP8266 (je nach benötigten Pin-Anzahlen und WLAN/Bluetooth-Funktionen)
* **Sensoren:**
  * DS18B20 (Temperatursensoren)
  * DHT11 / DHT22 (Temperatur & Luftfeuchtigkeit)
  * Spannungssensor / Spannungsteiler-Module (bis 16V / 25V)
  * Ultraschall- oder Kapazitiv-Sensoren für Tankstände
* **Aktoren & Module:**
  * 4-Kanal / 8-Kanal Relais-Modul oder MOSFET-Platinen
  * Step-Down Converter / Buck Converter (12V auf 5V / 3.3V zur stabilen Stromversorgung)

### Software & Bibliotheken
* [Arduino IDE](https://www.arduino.cc/en/software) (Version 1.8.x oder 2.x)
* Benötigte Arduino-Bibliotheken (über den Bibliotheksverwalter installierbar):
  * `OneWire` & `DallasTemperature` (für DS18B20)
  * `DHT sensor library`
  * *(Ergänze hier weitere spezifische Libs, z. B. `PubSubClient`, `Nextion`, etc.)*

---

## ⚙️ Installation & Einrichtung

1. **Repository klonen:**
   ```bash
   git clone https://github.com/hflocki/camper_tool.git
   ```

2. **Projekt öffnen:**
   * Öffne die Datei `_arduino/camper_tool/camper_tool.ino` in der Arduino IDE.

3. **Konfiguration anpassen:**
   * Öffne die Datei `config.h`.
   * Passe die Pin-Belegungen und Kalibrierungswerte für deine verbauten Sensoren und Relais an.

4. **Kompilieren & Hochladen:**
   * Wähle dein Board (z. B. *Arduino Mega 2560* oder *ESP32 Dev Module*) unter `Werkzeuge` -> `Board`.
   * Wählen den passenden COM-Port aus.
   * Klicke auf **Hochladen** ($Ctrl + U$ / $Cmd + U$).

---

## 🧪 Nutzung

1. Nach dem Einschalten startet das System automatisch und initialisiert alle angeschlossenen Sensoren.
2. Über den **Seriellen Monitor** (Baudrate: `115200` oder in `config.h` definiert) können Statusmeldungen und Sensordaten in Echtzeit eingesehen werden.
3. Eventuell angeschlossene Displays oder Schalter ermöglichen das direkte Schalten der Verbraucher im Camper.

---

## 🤝 Beitragen

Beiträge, Ideen und Verbesserungsvorschläge sind herzlich willkommen! 

1. Forke das Repository
2. Erstelle einen Feature-Branch (`git checkout -b feature/NeuesFeature`)
3. Committe deine Änderungen (`git commit -m 'Neues Feature hinzugefügt'`)
4. Pushe den Branch (`git push origin feature/NeuesFeature`)
5. Öffne einen **Pull Request**

---

## 📄 Lizenz

Dieses Projekt steht unter der [MIT License](LICENSE) (oder der im Repository angegebenen Lizenz).
