#include "history.h"
#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define HISTORY_FILE "/history.json"

void historyInit() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }
}

void historySave(const NetworkInfo& info) {
    HistoryEntry entries[HISTORY_MAX_ENTRIES];
    int count = historyLoad(entries, HISTORY_MAX_ENTRIES);

    HistoryEntry newEntry;
    newEntry.ts          = (uint32_t)(millis() / 1000);
    newEntry.pingMs      = info.pingGatewayMs;
    newEntry.downloadKbps = info.downloadKbps;
    newEntry.internetOk  = info.internetOk;
    strncpy(newEntry.ip, info.ipAddr,  sizeof(newEntry.ip));
    strncpy(newEntry.gw, info.gateway, sizeof(newEntry.gw));

    // Shift entries if full (keep newest)
    if (count >= HISTORY_MAX_ENTRIES) {
        memmove(&entries[0], &entries[1], (HISTORY_MAX_ENTRIES - 1) * sizeof(HistoryEntry));
        count = HISTORY_MAX_ENTRIES - 1;
    }
    entries[count++] = newEntry;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ts"]           = entries[i].ts;
        obj["ip"]           = entries[i].ip;
        obj["gw"]           = entries[i].gw;
        obj["pingMs"]       = entries[i].pingMs;
        obj["downloadKbps"] = entries[i].downloadKbps;
        obj["internetOk"]   = entries[i].internetOk;
    }

    File f = LittleFS.open(HISTORY_FILE, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

int historyLoad(HistoryEntry* entries, int maxCount) {
    if (!LittleFS.exists(HISTORY_FILE)) return 0;

    File f = LittleFS.open(HISTORY_FILE, "r");
    if (!f) return 0;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return 0;

    JsonArray arr = doc.as<JsonArray>();
    int count = 0;
    for (JsonObject obj : arr) {
        if (count >= maxCount) break;
        HistoryEntry& e = entries[count];
        e.ts           = obj["ts"]           | 0;
        e.pingMs       = obj["pingMs"]        | -1;
        e.downloadKbps = obj["downloadKbps"]  | 0.0f;
        e.internetOk   = obj["internetOk"]    | false;
        strncpy(e.ip, obj["ip"] | "---", sizeof(e.ip));
        strncpy(e.gw, obj["gw"] | "---", sizeof(e.gw));
        count++;
    }
    return count;
}

void historyClear() {
    LittleFS.remove(HISTORY_FILE);
}
