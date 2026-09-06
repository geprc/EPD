#include "qrcode.h"

#include "qrencode.h"

namespace {
// 内置编码器输出 45 × 45 模块；放大 4 倍并留出 10 像素边距后可放入 200 × 200 屏幕。
constexpr int kOffsetX = 10;
constexpr int kOffsetY = 10;
constexpr int kModuleScale = 4;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
}  // namespace

QRcode::QRcode(Adafruit_GFX &display) : display_(display) {}

void QRcode::drawModule(int x, int y) {
    const int left = x * kModuleScale + kOffsetX;
    const int top = y * kModuleScale + kOffsetY;
    display_.fillRect(left, top, kModuleScale, kModuleScale, kBlack);
}

void QRcode::create(const String &message) {
    // qrencode() 使用旧版 C 编码器提供的全局输入/输出缓冲区。
    // 传入 260 可确保目标缓冲区保留字符串结尾的 NUL。
    message.toCharArray(reinterpret_cast<char *>(strinbuf), 260);
    qrencode();

    display_.fillScreen(kWhite);
    for (byte x = 0; x < WD; ++x) {
        for (byte y = 0; y < WD; ++y) {
            if (QRBIT(x, y)) drawModule(x, y);
        }
    }
}
