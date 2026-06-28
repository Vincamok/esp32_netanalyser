#pragma once
#include "display.h"

// Initialise la carte réseau Ethernet (W5500 via SPI)
bool ethInit(NetworkInfo& info);

// Lance tous les tests séquentiellement, met à jour info et appelle cb à chaque étape
void runAllTests(NetworkInfo& info, void (*progressCb)(const char* label, uint8_t pct));

// Tests individuels
bool testEthLink(NetworkInfo& info);
bool testIpConfig(NetworkInfo& info);
bool testGatewayPing(NetworkInfo& info);
bool testDns(NetworkInfo& info);
bool testInternet(NetworkInfo& info);
bool testSpeed(NetworkInfo& info);

// Ping ICMP simple (retourne RTT en ms, -1 si timeout)
int32_t pingHost(const char* host, uint16_t timeoutMs = 2000);
