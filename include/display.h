#pragma once
#include <LovyanGFX.hpp>
#include <stdint.h>

#define COL_BG          0x0008
#define COL_HEADER      0x04FF
#define COL_OK          0x07E0
#define COL_WARN        0xFFE0
#define COL_ERR         0xF800
#define COL_TEXT        0xFFFF
#define COL_MUTED       0x8410
#define COL_ACCENT      0x001F

enum class TestStatus { PENDING, RUNNING, OK, WARN, FAIL };

struct TracerouteHop {
    char    ip[16];
    int32_t rttMs;
    bool    responded;
};

struct ScannedHost {
    char    ip[16];
    uint8_t mac[6];
    char    manufacturer[24];
    bool    reachable;
};

enum class PairStatus { OK, OPEN, SHORT, CROSS, UNKNOWN };

struct CableDiag {
    PairStatus pairA;
    PairStatus pairB;
    PairStatus pairC;
    PairStatus pairD;
    float      lengthMeters;
    bool       supported;
};

struct HistoryEntry {
    uint32_t ts;
    char     ip[16];
    char     gw[16];
    int32_t  pingMs;
    float    downloadKbps;
    bool     internetOk;
};

struct SecurityInfo {
    char publicIpIpify[16];
    char publicIpCloudflare[16];
    bool dnsLeak;
    bool arpSpoofDetected;
    char gwMacFirst[18];
    char gwMacSecond[18];
    bool transparentProxy;
};

struct NetworkInfo {
    bool     ethLinked      = false;
    bool     fullDuplex     = false;
    uint16_t linkSpeed      = 0;
    uint8_t  mac[6]         = {};

    char     ipAddr[16]     = "---";
    char     subnetMask[16] = "---";
    char     gateway[16]    = "---";
    char     dns1[16]       = "---";
    char     dns2[16]       = "---";

    int32_t  pingGatewayMs  = -1;
    int32_t  pingDns1Ms     = -1;
    int32_t  pingDns2Ms     = -1;
    uint8_t  pingLossGw     = 100;

    bool     dnsOk          = false;
    bool     internetOk     = false;

    float    downloadKbps   = 0;
    float    uploadKbps     = 0;

    TestStatus stEth        = TestStatus::PENDING;
    TestStatus stIp         = TestStatus::PENDING;
    TestStatus stGateway    = TestStatus::PENDING;
    TestStatus stDns        = TestStatus::PENDING;
    TestStatus stInternet   = TestStatus::PENDING;
    TestStatus stSpeed      = TestStatus::PENDING;
};

enum class Page {
    SUMMARY,
    IP_DETAILS,
    PING,
    SPEED,
    TRACEROUTE,
    SCANNER,
    SECURITY,
    CABLE,
    HISTORY,
    QR_CODE,
    WEBSERVER_INFO,
    PAGE_COUNT   // sentinel — keep last
};

struct DisplayExtra {
    const TracerouteHop* hops;
    int                  hopCount;
    const ScannedHost*   hosts;
    int                  hostCount;
    int                  scanOffset;
    const CableDiag*     cable;
    const SecurityInfo*  sec;
    const HistoryEntry*  history;
    int                  histCount;
};

void displayInit(LGFX& tft);
void displaySplash(LGFX& tft);
void displayProgress(LGFX& tft, const char* label, uint8_t pct);
void displayPage(LGFX& tft, const NetworkInfo& info, Page page,
                 const DisplayExtra* extra = nullptr);
uint16_t statusColor(TestStatus s);
