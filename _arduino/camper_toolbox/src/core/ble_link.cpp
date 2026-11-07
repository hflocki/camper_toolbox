#include "ble_link.h"
#include "devlog.h"

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

int32_t blePinParse(const char* s) {
    if (!s) return BLE_NO_PIN;
    size_t n = strlen(s);
    if (n == 0 || n > 6) return BLE_NO_PIN;
    for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return BLE_NO_PIN;
    return (int32_t)atol(s);   // "000000" -> 0, a valid passkey
}

void BleLink::setTarget(const char* mac_, int32_t pin_) {
    bool sameMac = strcmp(mac, mac_) == 0;
    if (sameMac && pin == pin_) return;
    disconnect();
    // Same device, new PIN: an old bond (maybe made with a wrong PIN) would block pairing
    if (sameMac && mac[0]) forgetBond();
    strlcpy(mac, mac_, sizeof(mac));
    pin = pin_;
    retryDelay = 0;
    lastAttempt = 0;
    devlogf(tag, "Device: %s%s", mac[0] ? mac : "(not set)", pin != BLE_NO_PIN ? " (PIN)" : "");
}

// Deletes a stored bond for the current device (both address types, the bond store
// does not tell which one was used)
void BleLink::forgetBond() {
    if (!mac[0]) return;
    bool a = NimBLEDevice::deleteBond(NimBLEAddress(std::string(mac), BLE_ADDR_PUBLIC));
    bool b = NimBLEDevice::deleteBond(NimBLEAddress(std::string(mac), BLE_ADDR_RANDOM));
    if (a || b) devlogf(tag, "Old bond deleted");
}

void BleLink::setState(const char* s) {
    if (strcmp(stateText, s) != 0) {
        strlcpy(stateText, s, sizeof(stateText));
        changes++;
        devlogf(tag, "State: %s", s);
    }
}

// ---- NimBLE callbacks (host task) --------------------------------------------

void BleLink::onConnect(NimBLEClient* c) {
    connectResult = 1;
}

void BleLink::onConnectFail(NimBLEClient* c, int reason) {
    pendingReason = reason;
    connectResult = -1;
}

void BleLink::onDisconnect(NimBLEClient* c, int reason) {
    pendingReason = reason;
    if (isConnecting) connectResult = -1;
    else linkLost = true;
}

void BleLink::onAuthenticationComplete(NimBLEConnInfo& info) {
    authEncrypted = info.isEncrypted();
    authAuthenticated = info.isAuthenticated();
    authBonded = info.isBonded();
    authDone = true;
}

// NimBLE reports HCI reasons as 0x200 + HCI code and SMP errors as
// 0x400 (our side) / 0x500 (reported by the device) + SMP code
const char* bleReasonText(int reason) {
    if (reason >= 0x400 && reason < 0x600) reason = 0x500 | (reason & 0xFF);
    switch (reason) {
        case 0x205: return "Kopplung abgelehnt (PIN?)";
        case 0x206: return "Schlüssel fehlt (neu koppeln)";
        case 0x208: return "Zeitüberschreitung";
        case 0x213: return "Vom Gerät getrennt";
        case 0x214: return "Gerät: zu wenig Ressourcen";
        case 0x216: return "Vom ESP getrennt";
        case 0x222: return "Zeitüberschreitung (LL)";
        case 0x23B: return "Verbindungsparameter abgelehnt";
        case 0x23D: return "MIC-Fehler (Schlüssel falsch)";
        case 0x23E: return "Verbindungsaufbau fehlgeschlagen";
        case 0x501: return "PIN-Eingabe fehlgeschlagen";
        case 0x502: return "Kopplung: OOB fehlt";
        case 0x503: return "Kopplung: Sicherheit zu gering";
        case 0x504: return "PIN falsch";
        case 0x505: return "Kopplung nicht unterstützt";
        case 0x506: return "Schlüssel zu kurz";
        case 0x508: return "Kopplung abgelehnt";
        case 0x509: return "Zu viele Versuche - warten";
        case 13:    return "Zeitüberschreitung (Host)";
        case BLE_REASON_SILENT:     return "Keine Daten erhalten (Abfragebefehl nötig?)";
        case BLE_REASON_PAIRING:    return "Kopplung fehlgeschlagen (PIN / Handy noch gekoppelt?)";
        case BLE_REASON_NEED_PIN:   return "Gerät verlangt Kopplung (PIN eintragen)";
        case BLE_REASON_NO_SERVICE: return "Dienst nicht gefunden (falsches Gerät?)";
        default:    return "Bluetooth-Fehler";
    }
}

