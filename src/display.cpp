#include "display.h"
#include "config.h"
#include "qrcode.h"
#include <LovyanGFX.hpp>

static const char* statusIcon(TestStatus s) {
    switch (s) {
        case TestStatus::OK:      return "[OK]";
        case TestStatus::WARN:    return "[!!]";
        case TestStatus::FAIL:    return "[KO]";
        case TestStatus::RUNNING: return "[..]";
        default:                  return "[  ]";
    }
}

uint16_t statusColor(TestStatus s) {
    switch (s) {
        case TestStatus::OK:      return COL_OK;
        case TestStatus::WARN:    return COL_WARN;
        case TestStatus::FAIL:    return COL_ERR;
        case TestStatus::RUNNING: return COL_ACCENT;
        default:                  return COL_MUTED;
    }
}

void displayInit(LGFX& tft) {
    tft.init();
    tft.setRotation(1);
    tft.setBrightness(200);
    tft.fillScreen(COL_BG);
}

void displaySplash(LGFX& tft) {
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_HEADER);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.print("ESP32 NET TESTER");
    tft.setTextColor(COL_MUTED);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.print("v2.0  USB-C Ethernet");
    tft.setCursor(10, 75);
    tft.print("Initialisation...");
}

void displayProgress(LGFX& tft, const char* label, uint8_t pct) {
    int w = tft.width();
    tft.fillRect(0, tft.height() - 24, w, 24, COL_BG);
    tft.setTextColor(COL_TEXT);
    tft.setTextSize(1);
    tft.setCursor(4, tft.height() - 20);
    tft.print(label);

    int barW = (w - 8) * pct / 100;
    tft.drawRect(4, tft.height() - 10, w - 8, 8, COL_MUTED);
    tft.fillRect(5, tft.height() - 9, barW, 6, COL_ACCENT);
}

static void drawSummary(LGFX& tft, const NetworkInfo& info) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 18;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== RESEAU ===");
    y += lh + 2;

    tft.setTextColor(statusColor(info.stEth));
    tft.setCursor(x, y);
    tft.printf("%s ETH ", statusIcon(info.stEth));
    if (info.ethLinked) {
        tft.printf("%dMbps %s", info.linkSpeed, info.fullDuplex ? "Full" : "Half");
    } else {
        tft.print("Non connecte");
    }
    y += lh;

    tft.setTextColor(statusColor(info.stIp));
    tft.setCursor(x, y);
    tft.printf("%s IP  %s", statusIcon(info.stIp), info.ipAddr);
    y += lh;

    tft.setTextColor(COL_MUTED);
    tft.setCursor(x + 28, y);
    tft.printf("Mask: %s", info.subnetMask);
    y += lh;

    tft.setTextColor(statusColor(info.stGateway));
    tft.setCursor(x, y);
    tft.printf("%s GW  %s", statusIcon(info.stGateway), info.gateway);
    if (info.pingGatewayMs >= 0) tft.printf(" %dms", info.pingGatewayMs);
    y += lh;

    tft.setTextColor(statusColor(info.stDns));
    tft.setCursor(x, y);
    tft.printf("%s DNS %s", statusIcon(info.stDns), info.dns1);
    y += lh;

    tft.setTextColor(statusColor(info.stInternet));
    tft.setCursor(x, y);
    tft.printf("%s WAN ", statusIcon(info.stInternet));
    if      (info.stInternet == TestStatus::OK)   tft.print("OK");
    else if (info.stInternet == TestStatus::FAIL) tft.print("HORS LIGNE");
    else                                          tft.print("...");
    y += lh;

    tft.setTextColor(statusColor(info.stSpeed));
    tft.setCursor(x, y);
    tft.printf("%s DL  %.1f Mbps", statusIcon(info.stSpeed), info.downloadKbps / 1000.0f);

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("UP/DN:page  LP-UP:rerun");
}

