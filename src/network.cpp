#include "network.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>       // Ethernet2 lib for W5500
#include <EthernetUdp.h>
#include <HTTPClient.h>
#include <WiFi.h>           // Pour esp_wifi_... et WiFiClient si besoin
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/icmp.h>
#include <netdb.h>

// ─── Ping ICMP via raw socket ─────────────────────────────────────────────
// Note: nécessite CONFIG_LWIP_IP_RAW_SOCKET=y dans sdkconfig
int32_t pingHost(const char* host, uint16_t timeoutMs) {
    struct hostent* he = gethostbyname(host);
    if (!he) return -1;

    int sock = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    if (sock < 0) return -1;

    struct timeval tv = { .tv_sec = 0, .tv_usec = timeoutMs * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    memcpy(&dest.sin_addr, he->h_addr, he->h_length);

    // Construire paquet ICMP echo request
    uint8_t pkt[64] = {};
    struct icmp_echo_hdr* icmp = (struct icmp_echo_hdr*)pkt;
    icmp->type  = ICMP_ECHO;
    icmp->code  = 0;
    icmp->id    = htons((uint16_t)esp_random());
    icmp->seqno = htons(1);
    // Checksum
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)pkt;
    for (int i = 0; i < (int)(sizeof(pkt) / 2); i++) sum += ntohs(ptr[i]);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    icmp->chksum = htons(~sum);

    unsigned long t0 = millis();
    sendto(sock, pkt, sizeof(pkt), 0, (struct sockaddr*)&dest, sizeof(dest));

    uint8_t buf[128];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    int received = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    close(sock);

    if (received < 0) return -1;
    return (int32_t)(millis() - t0);
}

// ─── Init W5500 via SPI ───────────────────────────────────────────────────
#ifdef ESP32_2432S028R
// Sur ESP32 Yellow l'écran occupe VSPI — W5500 utilise HSPI (SPI2_HOST).
static SPIClass ethSpi(HSPI);
#endif

bool ethInit(NetworkInfo& info) {
#ifdef ESP32_2432S028R
    ethSpi.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
    Ethernet.init(ETH_CS, ethSpi);
#else
    SPI.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
    Ethernet.init(ETH_CS);
#endif

    // Reset matériel
    if (ETH_RST >= 0) {
        pinMode(ETH_RST, OUTPUT);
        digitalWrite(ETH_RST, LOW);
        delay(20);
        digitalWrite(ETH_RST, HIGH);
        delay(50);
    }

    // Tentative DHCP
    if (Ethernet.begin(info.mac, 8000, 4000) == 0) {
        // Vérifier si câble branché
        if (Ethernet.linkStatus() == LinkOFF) {
            info.stEth = TestStatus::FAIL;
            return false;
        }
        // Câble présent mais DHCP échoué → IP statique de secours
        IPAddress fallback(169, 254, 1, 1);
        IPAddress mask(255, 255, 0, 0);
        Ethernet.begin(info.mac, fallback, IPAddress(0,0,0,0), IPAddress(0,0,0,0), mask);
        info.stIp = TestStatus::WARN;
    }

    return true;
}

// ─── Lecture état du lien ─────────────────────────────────────────────────
bool testEthLink(NetworkInfo& info) {
    EthernetLinkStatus ls = Ethernet.linkStatus();
    info.ethLinked = (ls != LinkOFF);

    if (!info.ethLinked) {
        info.stEth = TestStatus::FAIL;
        return false;
    }

    // W5500 : lire registres PHY pour duplex/vitesse
    // PHYCFGR register 0x002E  bit6=speed(1=100M), bit3=duplex(1=full)
    // Via Ethernet.h on accède directement au chip
    uint8_t phycfg = Ethernet.phyRead(0x002E);
    info.fullDuplex = (phycfg & 0x08) != 0;
    info.linkSpeed  = (phycfg & 0x40) ? 100 : 10;

    // MAC
    uint8_t* m = info.mac;
    snprintf((char*)nullptr, 0, ""); // dummy
    // Lire MAC depuis le chip W5500
    Ethernet.MACAddress(info.mac);

    info.stEth = TestStatus::OK;
    return true;
}

