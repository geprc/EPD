#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

// 将二维码绘制到任意兼容 Adafruit GFX 的帧缓冲区。
// 本类只负责修改缓冲区；何时刷新到物理屏幕由调用方决定。
class QRcode {
   public:
    explicit QRcode(Adafruit_GFX &display);
    void create(const String &message);

   private:
    // 将二维码中的一个黑色模块放大为 kModuleScale × kModuleScale 像素块。
    void drawModule(int x, int y);

    Adafruit_GFX &display_;
};
