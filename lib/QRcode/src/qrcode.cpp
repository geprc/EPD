#include <Arduino.h>
#include "qrcode.h"
#include "qrencode.h"

int offsetsX = 10;
int offsetsY = 10;
int screenwidth = 200;
int screenheight = 200;
int multiply = 4;

QRcode::QRcode(GxEPD_Class *display) {
    this->display = display;
}

void QRcode::render(int x, int y, int color) {
    int i, j;
    x = (x * multiply) + offsetsX;
    y = (y * multiply) + offsetsY;
    if (color == 1) {
        for (i = 0; i < multiply; i++) {
            for (j = 0; j < multiply; j++) {
                display->drawPixel(x + i, y + j, GxEPD_BLACK);
            }
        }
    } else {
        for (i = 0; i < multiply; i++) {
            for (j = 0; j < multiply; j++) {
                display->drawPixel(x + i, y + j, GxEPD_WHITE);
            }
        }
    }
}

void QRcode::create(String message) {
    message.toCharArray((char *)strinbuf, 260);
    qrencode();
    display->fillScreen(GxEPD_WHITE);
    for (byte x = 0; x < WD; x++) {
        for (byte y = 0; y < WD; y++) {
            if (QRBIT(x, y)) {
                render(x, y, 1);
            }
        }
    }
    display->updateWindow(0, 0, 200, 200, false);
}
