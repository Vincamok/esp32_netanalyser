#pragma once
#include "display.h"
#include "scanner.h"
#include "traceroute.h"

void webserverInit(NetworkInfo* info, ScannedHost* hosts, int* hostCount,
                   TracerouteHop* hops, int* hopCount);
void webserverTask(void* param);
