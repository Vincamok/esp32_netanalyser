#include "scanner.h"
#include "config.h"
#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

struct OuiEntry {
    uint8_t prefix[3];
    const char* name;
};

static const OuiEntry OUI_TABLE[] = {
    {{0x00, 0x0C, 0x29}, "VMware"},
    {{0x00, 0x50, 0x56}, "VMware"},
    {{0x00, 0x1A, 0x11}, "Google"},
    {{0x00, 0x17, 0xF2}, "Apple"},
    {{0xAC, 0xDE, 0x48}, "Apple"},
    {{0x3C, 0x22, 0xFB}, "Apple"},
    {{0x00, 0x00, 0x0C}, "Cisco"},
    {{0x00, 0x1B, 0x0D}, "Cisco"},
    {{0x00, 0x23, 0xEB}, "Cisco"},
    {{0x8C, 0x8D, 0x28}, "Intel"},
    {{0x00, 0x1E, 0x67}, "Intel"},
    {{0xF4, 0x06, 0xA5}, "Intel"},
    {{0x00, 0x16, 0x32}, "Samsung"},
    {{0x30, 0xCD, 0xA7}, "Samsung"},
    {{0xB4, 0xEF, 0xFA}, "Samsung"},
    {{0xB8, 0x27, 0xEB}, "Raspberry Pi"},
    {{0xDC, 0xA6, 0x32}, "Raspberry Pi"},
    {{0xE4, 0x5F, 0x01}, "Raspberry Pi"},
    {{0x50, 0xC7, 0xBF}, "TP-Link"},
    {{0xEC, 0x17, 0x2F}, "TP-Link"},
    {{0xA0, 0xF3, 0xC1}, "TP-Link"},
    {{0x00, 0x26, 0xF2}, "Netgear"},
    {{0xA0, 0x21, 0xB7}, "Netgear"},
    {{0x20, 0x4E, 0x7F}, "Netgear"},
    {{0x00, 0x1D, 0x7E}, "Linksys"},
    {{0xC0, 0xC1, 0xC0}, "Ubiquiti"},
    {{0x00, 0x15, 0x6D}, "Ubiquiti"},
    {{0x74, 0x83, 0xEF}, "Dell"},
    {{0xF0, 0x1F, 0xAF}, "HP"},
    {{0x3C, 0xD9, 0x2B}, "HP"},
};

static const char* lookupOui(const uint8_t mac[6]) {
    for (const auto& e : OUI_TABLE) {
        if (mac[0] == e.prefix[0] && mac[1] == e.prefix[1] && mac[2] == e.prefix[2]) {
            return e.name;
        }
    }
    return "Unknown";
}

void scannerInit() {}

int arpScan(ScannedHost* hosts, int maxHosts) {
    IPAddress localIp = Ethernet.localIP();
    uint8_t base[4] = { (uint8_t)localIp[0], (uint8_t)localIp[1], (uint8_t)localIp[2], 0 };
    int found = 0;

    // ARP request template
    // We use raw socket approach via UDP broadcast + TCP connect fallback
    // since lwIP on W5500 doesn't expose raw ARP. We detect hosts via TCP connect.
    for (int i = 1; i < 255 && found < maxHosts; i++) {
        if (i == localIp[3]) continue;

        char targetIp[16];
        snprintf(targetIp, sizeof(targetIp), "%d.%d.%d.%d", base[0], base[1], base[2], i);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        struct timeval tv = { .tv_sec = 0, .tv_usec = SCAN_TIMEOUT_MS * 1000 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        inet_aton(targetIp, &addr.sin_addr);

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        connect(sock, (struct sockaddr*)&addr, sizeof(addr));

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        struct timeval tout = { .tv_sec = 0, .tv_usec = SCAN_TIMEOUT_MS * 1000 };
        int ret = select(sock + 1, nullptr, &wset, nullptr, &tout);

        if (ret > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == 0 || err == ECONNREFUSED) {
                ScannedHost& h = hosts[found];
                strncpy(h.ip, targetIp, sizeof(h.ip));
                memset(h.mac, 0, 6);
                strncpy(h.manufacturer, lookupOui(h.mac), sizeof(h.manufacturer));
                h.reachable = true;
                found++;
            }
        }
        close(sock);
    }
    return found;
}

