#include "ble_link.h"

#if CT_BLE_LINKS

#include "ble.h"

#define RECONNECT_DELAY_MS 2000   // After a regular disconnect
#define QUEUE_DEPTH        8

void BleLink::begin(const char* tag_, const char* svc, const char* notify, const char* wr) {
    tag = tag_;
    setUuids(svc, notify, wr);
    if (!queue) queue = xQueueCreate(QUEUE_DEPTH, sizeof(BlePacket));
}

void BleLink::setUuids(const char* svc, const char* notify, const char* wr) {
    bool changed = strcmp(service, svc) || strcmp(notifyUuid, notify) || strcmp(writeUuid, wr);
    strlcpy(service, svc, sizeof(service));
    strlcpy(notifyUuid, notify, sizeof(notifyUuid));
    strlcpy(writeUuid, wr, sizeof(writeUuid));
    if (changed && (linkUp || isConnecting)) {
        disconnect();
        retryDelay = 0;
        lastAttempt = 0;
    }
}

void BleLink::setTarget(const char* mac_, uint32_t pin_) {
    if (strcmp(mac, mac_) == 0 && pin == pin_) return;
    disconnect();
    strlcpy(mac, mac_, sizeof(mac));
    pin = pin_;
    retryDelay = 0;
    lastAttempt = 0;
    Serial.printf("[%s] Device: %s%s\n", tag, mac[0] ? mac : "(not set)", pin ? " (PIN)" : "");
}

void BleLink::setState(const char* s) {
    if (strcmp(stateText, s) != 0) {
        strlcpy(stateText, s, sizeof(stateText));
        changes++;
        Serial.printf("[%s] State: %s\n", tag, s);
    }
}

// ---- NimBLE callbacks (host task) --------------------------------------------

void BleLink::onConnect(NimBLEClient* c) {
    connectResult = 1;
}

void BleLink::onConnectFail(NimBLEClient* c, int reason) {
    connectResult = -1;
}

void BleLink::onDisconnect(NimBLEClient* c, int reason) {
    if (isConnecting) connectResult = -1;
    else linkLost = true;
}

void BleLink::onPassKeyEntry(NimBLEConnInfo& info) {
    NimBLEDevice::injectPassKey(info, pin);
}

void BleLink::onConfirmPasskey(NimBLEConnInfo& info, uint32_t p) {
    NimBLEDevice::injectConfirmPasskey(info, true);
}

void BleLink::push(const uint8_t* d, size_t n) {
    if (!queue) return;
    BlePacket p;
    p.len = n > BLE_PACKET_MAX ? BLE_PACKET_MAX : n;
    memcpy(p.data, d, p.len);
    xQueueSend(queue, &p, 0);   // Queue full: drop (never block the host task)
}

// ---- Connection handling (loop task) -------------------------------------------

void BleLink::markDisconnected() {
    bool was = linkUp;
    linkUp = false;
    writeChr = nullptr;
    subscribed = 0;
    lastRssiValue = 0;
    if (was) changes++;
}

void BleLink::disconnect() {
    if (isConnecting) {
        if (client) client->cancelConnect();
        isConnecting = false;
        bleScanResume();
        bleConnectUnlock(this);
    }
    if (client && client->isConnected()) client->disconnect();
    markDisconnected();
}

// Starts a non-blocking connection attempt; the result arrives via the client callbacks
bool BleLink::startConnect() {
    if (!bleConnectLock(this)) return true;   // Another device is connecting: try again shortly
    if (!client) {
        client = NimBLEDevice::createClient();
        if (!client) {
            bleConnectUnlock(this);
            return false;
        }
        client->setClientCallbacks(this, false);
        client->setConnectTimeout(BLE_CONNECT_TIMEOUT);
    }

    // Address type from the scan list (public vs. random); otherwise alternate between attempts
    uint8_t type = tryType;
    bool knownType = bleFindAddrType(mac, &type);
    if (!knownType) tryType = (tryType == BLE_ADDR_PUBLIC) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;

    bleScanPause();
    setState("Verbinde...");
    Serial.printf("[%s] Connecting to %s (type %u%s)\n", tag, mac, type, knownType ? "" : ", guessed");

    connectResult = 0;
    if (!client->connect(NimBLEAddress(std::string(mac), type), true, true, true)) {
        bleScanResume();
        bleConnectUnlock(this);
        return false;
    }
    isConnecting = true;
    connectStarted = millis();
    return true;
}

bool BleLink::subscribeChr(NimBLERemoteCharacteristic* c) {
    if (!c || !(c->canNotify() || c->canIndicate())) return false;
    bool ok = c->subscribe(c->canNotify(), [this](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) {
        push(d, n);
    });
    if (ok) {
        subscribed++;
        Serial.printf("[%s] Subscribed %s\n", tag, c->getUUID().toString().c_str());
    }
    return ok;
}