bool testIpConfig(NetworkInfo& info) {
    IPAddress ip   = Ethernet.localIP();
    IPAddress mask = Ethernet.subnetMask();
    IPAddress gw   = Ethernet.gatewayIP();
    IPAddress dns1 = Ethernet.dnsServerIP();

    if (ip == IPAddress(0,0,0,0)) {
        info.stIp = TestStatus::FAIL;
        return false;
    }

    ip.toString().toCharArray(info.ipAddr, sizeof(info.ipAddr));
    mask.toString().toCharArray(info.subnetMask, sizeof(info.subnetMask));
    gw.toString().toCharArray(info.gateway, sizeof(info.gateway));
    dns1.toString().toCharArray(info.dns1, sizeof(info.dns1));

    // Détecter APIPA (169.254.x.x) → WARN
    bool apipa = (ip[0] == 169 && ip[1] == 254);
    info.stIp = apipa ? TestStatus::WARN : TestStatus::OK;
    return !apipa;
}

bool testGatewayPing(NetworkInfo& info) {
    if (info.stIp == TestStatus::FAIL) {
        info.stGateway = TestStatus::FAIL;
        return false;
    }
    info.stGateway = TestStatus::RUNNING;

    uint32_t total = 0;
    uint8_t  ok    = 0;
    for (int i = 0; i < PING_COUNT; i++) {
        int32_t rtt = pingHost(info.gateway, PING_TIMEOUT_MS);
        if (rtt >= 0) { total += rtt; ok++; }
        delay(200);
    }
    info.pingLossGw = 100 - (ok * 100 / PING_COUNT);
    info.pingGatewayMs = ok ? (int32_t)(total / ok) : -1;

    info.stGateway = (ok == PING_COUNT) ? TestStatus::OK
                   : (ok > 0)           ? TestStatus::WARN
                                        : TestStatus::FAIL;
    return ok > 0;
}

bool testDns(NetworkInfo& info) {
    struct hostent* he = gethostbyname(DNS_TEST_HOST);
    info.dnsOk = (he != nullptr);
    info.stDns = info.dnsOk ? TestStatus::OK : TestStatus::FAIL;

    // Ping DNS publics
    info.pingDns1Ms = pingHost(PING_HOST_1, PING_TIMEOUT_MS);
    info.pingDns2Ms = pingHost(PING_HOST_2, PING_TIMEOUT_MS);

    return info.dnsOk;
}

bool testInternet(NetworkInfo& info) {
    // Test HTTP HEAD vers un serveur léger
    HTTPClient http;
    http.begin("http://detectportal.firefox.com/success.txt");
    http.setTimeout(5000);
    int code = http.GET();
    http.end();

    info.internetOk = (code == 200);
    info.stInternet = info.internetOk ? TestStatus::OK : TestStatus::FAIL;
    return info.internetOk;
}

bool testSpeed(NetworkInfo& info) {
    info.stSpeed = TestStatus::RUNNING;

    // ─ Download : GET d'un fichier de taille connue ─
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "http://%s/100MB.test", SPEEDTEST_HOST);
    http.begin(url);
    http.setTimeout(15000);

    unsigned long t0 = millis();
    int code = http.GET();
    if (code == 200) {
        WiFiClient* stream = http.getStreamPtr();
        size_t total = 0;
        uint8_t buf[1460];
        unsigned long limit = millis() + 8000; // 8s max
        while (millis() < limit && stream->connected()) {
            int avail = stream->available();
            if (avail > 0) {
                int n = stream->read(buf, min(avail, (int)sizeof(buf)));
                total += n;
            } else {
                delay(1);
            }
            if (total >= (size_t)SPEEDTEST_SIZE_KB * 1024) break;
        }
        unsigned long elapsed = millis() - t0;
        if (elapsed > 0 && total > 0) {
            info.downloadKbps = (float)(total * 8) / elapsed; // kbps
        }
    }
    http.end();

    // ─ Upload : POST de données aléatoires ─
    // (serveur doit accepter POST, optionnel selon infra)
    // On utilise ici un test simplifié basé sur TCP throughput local si gateway dispo
    info.uploadKbps = 0; // à implémenter selon infrastructure cible

    info.stSpeed = (info.downloadKbps > 0) ? TestStatus::OK : TestStatus::WARN;
    return info.downloadKbps > 0;
}

// ─── Orchestrateur ───────────────────────────────────────────────────────────
void runAllTests(NetworkInfo& info, void (*cb)(const char*, uint8_t)) {
    cb("Lien Ethernet...",  5);
    testEthLink(info);

    cb("Configuration IP...", 20);
    testIpConfig(info);

    cb("Ping passerelle...", 40);
    testGatewayPing(info);

    cb("Resolution DNS...", 60);
    testDns(info);

    cb("Connectivite WAN...", 75);
    testInternet(info);

    cb("Test de debit...", 88);
    testSpeed(info);

    cb("Termine", 100);
}
