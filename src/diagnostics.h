#pragma once
#include <Arduino.h>

struct DiagResults {
    float jitterMs;
    int optimalMtu;
    float tcpThroughputKbps;
    bool captivePortal;
    bool captivePortalChecked;
};

float measureJitter(const char* host, int count);
int   findOptimalMtu(const char* host);
float measureTcpThroughput(const char* serverIp, uint16_t port);
bool  detectCaptivePortal();