// Logs every service and characteristic with its properties (diagnosis for unknown devices)
void BleLink::dumpGatt() {
    devlogf(tag, "GATT table:");
    for (NimBLERemoteService* s : client->getServices(false)) {
        devlogf(tag, "  service %s", s->getUUID().toString().c_str());
        for (NimBLERemoteCharacteristic* c : s->getCharacteristics(false)) {
            devlogf(tag, "    char %s  %s%s%s%s%s", c->getUUID().toString().c_str(),
                c->canRead() ? "read " : "", c->canWrite() ? "write " : "",
                c->canWriteNoResponse() ? "write-nr " : "", c->canNotify() ? "notify " : "",
                c->canIndicate() ? "indicate " : "");
        }
    }
}

void BleLink::onPassKeyEntry(NimBLEConnInfo& info) {
    if (pin == BLE_NO_PIN) {
        // The device asks for a PIN although none is configured
        devlogf(tag, "Device requests a PIN - enter it in the web UI");
        NimBLEDevice::injectPassKey(info, 0);
        return;
    }
    NimBLEDevice::injectPassKey(info, (uint32_t)pin);
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
    devlogf(tag, "Connecting to %s (type %u%s)", mac, type, knownType ? "" : ", guessed");

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
        devlogf(tag, "Subscribed %s", c->getUUID().toString().c_str());
    } else {
        // Typical cause: the characteristic needs an encrypted link (PIN / pairing)
        subscribeFailed++;
        devlogf(tag, "Subscribe %s FAILED (PIN / pairing needed?)", c->getUUID().toString().c_str());
    }
    return ok;
}

// Standard services that never carry device data. Skipped when subscribing to
// "every notifying characteristic", otherwise e.g. Service Changed (0x2A05) counts
// as a subscription and a failed data subscription goes unnoticed.
static bool isStandardService(const NimBLEUUID& u) {
    static const uint16_t STD[] = {0x1800, 0x1801, 0x180A, 0x180F};
    for (uint16_t s : STD) if (u == NimBLEUUID(s)) return true;
    return false;
}

