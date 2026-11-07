# AGENTS.md – Regeln für KI-Assistenten in diesem Repo

Camper Toolbox: ESP32-Firmware (Arduino IDE, kein PlatformIO) als Bluetooth-Gateway mit Plugins
(Gas DIMES/Mopeka, Wasserpumpe CaraWater, Dachhaube RV Tech, Kühlschrank WCS), Web-UI und
MQTT / Home Assistant. Sketch: `_arduino/camper_toolbox/`.

Die Plugins für Pumpe, Dachhaube und Kühlschrank werden parallel im Repo
[hflocki/vannode](https://github.com/hflocki/vannode) (`firmware/toolbox`, ohne Gas) weitergeführt.
Änderungen hier übernimmt der Maintainer dort selbst.

## Änderungen liefern

- **Nie selbst committen, pushen oder taggen.** Änderungen als Patch-Datei liefern, die der
  Maintainer im Repo-Ordner einspielt.
- Patch immer gegen den **aktuellen `origin/main`** erstellen (vorher holen) und mit
  `git apply --check` prüfen.
- Dateiname: `camper_toolbox_<thema>_<version>.patch`, z. B. `camper_toolbox_device_log_0.1.7.patch`.
- Binärdateien (`data/index.html.gz`) mit `git diff --binary` in den Patch aufnehmen.
- **Jede Lieferung erhöht die Version** in `_arduino/camper_toolbox/config.h`
  (`#define FW_VERSION`, letzte Stelle +1). Vorher prüfen, dass es den Tag `v<version>` noch nicht
  gibt (`git ls-remote --tags origin`).
- Wurde `web/index.html` geändert: `data/index.html.gz` neu erzeugen
  (`gzip -9 -n -c web/index.html > data/index.html.gz`) und das JavaScript prüfen (`node --check`).

## Git-Befehle für den Maintainer

Zu jedem Patch genau diesen Block mitliefern, mit Patch-Name, Commit-Text und Version ausgefüllt.
Der Tag startet `.github/workflows/release.yml`, das die `.bin`-Dateien baut und das
GitHub-Release anlegt. Der Patch wird nach dem Einspielen gelöscht.

```bash
cd ~/projekte/github/camper_toolbox
git pull --rebase
git apply <patch> && rm <patch>
git add -A
git status
git commit -m "<englischer Commit-Text> (v<version>)"
git push
git tag v<version> && git push origin v<version>
```

`git status` vor dem Commit prüfen: Es dürfen nur die Dateien aus dem Patch auftauchen.

## Code

- **Kommentare im Code auf Englisch.** Texte in der Web-UI und in Home Assistant auf Deutsch.
- Keine persönlichen Daten im Code oder in Beispielen (Fahrzeugnamen, echte MAC-Adressen, Namen):
  Gerätenamen und IDs generisch halten.
- **Zugangsdaten nie ins Repo**: Passwörter, Tokens, PINs nur in `config_local.h` (nicht in git)
  oder auf dem Gerät (Web-UI). Nie in `config.h`, Beispiele oder Workflows.
- Nicht versioniert (siehe `.gitignore`): `config_local.h`, `*.patch`, `.roo/`, `doc/funde.txt`.
- MAC, PIN und UUIDs der Geräte stehen in der Web-UI, nicht in `config_local.h`.
- Bluetooth-Grenze: höchstens **3 Verbindungen** pro ESP32
  (`PLUGIN_CARAWATER + PLUGIN_RVTECH + PLUGIN_WCS_FRIDGE <= 3`); Gas-Sensoren hören nur mit und
  zählen nicht.
- Log-Ausgaben der Bluetooth-Geräte über `devlogf(tag, ...)` (`src/core/devlog.h`), nicht direkt
  über `Serial`: So landen sie auch im herunterladbaren Geräte-Log der Web-UI.
- Protokolle nicht raten: Neue Annahmen (UUIDs, Status-Bits) erst loggen und mit echten Logs
  bestätigen, bevor sie Werte oder Schalter steuern.

## Prüfen vor der Lieferung

- Klammern und Syntax prüfen; wenn möglich mit `arduino-cli` bauen (Board ESP32-C3,
  Partition Scheme „Minimal SPIFFS“). Geht das nicht, das in der Antwort deutlich sagen.
- Geänderte APIs gegen NimBLE-Arduino 2.x, ArduinoJson 7 und ESPAsyncWebServer 3.x abgleichen
  (Versionen siehe `release.yml`).
