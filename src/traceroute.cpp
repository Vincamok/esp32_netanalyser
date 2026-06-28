#include "traceroute.h"
#include <Arduino.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/icmp.h>

int runTraceroute(const char* dest, TracerouteHop* hops, int maxHops) {
    struct hostent* he = gethostbyname(dest);
    if (!he) return 0;

    struct in_addr destAddr;
    memcpy(&destAddr, he->h_addr, he->h_length);

    int hopCount = 0;
    uint16_t pktId = (uint16_t)(esp_random() & 0xFFFF);

    for (int ttl = 1; ttl <= maxHops; ttl++) {
        int sock = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
        if (sock < 0) break;

        setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

        struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        uint8_t pkt[64] = {};
        struct icmp_echo_hdr* icmp = (struct icmp_echo_hdr*)pkt;
        icmp->type  = ICMP_ECHO;
        icmp->code  = 0;
        icmp->id    = htons(pktId);
        icmp->seqno = htons((uint16_t)ttl);

        uint32_t sum = 0;
        uint16_t* ptr = (uint16_t*)pkt;
        for (int i = 0; i < (int)(sizeof(pkt) / 2); i++) sum += ntohs(ptr[i]);
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        icmp->chksum = htons(~sum);

        struct sockaddr_in dst = {};
        dst.sin_family = AF_INET;
        dst.sin_addr = destAddr;

        unsigned long t0 = millis();
        sendto(sock, pkt, sizeof(pkt), 0, (struct sockaddr*)&dst, sizeof(dst));

        uint8_t buf[128];
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
        close(sock);

        TracerouteHop& hop = hops[hopCount];
        hop.rttMs = (int32_t)(millis() - t0);

        if (n > 0) {
            hop.responded = true;
            inet_ntoa_r(from.sin_addr, hop.ip, sizeof(hop.ip));
        } else {
            hop.responded = false;
            strncpy(hop.ip, "*", sizeof(hop.ip));
            hop.rttMs = -1;
        }
        hopCount++;

        if (n > 0 && from.sin_addr.s_addr == destAddr.s_addr) {
            break;
        }
    }
    return hopCount;
}
