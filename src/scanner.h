#pragma once
#include <Arduino.h>
#include "display.h"

#define MAX_SCANNED_HOSTS 64
#define SCANNED_PORTS_COUNT 10

struct PortScanResult {
    uint16_t port;
    bool open;
};

struct DhcpOffer {
    char serverIp[16];
    char offeredIp[16];
    uint8_t serverMac[6];
};

void scannerInit();
int  arpScan(ScannedHost* hosts, int maxHosts);
void portScanGateway(const char* gwIp, PortScanResult* results, int* count);
int  detectRogueDhcp(DhcpOffer* offers, int maxOffers, int waitMs);
bool sendWakeOnLan(const uint8_t mac[6]);
int  listenMdns(char* servicesBuf, int bufSize, int waitMs);
