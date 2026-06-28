#include "qrcode.h"
#include "display.h"
#include "config.h"
#include <qrcode.h>

void drawQrCodePage(LGFX& tft, const NetworkInfo& info) {
    tft.fillScreen(COL_BG);

    char text[128];
    snprintf(text, sizeof(text),
        "IP=%s GW=%s DL=%.1fMbps PING=%ldms",
        info.ipAddr, info.gateway,
        info.downloadKbps / 1000.0f,
        (long)info.pingGatewayMs);

    QRCode qr;
    uint8_t qrData[qrcode_getBufferSize(3)];
    qrcode_initText(&qr, qrData, 3, ECC_LOW, text);

    int w = tft.width();
    int h = tft.height() - 20;
    int moduleSize = min(w, h) / (qr.size + 4);
    if (moduleSize < 1) moduleSize = 1;
    int offsetX = (w - qr.size * moduleSize) / 2;
    int offsetY = (h - qr.size * moduleSize) / 2;

    tft.fillRect(offsetX - moduleSize * 2, offsetY - moduleSize * 2,
                 qr.size * moduleSize + moduleSize * 4,
                 qr.size * moduleSize + moduleSize * 4, COL_TEXT);

    for (int y = 0; y < qr.size; y++) {
        for (int x = 0; x < qr.size; x++) {
            uint16_t col = qrcode_getModule(&qr, x, y) ? COL_BG : COL_TEXT;
            tft.fillRect(offsetX + x * moduleSize, offsetY + y * moduleSize,
                         moduleSize, moduleSize, col);
        }
    }

    tft.setTextColor(COL_MUTED);
    tft.setTextSize(1);
    tft.setCursor(4, tft.height() - 12);
    tft.print("[UP/DN] Naviguer");
}
