#include "webserver.h"
#include "config.h"
#include "history.h"
#include "scanner.h"
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Ethernet.h>

static WebServer server(WEBSERVER_PORT);
static NetworkInfo*    s_info      = nullptr;
static ScannedHost*    s_hosts     = nullptr;
static int*            s_hostCount = nullptr;
static TracerouteHop*  s_hops      = nullptr;
static int*            s_hopCount  = nullptr;

static void handleRoot() {
    if (!s_info) { server.send(503, "text/plain", "Not ready"); return; }

    String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>ESP32 Net Tester</title>"
        "<style>"
        "body{background:#000;color:#eee;font-family:monospace;margin:20px}"
        "h1{color:#0af}h2{color:#08f}"
        "table{border-collapse:collapse;width:100%;margin-bottom:20px}"
        "th,td{border:1px solid #333;padding:6px 10px;text-align:left}"
        "th{background:#111;color:#0af}"
        ".ok{color:#0f0}.warn{color:#ff0}.err{color:#f00}.muted{color:#666}"
        "</style></head><body>"
        "<h1>ESP32 Network Tester</h1>");

    // Network summary table
    html += F("<h2>Network</h2><table>"
        "<tr><th>Parameter</th><th>Value</th></tr>");
    html += "<tr><td>IP</td><td>" + String(s_info->ipAddr) + "</td></tr>";
    html += "<tr><td>Subnet</td><td>" + String(s_info->subnetMask) + "</td></tr>";
    html += "<tr><td>Gateway</td><td>" + String(s_info->gateway) + "</td></tr>";
    html += "<tr><td>DNS</td><td>" + String(s_info->dns1) + "</td></tr>";
    html += "<tr><td>Ping GW</td><td class='" +
            String(s_info->pingGatewayMs < 0 ? "err" : s_info->pingGatewayMs < 20 ? "ok" : "warn") + "'>" +
            (s_info->pingGatewayMs < 0 ? String("TIMEOUT") : String(s_info->pingGatewayMs) + " ms") + "</td></tr>";
    html += "<tr><td>Internet</td><td class='" + String(s_info->internetOk ? "ok" : "err") + "'>" +
            String(s_info->internetOk ? "OK" : "OFFLINE") + "</td></tr>";
    html += "<tr><td>Download</td><td>" + String(s_info->downloadKbps / 1000.0f, 2) + " Mbps</td></tr>";
    html += F("</table>");

    // Scanned hosts
    if (s_hosts && s_hostCount && *s_hostCount > 0) {
        html += F("<h2>Scanned Hosts</h2><table>"
            "<tr><th>IP</th><th>Manufacturer</th></tr>");
        for (int i = 0; i < *s_hostCount; i++) {
            html += "<tr><td>" + String(s_hosts[i].ip) + "</td><td>" +
                    String(s_hosts[i].manufacturer) + "</td></tr>";
        }
        html += F("</table>");
    }

    // Traceroute
    if (s_hops && s_hopCount && *s_hopCount > 0) {
        html += F("<h2>Traceroute to 8.8.8.8</h2><table>"
            "<tr><th>Hop</th><th>IP</th><th>RTT</th></tr>");
        for (int i = 0; i < *s_hopCount; i++) {
            html += "<tr><td>" + String(i + 1) + "</td><td>" +
                    String(s_hops[i].ip) + "</td><td>" +
                    (s_hops[i].rttMs < 0 ? String("*") : String(s_hops[i].rttMs) + " ms") + "</td></tr>";
        }
        html += F("</table>");
    }

    html += F("<script>"
        "setInterval(()=>fetch('/api/status').then(r=>r.json()).then(d=>{"
        "document.querySelector('h1').textContent='ESP32 Net Tester - '+d.ip"
        "}),5000);"
        "</script></body></html>");

    server.send(200, "text/html", html);
}

static void handleApiStatus() {
    if (!s_info) { server.send(503, "application/json", "{}"); return; }

    JsonDocument doc;
    doc["ip"]           = s_info->ipAddr;
    doc["subnet"]       = s_info->subnetMask;
    doc["gateway"]      = s_info->gateway;
    doc["dns1"]         = s_info->dns1;
    doc["pingGwMs"]     = s_info->pingGatewayMs;
    doc["pingDns1Ms"]   = s_info->pingDns1Ms;
    doc["pingDns2Ms"]   = s_info->pingDns2Ms;
    doc["pingLossGw"]   = s_info->pingLossGw;
    doc["dnsOk"]        = s_info->dnsOk;
    doc["internetOk"]   = s_info->internetOk;
    doc["downloadKbps"] = s_info->downloadKbps;
    doc["uploadKbps"]   = s_info->uploadKbps;
    doc["linkSpeed"]    = s_info->linkSpeed;
    doc["fullDuplex"]   = s_info->fullDuplex;
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
        s_info->mac[0], s_info->mac[1], s_info->mac[2],
        s_info->mac[3], s_info->mac[4], s_info->mac[5]);
    doc["mac"] = mac;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleApiScan() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    if (s_hosts && s_hostCount) {
        for (int i = 0; i < *s_hostCount; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["ip"]           = s_hosts[i].ip;
            obj["manufacturer"] = s_hosts[i].manufacturer;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                s_hosts[i].mac[0], s_hosts[i].mac[1], s_hosts[i].mac[2],
                s_hosts[i].mac[3], s_hosts[i].mac[4], s_hosts[i].mac[5]);
            obj["mac"] = mac;
        }
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleApiTraceroute() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    if (s_hops && s_hopCount) {
        for (int i = 0; i < *s_hopCount; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["hop"]       = i + 1;
            obj["ip"]        = s_hops[i].ip;
            obj["rttMs"]     = s_hops[i].rttMs;
            obj["responded"] = s_hops[i].responded;
        }
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleWol() {
    if (!server.hasArg("mac")) {
        server.send(400, "text/plain", "Missing mac param");
        return;
    }
    String macStr = server.arg("mac");
    uint8_t mac[6];
    if (sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        server.send(400, "text/plain", "Invalid MAC format");
        return;
    }

    bool ok = sendWakeOnLan(mac);
    server.send(200, "text/plain", ok ? "WOL sent" : "WOL failed");
}

static void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

void webserverInit(NetworkInfo* info, ScannedHost* hosts, int* hostCount,
                   TracerouteHop* hops, int* hopCount) {
    s_info      = info;
    s_hosts     = hosts;
    s_hostCount = hostCount;
    s_hops      = hops;
    s_hopCount  = hopCount;

    server.on("/",              handleRoot);
    server.on("/api/status",    handleApiStatus);
    server.on("/api/scan",      handleApiScan);
    server.on("/api/traceroute",handleApiTraceroute);
    server.on("/wol",           handleWol);
    server.onNotFound(handleNotFound);
    server.begin();
}

void webserverTask(void* param) {
    while (true) {
        server.handleClient();
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}
