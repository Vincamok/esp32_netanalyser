#pragma once
#include "display.h"
#include <Arduino.h>

void historyInit();
void historySave(const NetworkInfo& info);
int  historyLoad(HistoryEntry* entries, int maxCount);
void historyClear();