// Called once the link is up: pairing, service discovery and subscription (typically < 1 s)
bool BleLink::finishConnect() {
    // MITM (passkey) only for the duration of this pairing, then back to the defaults
    // from bleInit(). Before, secure connections stayed switched off for every link.
    if (pin != BLE_NO_PIN) {
        NimBLEDevice::setSecurityAuth(true, true, true);
        bool ok = client->secureConnection(false);
        NimBLEDevice::setSecurityAuth(BLE_SEC_BOND, BLE_SEC_MITM, BLE_SEC_SC);
        if (!ok) {
            int e = client->getLastError();
            lastReason = (e >= 0x100) ? e : BLE_REASON_PAIRING;
            changes++;
            devlogf(tag, "Pairing failed (0x%03X: %s) - wrong PIN, or the device is still bonded to a phone?", e, bleReasonText(lastReason));
            setState("Kopplung fehlgeschlagen");
            disconnect();
            forgetBond();   // Do not keep a half-made bond for the next attempt
            return false;
        }
        devlogf(tag, "Paired / encrypted");
    }
    if (authDone) {
        authDone = false;
        devlogf(tag, "Security: encrypted %s, authenticated %s, bonded %s",
            authEncrypted ? "yes" : "no", authAuthenticated ? "yes" : "no", authBonded ? "yes" : "no");
    }

    subscribed = 0;
    subscribeFailed = 0;
    writeChr = nullptr;
    const std::vector<NimBLERemoteService*>& services = client->getServices(true);
    for (NimBLERemoteService* s : services) s->getCharacteristics(true);
    dumpGatt();
    for (NimBLERemoteService* s : services) {
        if (service[0] && !(s->getUUID() == NimBLEUUID(service))) continue;
        if (!service[0] && !notifyUuid[0] && isStandardService(s->getUUID())) continue;
        const std::vector<NimBLERemoteCharacteristic*>& chrs = s->getCharacteristics(false);   // Fetched above
        for (NimBLERemoteCharacteristic* c : chrs) {
            if (notifyUuid[0] ? (c->getUUID() == NimBLEUUID(notifyUuid)) : true) subscribeChr(c);
            if (writeUuid[0] && !writeChr && (c->getUUID() == NimBLEUUID(writeUuid))) writeChr = c;
        }
    }
    if (subscribed == 0 && subscribeFailed > 0) {
        // Characteristics exist, but the device refuses the subscription without pairing
        devlogf(tag, "Subscriptions refused - device needs a PIN (or a different one)");
        lastReason = BLE_REASON_NEED_PIN;
        changes++;
        setState("PIN nötig?");
        disconnect();
        return false;
    }
    if (subscribed == 0 || (writeUuid[0] && !writeChr)) {
        devlogf(tag, "Service / characteristics not found - wrong device?");
        lastReason = BLE_REASON_NO_SERVICE;
        changes++;
        setState("Falsches Gerät?");
        disconnect();
        return false;
    }

    linkLost = false;
    linkUp = true;
    lastReason = 0;
    rxLogged = 0;
    lastRx = millis();
    mtuValue = client->getMTU();
    lastRssiValue = client->getRssi();
    changes++;
    devlogf(tag, "Connected, MTU %u, %u subscription(s)", (unsigned)mtuValue, subscribed);
    return true;
}

// Retry later; back off while the device is out of range
void BleLink::connectFailed() {
    retryDelay = retryDelay ? min(retryDelay * 2, (unsigned long)BLE_RETRY_MAX_MS) : BLE_RETRY_MS;
    lastAttempt = millis();
    if (strcmp(stateText, "Kopplung fehlgeschlagen") && strcmp(stateText, "Falsches Gerät?") &&
        strcmp(stateText, "PIN nötig?")) {
        setState("Nicht erreichbar");
    }
}

void BleLink::loop() {
    int r = pendingReason;
    if (r) {
        pendingReason = 0;
        // Our own disconnect after a diagnosed problem: keep the more useful reason
        bool own = lastReason > 0 && lastReason < 0x100;
        if (!(r == 0x216 && (own || lastReason >= 0x400))) lastReason = r;
        changes++;
        devlogf(tag, "Reason 0x%03X: %s", r, bleReasonText(r));
    }
    if (linkLost) {
        linkLost = false;
        if (linkUp) devlogf(tag, "Disconnected");
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
            devlogf(tag, "Nothing received for %lu s - reconnecting (poll command needed?)", silenceMs / 1000);
            lastReason = BLE_REASON_SILENT;
            changes++;
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
    // Diagnosis: the first packets of every connection as hex and text
    if (rxLogged < 20) {
        rxLogged++;
        char hex[BLE_PACKET_MAX * 3 + 1];
        char txt[BLE_PACKET_MAX + 1];
        size_t h = 0;
        for (uint16_t i = 0; i < p.len; i++) {
            h += snprintf(hex + h, sizeof(hex) - h, " %02X", p.data[i]);
            txt[i] = isprint(p.data[i]) ? (char)p.data[i] : '.';
        }
        hex[h] = '\0';
        txt[p.len] = '\0';
        devlogf(tag, "RX %u bytes:%s  |%s|", (unsigned)p.len, hex, txt);
    }
    return true;
}

void BleLink::setReleased(bool r) {
    if (r == isReleased) return;
    isReleased = r;
    changes++;
    if (r) {
        releasedAt = millis();
        disconnect();
        devlogf(tag, "Bluetooth released for the app");
    } else {
        retryDelay = 0;
        lastAttempt = 0;
        devlogf(tag, "Bluetooth taken back");
    }
}

#endif // CT_BLE_LINKS