static void drawIpDetails(LGFX& tft, const NetworkInfo& info) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 16;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== DETAILS IP ===");
    y += lh + 2;

    auto row = [&](const char* label, const char* val) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.printf("%-7s", label);
        tft.setTextColor(COL_TEXT);
        tft.print(val);
        y += lh;
    };

    row("IP",      info.ipAddr);
    row("Masque",  info.subnetMask);
    row("Gateway", info.gateway);
    row("DNS 1",   info.dns1);
    row("DNS 2",   info.dns2[0] ? info.dns2 : "---");

    y += 4;
    char macStr[20];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
        info.mac[0], info.mac[1], info.mac[2],
        info.mac[3], info.mac[4], info.mac[5]);
    row("MAC", macStr);

    tft.setTextColor(info.fullDuplex ? COL_OK : COL_WARN);
    tft.setCursor(x, y);
    tft.printf("Mode %s-duplex @ %d Mbps",
        info.fullDuplex ? "Full" : "Half", info.linkSpeed);

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Retour  [DN] Ping");
}

static void drawPing(LGFX& tft, const NetworkInfo& info) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 18;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== PING ===");
    y += lh + 2;

    auto pingRow = [&](const char* host, int32_t ms, uint8_t loss) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y);
        tft.printf("%-15s", host);
        if (ms < 0) {
            tft.setTextColor(COL_ERR);
            tft.print("TIMEOUT");
        } else {
            uint16_t col = ms < 20 ? COL_OK : (ms < 100 ? COL_WARN : COL_ERR);
            tft.setTextColor(col);
            tft.printf("%4dms", ms);
            tft.setTextColor(loss == 0 ? COL_OK : COL_WARN);
            tft.printf(" -%d%%", loss);
        }
        y += lh;
    };

    pingRow(info.gateway,  info.pingGatewayMs, info.pingLossGw);
    pingRow(PING_HOST_1,   info.pingDns1Ms,    0);
    pingRow(PING_HOST_2,   info.pingDns2Ms,    0);

    y += 6;
    tft.setTextColor(COL_MUTED);
    tft.setCursor(x, y); tft.print("DNS:");
    y += lh;
    tft.setTextColor(info.dnsOk ? COL_OK : COL_ERR);
    tft.setCursor(x + 10, y);
    tft.printf("%s -> %s", DNS_TEST_HOST, info.dnsOk ? "OK" : "ECHEC");

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] IP  [DN] Debit");
}

static void drawSpeed(LGFX& tft, const NetworkInfo& info) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 20;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== DEBIT ===");
    y += lh + 2;

    auto speedBar = [&](const char* label, float kbps, float maxKbps) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y);
        tft.printf("%-10s", label);
        float mbps = kbps / 1000.0f;
        uint16_t col = mbps > 50 ? COL_OK : (mbps > 10 ? COL_WARN : COL_ERR);
        tft.setTextColor(col);
        tft.printf("%.2f Mbps", mbps);
        y += 14;
        int barMax = tft.width() - x - 4;
        int barFill = (int)(barMax * min(kbps, maxKbps) / maxKbps);
        tft.drawRect(x, y, barMax, 8, COL_MUTED);
        tft.fillRect(x + 1, y + 1, max(barFill - 2, 0), 6, col);
        y += lh;
    };

    if (info.stSpeed == TestStatus::RUNNING) {
        tft.setTextColor(COL_ACCENT);
        tft.setCursor(x, y); tft.print("Test en cours...");
    } else if (info.stSpeed == TestStatus::PENDING) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("En attente...");
    } else {
        speedBar("Download", info.downloadKbps, 100000.0f);
        speedBar("Upload  ", info.uploadKbps,   100000.0f);
        y += 4;
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y);
        tft.printf("Lien: %dMbps %s",
            info.linkSpeed, info.fullDuplex ? "Full" : "Half");
    }

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Ping  [DN] Traceroute");
}

static void drawTraceroute(LGFX& tft, const TracerouteHop* hops, int count) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 15;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== TRACEROUTE ===");
    y += lh + 2;

    if (!hops || count == 0) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("En attente...");
    } else {
        int maxRows = (tft.height() - y - 14) / lh;
        for (int i = 0; i < count && i < maxRows; i++) {
            tft.setTextColor(COL_MUTED);
            tft.setCursor(x, y);
            tft.printf("%2d ", i + 1);
            if (hops[i].responded) {
                tft.setTextColor(hops[i].rttMs < 50 ? COL_OK : hops[i].rttMs < 150 ? COL_WARN : COL_ERR);
                // Truncate IP to fit
                char shortIp[13];
                strncpy(shortIp, hops[i].ip, 12);
                shortIp[12] = '\0';
                tft.printf("%-12s %dms", shortIp, hops[i].rttMs);
            } else {
                tft.setTextColor(COL_MUTED);
                tft.print("*");
            }
            y += lh;
        }
    }

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Debit  [DN] Scanner");
}

