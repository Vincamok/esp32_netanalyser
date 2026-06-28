#include "cable_diag.h"
#include <Arduino.h>

#ifdef USE_DP83848_RMII
#include <esp_eth.h>
#include <driver/gpio.h>

// DP83848 register addresses
#define DP83848_CDCTRL1  0x12
#define DP83848_CDSTE    0x13
#define DP83848_CDPSR    0x14

static esp_eth_handle_t s_ethHandle = nullptr;

struct PhyRegOp { uint32_t reg; uint32_t val; };

static uint16_t phyRead(uint8_t reg) {
    PhyRegOp op = { reg, 0 };
    if (s_ethHandle) {
        esp_eth_ioctl(s_ethHandle, ETH_CMD_READ_PHY_REG, &op);
    }
    return (uint16_t)op.val;
}

static void phyWrite(uint8_t reg, uint16_t val) {
    PhyRegOp op = { reg, (uint32_t)val };
    if (s_ethHandle) {
        esp_eth_ioctl(s_ethHandle, ETH_CMD_WRITE_PHY_REG, &op);
    }
}

static PairStatus decodePairStatus(uint8_t bits) {
    switch (bits & 0x03) {
        case 0: return PairStatus::OK;
        case 1: return PairStatus::OPEN;
        case 2: return PairStatus::SHORT;
        case 3: return PairStatus::CROSS;
        default: return PairStatus::UNKNOWN;
    }
}

CableDiag runCableDiag() {
    CableDiag d;
    d.supported = true;

    // Trigger TDR measurement by writing to CDCTRL1
    uint16_t ctrl = phyRead(DP83848_CDCTRL1);
    ctrl |= (1 << 15); // TDR_START bit
    phyWrite(DP83848_CDCTRL1, ctrl);
    delay(200);

    uint16_t cdctrl1 = phyRead(DP83848_CDCTRL1);
    // Bits [1:0] = pair A, [3:2] = pair B, [5:4] = pair C, [7:6] = pair D
    d.pairA = decodePairStatus((cdctrl1 >> 0) & 0x03);
    d.pairB = decodePairStatus((cdctrl1 >> 2) & 0x03);
    d.pairC = decodePairStatus((cdctrl1 >> 4) & 0x03);
    d.pairD = decodePairStatus((cdctrl1 >> 6) & 0x03);

    uint16_t cdste = phyRead(DP83848_CDSTE);
    uint16_t cdpsr = phyRead(DP83848_CDPSR);
    uint16_t tdrCount = (cdste & 0xFF) | ((cdpsr & 0x03) << 8);
    d.lengthMeters = tdrCount * 0.8f;

    return d;
}

#else

CableDiag runCableDiag() {
    CableDiag d;
    d.supported  = false;
    d.pairA      = PairStatus::UNKNOWN;
    d.pairB      = PairStatus::UNKNOWN;
    d.pairC      = PairStatus::UNKNOWN;
    d.pairD      = PairStatus::UNKNOWN;
    d.lengthMeters = -1.0f;
    return d;
}

#endif

const char* pairStatusStr(PairStatus s) {
    switch (s) {
        case PairStatus::OK:      return "OK";
        case PairStatus::OPEN:    return "OPEN";
        case PairStatus::SHORT:   return "SHORT";
        case PairStatus::CROSS:   return "CROSS";
        default:                  return "N/A";
    }
}
