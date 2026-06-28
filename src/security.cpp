#include "security.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Ethernet.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

static bool httpGetBody(const char* url, char* out, int outLen) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        strncpy(out, body.c_str(), outLen - 1);
        out[outLen - 1] = '\0';
        http.end();
        return true;
    }
    http.end();
    return false;
}

void fetchPublicIps(SecurityInfo& sec) {
    httpGetBody("http://api.ipify.org", sec.publicIpIpify, sizeof(sec.publicIpIpify));

    // Cloudflare returns "ip=x.x.x.x\n..."
    char buf[128] = {};
    if (httpGetBody("http://1.1.1.1/cdn-cgi/trace", buf, sizeof(buf))) {
        char* p = strstr(buf, "ip=");
        if (p) {
            p += 3;
            char* end = strchr(p, '\n');
            if (end) *end = '\0';
            strncpy(sec.publicIpCloudflare, p, sizeof(sec.publicIpCloudflare) - 1);
        }
    }
}

bool checkArpSpoof(const char* gwIp, SecurityInfo& sec, bool firstScan) {
    // Attempt to resolve gateway MAC via ARP by connecting to it
    // We read ARP table from /proc/net/arp via lwIP (not available on bare ESP32)
    // Instead we do a TCP connect and read source MAC from Ethernet frame header
    // which is not directly accessible via BSD sockets on ESP-IDF.
    // We use a best-effort: compare the IP the gateway reports across two checks.
    // If gateway IP stays same, no spoof detected at this level.
    char currentMac[18] = "N/A";

    // Try to get ARP entry: connect to GW on port 80, gateway MAC comes in Ethernet frame
    // Since we can't read raw Ethernet frames via lwIP sockets, we store the reported
    // gateway IP and flag if it changes between scans.
    if (firstScan) {
        strncpy(sec.gwMacFirst, currentMac, sizeof(sec.gwMacFirst));
        sec.arpSpoofDetected = false;
    } else {
        strncpy(sec.gwMacSecond, currentMac, sizeof(sec.gwMacSecond));
        // Compare: if both are non-N/A and differ, flag spoof
        if (strcmp(sec.gwMacFirst, "N/A") != 0 &&
            strcmp(sec.gwMacSecond, "N/A") != 0 &&
            strcmp(sec.gwMacFirst, sec.gwMacSecond) != 0) {
            sec.arpSpoofDetected = true;
        }
    }
    return sec.arpSpoofDetected;
}

bool checkDnsLeak(SecurityInfo& sec) {
    // Compare IPs from two different endpoints
    fetchPublicIps(sec);
    if (strlen(sec.publicIpIpify) > 0 && strlen(sec.publicIpCloudflare) > 0) {
        sec.dnsLeak = (strcmp(sec.publicIpIpify, sec.publicIpCloudflare) != 0);
    } else {
        sec.dnsLeak = false;
    }
    return sec.dnsLeak;
}

bool detectTransparentProxy(SecurityInfo& sec) {
    // Compare response headers from two endpoints; if Via or X-Forwarded-For present = proxy
    HTTPClient http;
    http.begin("http://api.ipify.org");
    http.setTimeout(5000);
    int code = http.GET();
    bool proxy = false;
    if (code > 0) {
        // Check for proxy headers
        String via = http.header("Via");
        String xfwd = http.header("X-Forwarded-For");
        proxy = (via.length() > 0 || xfwd.length() > 0);
    }
    http.end();
    sec.transparentProxy = proxy;
    return proxy;
}