// Called once the link is up: pairing, service discovery and subscription (typically < 1 s)
bool BleLink::finishConnect() {
    if (pin) {
        NimBLEDevice::setSecurityAuth(true, true, false);  // Bonding + MITM (passkey pairing)
        if (!client->secureConnection(false)) {
            Serial.printf("[%s] Pairing failed - wrong PIN?\n", tag);
            setState("Kopplung fehlgeschlagen");
            disconnect();
            return false;
        }
    }

    subscribed = 0;
    writeChr = nullptr;
    const std::vector<NimBLERemoteService*>& services = client->getServices(true);
    for (NimBLERemoteService* s : services) {
        if (service[0] && !(s->getUUID() == NimBLEUUID(service))) continue;
        const std::vector<NimBLERemoteCharacteristic*>& chrs = s->getCharacteristics(true);
        for (NimBLERemoteCharacteristic* c : chrs) {
            if (notifyUuid[0] ? (c->getUUID() == NimBLEUUID(notifyUuid)) : true) subscribeChr(c);
            if (writeUuid[0] && !writeChr && (c->getUUID() == NimBLEUUID(writeUuid))) writeChr = c;
        }
    }
    if (subscribed == 0 || (writeUuid[0] && !writeChr)) {
        Serial.printf("[%s] Service / characteristics not found - wrong device?\n", tag);
        setState("Falsches Gerät?");
        disconnect();
        return false;
    }

    linkLost = false;
    linkUp = true;
    lastRx = millis();
    mtuValue = client->getMTU();
    lastRssiValue = client->getRssi();
    changes++;
    Serial.printf("[%s] Connected, MTU %u, %u subscription(s)\n", tag, (unsigned)mtuValue, subscribed);
    return true;
}

// Retry later; back off while the device is out of range
void BleLink::connectFailed() {
    retryDelay = retryDelay ? min(retryDelay * 2, (unsigned long)BLE_RETRY_MAX_MS) : BLE_RETRY_MS;
    lastAttempt = millis();
    if (strcmp(stateText, "Kopplung fehlgeschlagen") && strcmp(stateText, "Falsches Gerät?")) {
        setState("Nicht erreichbar");
    }
}

void BleLink::loop() {
    if (linkLost) {
        linkLost = false;
        if (linkUp) Serial.printf("[%s] Disconnected\n", tag);
        markDisconnected();
        retryDelay = RECONNECT_DELAY_MS;
        lastAttempt = millis();
    }

    // App release ends automatically after BLE_RELEASE_MS
    if (isReleased && BLE_RELEASE_MS > 0 && millis() - releasedAt > BLE_RELEASE_MS) setReleased(false);

    if (mac[0] == '\0') {
        setState("Kein Gerät");
    } else if (isReleased) {
        setState("Für App freigegeben");
    } else if (isConnecting) {
        // Async connect in progress - loop() keeps running meanwhile
        int result = connectResult;
        if (result == 0 && millis() - connectStarted > BLE_CONNECT_TIMEOUT + 2000) {
            client->cancelConnect();   // Safety net if the stack never reports a result
            result = -1;
        }
        if (result != 0) {
            isConnecting = false;
            bleScanResume();
            bleConnectUnlock(this);
            if (result == 1 && finishConnect()) {
                retryDelay = 0;
                setState("Verbunden");
            } else {
                connectFailed();
            }
        }
    } else if (!linkUp) {
        if (lastAttempt == 0 || millis() - lastAttempt > retryDelay) {
            if (!startConnect()) connectFailed();
        }
    } else {
        setState("Verbunden");
        if (silenceMs && millis() - lastRx > silenceMs) {
            Serial.printf("[%s] Nothing received for %lu s - reconnecting\n", tag, silenceMs / 1000);
            disconnect();
            retryDelay = RECONNECT_DELAY_MS;
            lastAttempt = millis();
        }
        if (millis() - lastRssiPoll > 5000 && client && client->isConnected()) {
            lastRssiPoll = millis();
            lastRssiValue = client->getRssi();
        }
    }
}

bool BleLink::write(const uint8_t* d, size_t n, bool response) {
    if (!writeChr || !client || !client->isConnected()) return false;
    // NimBLE falls back to a long write if the MTU is smaller than the data
    return writeChr->writeValue(d, n, response);
}

bool BleLink::receive(BlePacket& p) {
    if (!queue) return false;
    if (xQueueReceive(queue, &p, 0) != pdTRUE) return false;
    lastRx = millis();
    return true;
}

void BleLink::setReleased(bool r) {
    if (r == isReleased) return;
    isReleased = r;
    changes++;
    if (r) {
        releasedAt = millis();
        disconnect();
        Serial.printf("[%s] Bluetooth released for the app\n", tag);
    } else {
        retryDelay = 0;
        lastAttempt = 0;
        Serial.printf("[%s] Bluetooth taken back\n", tag);
    }
}

#endif // CT_BLE_LINKS
