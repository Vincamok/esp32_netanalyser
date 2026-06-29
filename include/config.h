#pragma once

// ─── Mode carte réseau : définir l'un des deux
// #define USE_DP83848_RMII
#define USE_W5500_SPI

#if defined(ESP32_YELLOW)
// ────────────────────────────────────────────────────────────────────────────
// ESP32 Yellow (CYD 2.4" ILI9341) — W5500 sur HSPI (pins libres)
// ────────────────────────────────────────────────────────────────────────────

// W5500 SPI → HSPI (SPI2_HOST) pour ne pas entrer en conflit avec l'écran
#define ETH_MOSI    23
#define ETH_MISO    19
#define ETH_SCK     18
#define ETH_CS       5
#define ETH_INT     17
#define ETH_RST     16

// Écran ILI9341 → VSPI (SPI3_HOST)
// Câblage standard CYD 2.4"
#define TFT_CS      15
#define TFT_DC       2
#define TFT_RST     -1
#define TFT_BL      21
#define TFT_MOSI    13
#define TFT_MISO    12
#define TFT_SCLK    14

// Boutons : BOOT (GPIO 0) + entrée seule GPIO 35
#define BTN_UP       0
#define BTN_DOWN    35

#elif defined(USE_DP83848_RMII)
// ────────────────────────────────────────────────────────────────────────────
// DP83848 RMII (ESP32-S3 EMAC)
// ────────────────────────────────────────────────────────────────────────────
// (pas de pins SPI W5500 ici)
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_RMII_CLK     0
#define ETH_POWER_PIN   -1
#define ETH_TYPE        ETH_PHY_DP83848
#define ETH_ADDR         1

// Écran TFT T-Display-S3
#define TFT_CS       6
#define TFT_DC       7
#define TFT_RST      5
#define TFT_BL      38
#define TFT_MOSI    17
#define TFT_SCLK    18

#define BTN_UP       0
#define BTN_DOWN    14

#else
// ────────────────────────────────────────────────────────────────────────────
// LilyGo T-Display-S3 + W5500 SPI (défaut)
// ────────────────────────────────────────────────────────────────────────────

// W5500 SPI
#define ETH_MOSI    11
#define ETH_MISO    13
#define ETH_SCK     12
#define ETH_CS      10
#define ETH_INT      9
#define ETH_RST      8

// Écran ST7789 T-Display-S3
#define TFT_CS       6
#define TFT_DC       7
#define TFT_RST      5
#define TFT_BL      38
#define TFT_MOSI    17
#define TFT_SCLK    18

#define BTN_UP       0
#define BTN_DOWN    14

#endif

// ─── Tests réseau
#define PING_HOST_1         "8.8.8.8"
#define PING_HOST_2         "1.1.1.1"
#define SPEEDTEST_HOST      "ipv4.ikoula.com"
#define SPEEDTEST_SIZE_KB   512
#define DNS_TEST_HOST       "www.google.fr"
#define IPERF_SERVER_IP     "192.168.1.1"
#define IPERF_SERVER_PORT   5001

// ─── Timing
#define PING_COUNT          4
#define PING_TIMEOUT_MS     2000
#define REFRESH_INTERVAL_MS 30000
#define SCREEN_TIMEOUT_MS   60000
#define SCAN_TIMEOUT_MS     500
#define BTN_LONGPRESS_MS    1000

// ─── Scanner / diagnostics
#define TRACEROUTE_MAX_HOPS 15
#define MTU_MIN             576
#define MTU_MAX             1500
#define HISTORY_MAX_ENTRIES 10
#define WEBSERVER_PORT      80
