#include "diagnostics.h"
#include "network.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/icmp.h>
#include <math.h>

float measureJitter(const char* host, int count) {
    int32_t rtts[20];
    int valid = 0;

    for (int i = 0; i < count && i < 20; i++) {
        int32_t rtt = pingHost(host, 2000);
        if (rtt >= 0) rtts[valid++] = rtt;
        delay(100);
    }
    if (valid < 2) return -1.0f;

    float mean = 0;
    for (int i = 0; i < valid; i++) mean += rtts[i];
    mean /= valid;

    float variance = 0;
    for (int i = 0; i < valid; i++) {
        float d = rtts[i] - mean;
        variance += d * d;
    }
    variance /= valid;
    return sqrtf(variance);
}

// Send ICMP with DF bit set and given payload size; return true if reply received without fragmentation
static bool icmpProbe(const char* host, int payloadSize) {
    struct hostent* he = gethostbyname(host);
    if (!he) return false;

    int sock = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    if (sock < 0) return false;

    // Set DF bit
    int val = IP_PMTUDISC_DO;
    setsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pktSize = sizeof(struct icmp_echo_hdr) + payloadSize;
    uint8_t* pkt = (uint8_t*)malloc(pktSize);
    if (!pkt) { close(sock); return false; }
    memset(pkt, 0xAB, pktSize);

    struct icmp_echo_hdr* icmp = (struct icmp_echo_hdr*)pkt;
    icmp->type  = ICMP_ECHO;
    icmp->code  = 0;
    icmp->id    = htons(0x1234);
    icmp->seqno = htons(1);
    icmp->chksum = 0;

    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)pkt;
    for (int i = 0; i < pktSize / 2; i++) sum += ntohs(ptr[i]);
    if (pktSize & 1) sum += ((uint8_t*)pkt)[pktSize - 1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    icmp->chksum = htons(~sum);

    struct sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    memcpy(&dst.sin_addr, he->h_addr, he->h_length);

    sendto(sock, pkt, pktSize, 0, (struct sockaddr*)&dst, sizeof(dst));
    free(pkt);

    uint8_t buf[256];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    close(sock);

    return n > 0;
}

int findOptimalMtu(const char* host) {
    int lo = 576, hi = 1500, best = 576;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        // ICMP overhead: 28 bytes (IP header 20 + ICMP header 8)
        int payload = mid - 28;
        if (payload < 0) payload = 0;
        if (icmpProbe(host, payload)) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
        delay(50);
    }
    return best;
}

float measureTcpThroughput(const char* serverIp, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0.0f;

    struct sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    inet_aton(serverIp, &dst.sin_addr);

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr*)&dst, sizeof(dst)) < 0) {
        close(sock);
        return 0.0f;
    }

    const int BLOCK = 1460;
    uint8_t buf[BLOCK];
    memset(buf, 0xAA, BLOCK);

    size_t total = 0;
    unsigned long t0 = millis();
    unsigned long limit = t0 + 5000;

    while (millis() < limit) {
        int n = send(sock, buf, BLOCK, 0);
        if (n <= 0) break;
        total += n;
    }
    unsigned long elapsed = millis() - t0;
    close(sock);

    if (elapsed == 0 || total == 0) return 0.0f;
    return (float)(total * 8) / elapsed; // kbps
}

bool detectCaptivePortal() {
    HTTPClient http;
    http.begin("http://connectivitycheck.gstatic.com/generate_204");
    http.setTimeout(5000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    int code = http.GET();
    http.end();
    return (code != 204 && code > 0);
}