static void drawScanner(LGFX& tft, const ScannedHost* hosts, int count, int offset) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 15;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y);
    tft.printf("=== SCANNER (%d) ===", count);
    y += lh + 2;

    if (!hosts || count == 0) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("Scan en cours...");
    } else {
        int maxRows = (tft.height() - y - 14) / lh;
        for (int i = offset; i < count && (i - offset) < maxRows; i++) {
            tft.setTextColor(COL_OK);
            tft.setCursor(x, y);
            tft.printf("%-15s", hosts[i].ip);
            tft.setTextColor(COL_MUTED);
            // Truncate manufacturer name
            char mfr[13];
            strncpy(mfr, hosts[i].manufacturer, 12);
            mfr[12] = '\0';
            tft.print(mfr);
            y += lh;
        }
        if (count > maxRows) {
            tft.setTextColor(COL_MUTED);
            tft.setCursor(4, tft.height() - 12);
            tft.printf("[UP]prev [DN]next  %d/%d", offset + 1, count);
            return;
        }
    }

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Traceroute  [DN] Secu");
}

static void drawSecurity(LGFX& tft, const SecurityInfo* sec) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 16;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== SECURITE ===");
    y += lh + 2;

    if (!sec) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("En attente...");
    } else {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("IP publique (ipify):");
        y += lh;
        tft.setTextColor(COL_TEXT);
        tft.setCursor(x + 8, y);
        tft.print(sec->publicIpIpify[0] ? sec->publicIpIpify : "---");
        y += lh;

        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("IP publique (CF):");
        y += lh;
        tft.setTextColor(COL_TEXT);
        tft.setCursor(x + 8, y);
        tft.print(sec->publicIpCloudflare[0] ? sec->publicIpCloudflare : "---");
        y += lh + 4;

        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("DNS leak: ");
        tft.setTextColor(sec->dnsLeak ? COL_ERR : COL_OK);
        tft.print(sec->dnsLeak ? "DETECTE" : "OK");
        y += lh;

        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("ARP spoof: ");
        tft.setTextColor(sec->arpSpoofDetected ? COL_ERR : COL_OK);
        tft.print(sec->arpSpoofDetected ? "ALERTE" : "OK");
        y += lh;

        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("Proxy: ");
        tft.setTextColor(sec->transparentProxy ? COL_WARN : COL_OK);
        tft.print(sec->transparentProxy ? "DETECTE" : "Non");
    }

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Scanner  [DN] Cable");
}

static void drawCable(LGFX& tft, const CableDiag* cable) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 16;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== CABLE DIAG ===");
    y += lh + 2;

    if (!cable) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("En attente...");
    } else if (!cable->supported) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("W5500: TDR non dispo");
        y += lh;
        tft.setCursor(x, y); tft.print("Utiliser DP83848 RMII");
    } else {
        auto pairRow = [&](const char* name, PairStatus s) {
            tft.setTextColor(COL_MUTED);
            tft.setCursor(x, y);
            tft.printf("Paire %s: ", name);
            uint16_t col = (s == PairStatus::OK) ? COL_OK :
                           (s == PairStatus::UNKNOWN) ? COL_MUTED : COL_ERR;
            tft.setTextColor(col);
            switch (s) {
                case PairStatus::OK:    tft.print("OK");    break;
                case PairStatus::OPEN:  tft.print("OPEN");  break;
                case PairStatus::SHORT: tft.print("SHORT"); break;
                case PairStatus::CROSS: tft.print("CROSS"); break;
                default:                tft.print("N/A");   break;
            }
            y += lh;
        };

        pairRow("A", cable->pairA);
        pairRow("B", cable->pairB);
        pairRow("C", cable->pairC);
        pairRow("D", cable->pairD);

        y += 4;
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y);
        if (cable->lengthMeters >= 0) {
            tft.printf("Longueur: %.1f m", cable->lengthMeters);
        } else {
            tft.print("Longueur: N/A");
        }
    }

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Secu  [DN] Historique");
}

