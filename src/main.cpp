#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "config.h"
#include "display.h"
#include "network.h"
#include "scanner.h"
#include "traceroute.h"
#include "diagnostics.h"
#include "security.h"
#include "cable_diag.h"
#include "history.h"
#include "webserver.h"
#include "qrcode.h"

// ─── LovyanGFX T-Display-S3 (ST7789) ─────────────────────────────────────────
#ifndef ESP32_2432S028R
class LGFX_TDisplay : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
public:
    LGFX_TDisplay() {
        {
            auto cfg = _bus.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read  = 20000000;
            cfg.pin_sclk   = TFT_SCLK;
            cfg.pin_mosi   = TFT_MOSI;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs     = TFT_CS;
            cfg.pin_rst    = TFT_RST;
            cfg.panel_width  = TFT_WIDTH;
            cfg.panel_height = TFT_HEIGHT;
            cfg.offset_rotation = 1;
            cfg.invert  = true;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = TFT_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};
#endif // !ESP32_2432S028R

// ─── LovyanGFX ESP32-2432S028R (ILI9341 2.8") ────────────────────────────────
#ifdef ESP32_2432S028R
class LGFX_Yellow : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
public:
    LGFX_Yellow() {
        {
            auto cfg = _bus.config();
            cfg.spi_host   = SPI3_HOST;   // VSPI sur ESP32 classic
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.pin_sclk   = TFT_SCLK;
            cfg.pin_mosi   = TFT_MOSI;
            cfg.pin_miso   = TFT_MISO;
            cfg.pin_dc     = TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs       = TFT_CS;
            cfg.pin_rst      = TFT_RST;
            cfg.panel_width  = TFT_WIDTH;
            cfg.panel_height = TFT_HEIGHT;
            cfg.offset_rotation = 0;
            cfg.invert       = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = TFT_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};
#endif // ESP32_2432S028R

// ─── Globals ──────────────────────────────────────────────────────────────────
#ifdef ESP32_2432S028R
static LGFX_Yellow   tft;
#else
static LGFX_TDisplay tft;
#endif
static NetworkInfo   netInfo;
static SecurityInfo  secInfo       = {};
static DiagResults   diagInfo      = {};
static CableDiag     cableDiag     = {};

static ScannedHost   scannedHosts[MAX_SCANNED_HOSTS];
static int           scannedCount  = 0;
static TracerouteHop trHops[TRACEROUTE_MAX_HOPS];
static int           trHopCount    = 0;
static HistoryEntry  histEntries[HISTORY_MAX_ENTRIES];
static int           histCount     = 0;
static int           scanOffset    = 0;

static Page          currentPage   = Page::SUMMARY;
static unsigned long lastTestMs    = 0;
static unsigned long lastButtonMs  = 0;
static bool          btnUpLast     = HIGH;
static bool          btnDnLast     = HIGH;
static unsigned long btnUpPressMs  = 0;
static bool          btnUpWasLong  = false;

static DisplayExtra buildExtra() {
    DisplayExtra e = {};
    e.hops       = trHops;
    e.hopCount   = trHopCount;
    e.hosts      = scannedHosts;
    e.hostCount  = scannedCount;
    e.scanOffset = scanOffset;
    e.cable      = &cableDiag;
    e.sec        = &secInfo;
    e.history    = histEntries;
    e.histCount  = histCount;
    return e;
}

static void generateMac(uint8_t* mac) {
    uint64_t id = ESP.getEfuseMac();
    mac[0] = 0x02;
    mac[1] = (id >> 40) & 0xFF;
    mac[2] = (id >> 32) & 0xFF;
    mac[3] = (id >> 16) & 0xFF;
    mac[4] = (id >>  8) & 0xFF;
    mac[5] =  id        & 0xFF;
}

static void onProgress(const char* label, uint8_t pct) {
    displayProgress(tft, label, pct);
}

// ─── Cycle de tests complet ───────────────────────────────────────────────────
static void runFullCycle() {
    runAllTests(netInfo, onProgress);

    onProgress("Diagnostics cable...", 92);
    cableDiag = runCableDiag();

    onProgress("Jitter / MTU...", 94);
    diagInfo.jitterMs      = measureJitter(PING_HOST_1, 20);
    diagInfo.optimalMtu    = findOptimalMtu(PING_HOST_1);
    diagInfo.captivePortal = detectCaptivePortal();

    onProgress("Securite reseau...", 96);
    fetchPublicIps(secInfo);
    checkArpSpoof(netInfo.gateway, secInfo, lastTestMs == 0);
    checkDnsLeak(secInfo);
    detectTransparentProxy(secInfo);

    onProgress("Scan ARP...", 97);
    scannedCount = arpScan(scannedHosts, MAX_SCANNED_HOSTS);

    onProgress("Traceroute...", 98);
    trHopCount = runTraceroute(PING_HOST_1, trHops, TRACEROUTE_MAX_HOPS);

    onProgress("Sauvegarde historique...", 99);
    historySave(netInfo);
    histCount = historyLoad(histEntries, HISTORY_MAX_ENTRIES);
    scanOffset = 0;

    onProgress("Termine", 100);
    lastTestMs = millis();
}

void setup() {
    Serial.begin(115200);

    pinMode(BTN_UP,   INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);

    displayInit(tft);
    displaySplash(tft);
    delay(1200);

    historyInit();
    scannerInit();
    generateMac(netInfo.mac);

    displayProgress(tft, "Init carte reseau...", 2);
    if (!ethInit(netInfo)) {
        displayProgress(tft, "Echec ETH - verifier connexion", 0);
        delay(3000);
    }

    // Serveur web sur core 0 (lancé avant les tests lents)
    webserverInit(&netInfo, scannedHosts, &scannedCount, trHops, &trHopCount);
    xTaskCreatePinnedToCore(webserverTask, "webserver", 8192, nullptr, 1, nullptr, 0);

    runFullCycle();

    DisplayExtra ex = buildExtra();
    displayPage(tft, netInfo, currentPage, &ex);
}

void loop() {
    unsigned long now = millis();

    // ─── Boutons ──────────────────────────────────────────────────────────
    bool btnUp = digitalRead(BTN_UP);
    bool btnDn = digitalRead(BTN_DOWN);

    // Long press detection for BTN_UP (outside debounce window)
    if (btnUp == LOW && btnUpLast == HIGH) {
        btnUpPressMs = now;
        btnUpWasLong = false;
    }
    if (btnUp == LOW && btnUpPressMs > 0 && !btnUpWasLong &&
        (now - btnUpPressMs) >= BTN_LONGPRESS_MS) {
        btnUpWasLong = true;
        displayProgress(tft, "Relance des tests...", 0);
        runFullCycle();
        DisplayExtra ex = buildExtra();
        displayPage(tft, netInfo, currentPage, &ex);
        lastButtonMs = now;
    }

    if (now - lastButtonMs > 200) {
        bool changed = false;
        if (btnUp == HIGH && btnUpLast == LOW && !btnUpWasLong) {
            int p = (int)currentPage - 1;
            if (p < 0) p = (int)Page::PAGE_COUNT - 1;
            currentPage = (Page)p;
            scanOffset = 0;
            changed = true;
        }
        if (btnDn == LOW && btnDnLast == HIGH) {
            if (currentPage == Page::SCANNER && scannedCount > 0) {
                int maxRows = (tft.height() - 36) / 15;
                if (scanOffset + maxRows < scannedCount) {
                    scanOffset++;
                    changed = true;
                } else {
                    int p = (int)currentPage + 1;
                    if (p >= (int)Page::PAGE_COUNT) p = 0;
                    currentPage = (Page)p;
                    scanOffset = 0;
                    changed = true;
                }
            } else {
                int p = (int)currentPage + 1;
                if (p >= (int)Page::PAGE_COUNT) p = 0;
                currentPage = (Page)p;
                scanOffset = 0;
                changed = true;
            }
        }

        if (changed) {
            lastButtonMs = now;
            DisplayExtra ex = buildExtra();
            displayPage(tft, netInfo, currentPage, &ex);
        }
    }
    btnUpLast = btnUp;
    btnDnLast = btnDn;

    // ─── Rafraîchissement auto ────────────────────────────────────────────
    if (now - lastTestMs >= REFRESH_INTERVAL_MS) {
        testGatewayPing(netInfo);
        testDns(netInfo);
        testInternet(netInfo);
        checkArpSpoof(netInfo.gateway, secInfo, false);
        historySave(netInfo);
        histCount = historyLoad(histEntries, HISTORY_MAX_ENTRIES);
        { DisplayExtra ex = buildExtra(); displayPage(tft, netInfo, currentPage, &ex); }
        lastTestMs = now;
    }

    delay(10);
}