void portScanGateway(const char* gwIp, PortScanResult* results, int* count) {
    static const uint16_t PORTS[] = {22, 23, 25, 53, 80, 443, 445, 3389, 8080, 8443};
    *count = SCANNED_PORTS_COUNT;

    for (int i = 0; i < SCANNED_PORTS_COUNT; i++) {
        results[i].port = PORTS[i];
        results[i].open = false;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORTS[i]);
        inet_aton(gwIp, &addr.sin_addr);

        connect(sock, (struct sockaddr*)&addr, sizeof(addr));

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        struct timeval tv = { .tv_sec = 0, .tv_usec = SCAN_TIMEOUT_MS * 1000 };
        int ret = select(sock + 1, nullptr, &wset, nullptr, &tv);

        if (ret > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            results[i].open = (err == 0);
        }
        close(sock);
    }
}

// DHCP Discover/Offer detection via raw UDP
int detectRogueDhcp(DhcpOffer* offers, int maxOffers, int waitMs) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;

    int bcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));

    struct timeval tv = { .tv_sec = waitMs / 1000, .tv_usec = (waitMs % 1000) * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in src = {};
    src.sin_family = AF_INET;
    src.sin_port = htons(68);
    src.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&src, sizeof(src));

    // Build minimal DHCPDISCOVER
    uint8_t pkt[300] = {};
    pkt[0] = 1;   // BOOTREQUEST
    pkt[1] = 1;   // Ethernet
    pkt[2] = 6;   // MAC length
    pkt[3] = 0;   // hops
    pkt[4] = 0xDE; pkt[5] = 0xAD; pkt[6] = 0xBE; pkt[7] = 0xEF; // xid
    // chaddr (offset 28): use random MAC
    pkt[28] = 0x02; pkt[29] = 0xAA; pkt[30] = 0xBB; pkt[31] = 0xCC; pkt[32] = 0xDD; pkt[33] = 0xEE;
    // Magic cookie at offset 236
    pkt[236] = 99; pkt[237] = 130; pkt[238] = 83; pkt[239] = 99;
    // Option 53: DHCP Discover
    pkt[240] = 53; pkt[241] = 1; pkt[242] = 1;
    // End option
    pkt[243] = 255;

    struct sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(67);
    dst.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(sock, pkt, 300, 0, (struct sockaddr*)&dst, sizeof(dst));

    int found = 0;
    uint8_t buf[600];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);

    unsigned long deadline = millis() + waitMs;
    while (millis() < deadline && found < maxOffers) {
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
        if (n > 240 && buf[0] == 2) { // BOOTREPLY
            // Verify xid matches
            if (buf[4] == 0xDE && buf[5] == 0xAD && buf[6] == 0xBE && buf[7] == 0xEF) {
                DhcpOffer& o = offers[found];
                snprintf(o.serverIp, sizeof(o.serverIp), "%d.%d.%d.%d",
                    buf[20], buf[21], buf[22], buf[23]);
                snprintf(o.offeredIp, sizeof(o.offeredIp), "%d.%d.%d.%d",
                    buf[16], buf[17], buf[18], buf[19]);
                inet_ntoa_r(from.sin_addr, o.serverIp, sizeof(o.serverIp));
                memset(o.serverMac, 0, 6);
                found++;
            }
        }
    }
    close(sock);
    return found;
}

bool sendWakeOnLan(const uint8_t mac[6]) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    int bcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));

    uint8_t magic[102];
    memset(magic, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(&magic[6 + i * 6], mac, 6);
    }

    struct sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(9);
    dst.sin_addr.s_addr = INADDR_BROADCAST;

    int ret = sendto(sock, magic, sizeof(magic), 0, (struct sockaddr*)&dst, sizeof(dst));
    close(sock);
    return ret == sizeof(magic);
}

int listenMdns(char* servicesBuf, int bufSize, int waitMs) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct timeval tv = { .tv_sec = waitMs / 1000, .tv_usec = (waitMs % 1000) * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5353);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    struct ip_mreq mreq;
    inet_aton("224.0.0.251", &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    int written = 0;
    uint8_t buf[512];
    int count = 0;

    static const char* services[] = {"_http._tcp", "_printer._tcp", "_smb._tcp"};

    unsigned long deadline = millis() + waitMs;
    while (millis() < deadline) {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) continue;

        // Simple mDNS parse: scan for known service strings in packet
        for (int s = 0; s < 3; s++) {
            const char* svc = services[s];
            int slen = strlen(svc);
            for (int i = 0; i < n - slen; i++) {
                if (memcmp(&buf[i], svc, slen) == 0) {
                    int avail = bufSize - written - 1;
                    if (avail > slen + 2) {
                        if (written > 0) servicesBuf[written++] = ',';
                        memcpy(&servicesBuf[written], svc, slen);
                        written += slen;
                        servicesBuf[written] = '\0';
                        count++;
                    }
                    break;
                }
            }
        }
    }

    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(sock);
    return count;
}