static void drawHistory(LGFX& tft, const HistoryEntry* history, int count) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 15;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== HISTORIQUE ===");
    y += lh + 2;

    if (!history || count == 0) {
        tft.setTextColor(COL_MUTED);
        tft.setCursor(x, y); tft.print("Aucun enregistrement");
    } else {
        int show = min(count, 5);
        // Show most recent first
        for (int i = count - 1; i >= count - show; i--) {
            const HistoryEntry& e = history[i];
            tft.setTextColor(e.internetOk ? COL_OK : COL_ERR);
            tft.setCursor(x, y);
            tft.printf("t+%lus %-15s", (unsigned long)e.ts, e.ip);
            y += lh;
            tft.setTextColor(COL_MUTED);
            tft.setCursor(x + 8, y);
            tft.printf("GW:%s P:%dms", e.gw, (int)e.pingMs);
            y += lh;
        }
    }

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] Cable  [DN] QR Code");
}

static void drawWebserverInfo(LGFX& tft, const NetworkInfo& info) {
    tft.fillScreen(COL_BG);
    int x = 4, y = 2, lh = 18;

    tft.setTextColor(COL_HEADER);
    tft.setTextSize(1);
    tft.setCursor(x, y); tft.print("=== WEBSERVER ===");
    y += lh + 4;

    tft.setTextColor(COL_MUTED);
    tft.setCursor(x, y); tft.print("Acceder via navigateur:");
    y += lh + 4;

    tft.setTextColor(COL_OK);
    tft.setTextSize(2);
    tft.setCursor(x, y);
    tft.printf("http://%s", info.ipAddr);
    y += lh * 2 + 4;

    tft.setTextSize(1);
    tft.setTextColor(COL_MUTED);
    tft.setCursor(x, y); tft.print("/api/status  - JSON complet");
    y += lh;
    tft.setCursor(x, y); tft.print("/api/scan    - Hotes ARP");
    y += lh;
    tft.setCursor(x, y); tft.print("/api/traceroute");
    y += lh;
    tft.setCursor(x, y); tft.print("/wol?mac=XX:XX:XX:XX:XX:XX");

    tft.setTextColor(COL_MUTED);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP] QR Code  [DN] Resume");
}

void displayPage(LGFX& tft, const NetworkInfo& info, Page page,
                 const DisplayExtra* extra) {
    const TracerouteHop* hops    = extra ? extra->hops     : nullptr;
    int                  hopCnt  = extra ? extra->hopCount  : 0;
    const ScannedHost*   hosts   = extra ? extra->hosts     : nullptr;
    int                  hostCnt = extra ? extra->hostCount : 0;
    int                  scanOff = extra ? extra->scanOffset: 0;
    const CableDiag*     cable   = extra ? extra->cable     : nullptr;
    const SecurityInfo*  sec     = extra ? extra->sec       : nullptr;
    const HistoryEntry*  hist    = extra ? extra->history   : nullptr;
    int                  histCnt = extra ? extra->histCount : 0;

    switch (page) {
        case Page::SUMMARY:       drawSummary(tft, info);                       break;
        case Page::IP_DETAILS:    drawIpDetails(tft, info);                     break;
        case Page::PING:          drawPing(tft, info);                          break;
        case Page::SPEED:         drawSpeed(tft, info);                         break;
        case Page::TRACEROUTE:    drawTraceroute(tft, hops, hopCnt);            break;
        case Page::SCANNER:       drawScanner(tft, hosts, hostCnt, scanOff);    break;
        case Page::SECURITY:      drawSecurity(tft, sec);                       break;
        case Page::CABLE:         drawCable(tft, cable);                        break;
        case Page::HISTORY:       drawHistory(tft, hist, histCnt);              break;
        case Page::QR_CODE:       drawQrCodePage(tft, info);                    break;
        case Page::WEBSERVER_INFO:drawWebserverInfo(tft, info);                 break;
        default: break;
    }
}
