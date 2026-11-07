#include "devlog.h"

#include <mutex>
#include <stdarg.h>

struct DevLog {
    char tag[16];
    char* buf;       // DEVLOG_SIZE bytes, nullptr = slot free
    size_t head;     // Index of the oldest byte
    size_t len;      // Bytes in use
};

static DevLog logs[DEVLOG_MAX_TAGS];
// Written from loop(), read from the web server task
static std::mutex logLock;

// Caller holds logLock
static DevLog* findLog(const char* tag, bool create) {
    DevLog* freeSlot = nullptr;
    for (DevLog& l : logs) {
        if (l.buf && strcmp(l.tag, tag) == 0) return &l;
        if (!l.buf && !freeSlot) freeSlot = &l;
    }
    if (!create || !freeSlot) return nullptr;
    freeSlot->buf = (char*)malloc(DEVLOG_SIZE);
    if (!freeSlot->buf) return nullptr;   // Out of memory: serial output only
    strlcpy(freeSlot->tag, tag, sizeof(freeSlot->tag));
    freeSlot->head = 0;
    freeSlot->len = 0;
    return freeSlot;
}

// Caller holds logLock
static void appendLog(DevLog* l, const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        l->buf[(l->head + l->len) % DEVLOG_SIZE] = s[i];
        if (l->len < DEVLOG_SIZE) l->len++;
        else l->head = (l->head + 1) % DEVLOG_SIZE;   // Full: overwrite the oldest byte
    }
}

void devlogf(const char* tag, const char* fmt, ...) {
    char msg[600];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(msg)) n = sizeof(msg) - 1;

    Serial.printf("[%s] %s\n", tag, msg);

    // Time stamp: uptime h:mm:ss.mmm (the ESP has no clock time)
    unsigned long ms = millis();
    char stamp[24];
    int sn = snprintf(stamp, sizeof(stamp), "%lu:%02lu:%02lu.%03lu ", ms / 3600000UL, (ms / 60000UL) % 60,
                      (ms / 1000UL) % 60, ms % 1000UL);

    std::lock_guard<std::mutex> guard(logLock);
    DevLog* l = findLog(tag, true);
    if (!l) return;
    appendLog(l, stamp, sn);
    appendLog(l, msg, n);
    appendLog(l, "\n", 1);
}

bool devlogGet(const char* tag, String& out) {
    std::lock_guard<std::mutex> guard(logLock);
    DevLog* l = findLog(tag, false);
    if (!l || l->len == 0) return false;
    size_t start = l->head, len = l->len;
    if (len == DEVLOG_SIZE) {
        // Buffer has wrapped: skip the cut-off first line
        size_t skip = 0;
        while (skip < len && l->buf[(start + skip) % DEVLOG_SIZE] != '\n') skip++;
        if (skip < len) skip++;
        start = (start + skip) % DEVLOG_SIZE;
        len -= skip;
    }
    size_t first = len;
    if (start + first > DEVLOG_SIZE) first = DEVLOG_SIZE - start;   // Wraps around
    out.concat(l->buf + start, first);
    if (first < len) out.concat(l->buf, len - first);
    return true;
}

void devlogClear(const char* tag) {
    std::lock_guard<std::mutex> guard(logLock);
    DevLog* l = findLog(tag, false);
    if (l) {
        l->head = 0;
        l->len = 0;
    }
}
