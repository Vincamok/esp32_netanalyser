#pragma once
#include "display.h"
#include <Arduino.h>

void fetchPublicIps(SecurityInfo& sec);
bool checkArpSpoof(const char* gwIp, SecurityInfo& sec, bool firstScan);
bool checkDnsLeak(SecurityInfo& sec);
bool detectTransparentProxy(SecurityInfo& sec);
