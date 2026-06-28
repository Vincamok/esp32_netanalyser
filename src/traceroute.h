#pragma once
#include <Arduino.h>
#include "display.h"

int runTraceroute(const char* dest, TracerouteHop* hops, int maxHops);
