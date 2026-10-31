/*
 * Camper Tool - one ESP32, several camper devices as plugins
 * ===========================================================
 * Which plugins are built in is chosen in config.h (PLUGIN_xxx = number of instances):
 *   - Gas: Rotarex DIMES / SRG WAVE, Mopeka Pro, Mopeka Standard (passive BLE)
 *   - Water pump: CaraWater (e.g. ISP 15), BLE connection (status + control)
 *   - Roof hatch: RV Tech skylight motor, BLE connection (position, presets)
 *   - Fridge: WCS compressor fridge, BLE connection (temperatures, voltage)
 *
 * Values are published via MQTT (Home Assistant discovery) and shown in the web UI.
 * Core: src/core (WiFi, MQTT, web server, BLE), plugins: src/plugins.
 */

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "config.h"
#include "src/core/registry.h"
#include "src/core/settings.h"
#include "src/core/wifi_manager.h"
#include "src/core/ble.h"
#include "src/core/mqtt_ha.h"
#include "src/core/webserver.h"

AppConfig config;
CoreData coreData;

static unsigned long lastStatusPrint = 0;

void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(DEFAULT_OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        // Filesystem upload over the network replaces config.json too; unmount first
        if (ArduinoOTA.getCommand() != U_FLASH) LittleFS.end();
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("[OTA] Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        unsigned int pct = (total > 0) ? (progress * 100U / total) : 0;
        Serial.printf("[OTA] Progress: %u%%\r", pct);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]\n", error);
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

static void printRead() {
    JsonDocument doc;
    buildLiveJson(doc);
    serializeJsonPretty(doc, Serial);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(50);  // Do not block the loop while waiting for a line end
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Camper Tool v" FW_VERSION);
    Serial.println("========================================");
    Serial.println();

    memset(&coreData, 0, sizeof(coreData));

    // Phase 1: mount LittleFS, create the plugins, load the config
    settingsInit();
    FOR_EACH_PLUGIN(p) {
        Serial.printf("[MAIN] Plugin %-10s %-22s %s\n", p->id, p->typeName, p->enabled ? "on" : "off");
    }
    webserverInit();

    // Phase 2: WiFi (blocking connect or AP fallback)
    wifiManagerInit();

    // Phase 3: Bluetooth LE (after WiFi, both share the radio)
    if (registryNeedsBle()) bleInit();

    // Phase 4: plugins
    FOR_EACH_ENABLED_PLUGIN(p) p->begin();

    // Phase 5: web server, OTA, MQTT
    webserverStart();
    setupOTA();
    mqttInit();

    Serial.printf("[MAIN] Setup done! Heap: %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
    wifiManagerLoop();
    ArduinoOTA.handle();

    bleLoop();
    FOR_EACH_ENABLED_PLUGIN(p) p->loop();
    mqttLoop();
    webserverLoop();

    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "read") {
            printRead();
        } else if (cmd == "scan") {
            blePrintSeenList();
        } else if (cmd == "noauth") {
            // Forgotten web login: switch it off over USB
            config.web_user[0] = '\0';
            config.web_pass[0] = '\0';
            saveConfig();
            Serial.println("[CMD] Web login removed");
        } else if (cmd == "help") {
            Serial.println("[CMD] Commands: read, scan, noauth, help");
        }
    }

    if (millis() - lastStatusPrint > 10000) {
        lastStatusPrint = millis();
        Serial.printf("[STATUS] WiFi:%s MQTT:%s BLE adv:%lu Heap:%u\n",
            getWiFiStateString().c_str(), mqttIsConnected() ? "OK" : "---",
            (unsigned long)bleAdvCount(), (unsigned)ESP.getFreeHeap());
    }

    delay(5);
    yield();
}
