// class GxGDEM029T94 : Display class for GDEM029T94 e-Paper from Dalian Good Display Co., Ltd.: www.e-paper-display.com
//
// based on Demo Example from Good Display, available here: http://www.e-paper-display.com/download_detail/downloadsId=806.html
// Controller : SSD1681 : http://www.e-paper-display.com/download_detail/downloadsId=825.html
//
// Author : J-M Zingg
//
// Version : see library.properties
//
// License: GNU GENERAL PUBLIC LICENSE V3, see LICENSE
//
// Library: https://github.com/ZinggJM/GxEPD

#include "GxGDEM029T94.h"

//#define DISABLE_DIAGNOSTIC_OUTPUT

#if defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

// Partial Update Delay, may have an influence on degradation
#define GxGDEM029T94_PU_DELAY 300

GxGDEM029T94::GxGDEM029T94(GxIO& io, int8_t rst, int8_t busy) :
  GxEPD(GxGDEM029T94_WIDTH, GxGDEM029T94_HEIGHT), IO(io),
  _current_page(-1), _using_partial_mode(false), _diag_enabled(false),
  _rst(rst), _busy(busy)
{
}

void GxGDEM029T94::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;

  // check rotation, move pixel around if necessary
  switch (getRotation())
  {
    case 1:
      swap(x, y);
      x = GxGDEM029T94_WIDTH - x - 1;
      break;
    case 2:
      x = GxGDEM029T94_WIDTH - x - 1;
      y = GxGDEM029T94_HEIGHT - y - 1;
      break;
    case 3:
      swap(x, y);
      y = GxGDEM029T94_HEIGHT - y - 1;
      break;
  }
  uint16_t i = x / 8 + y * GxGDEM029T94_WIDTH / 8;
  if (_current_page < 1)
  {
    if (i >= sizeof(_buffer)) return;
  }
  else
  {
    y -= _current_page * GxGDEM029T94_PAGE_HEIGHT;
    if ((y < 0) || (y >= GxGDEM029T94_PAGE_HEIGHT)) return;
    i = x / 8 + y * GxGDEM029T94_WIDTH / 8;
  }

  if (!color)
    _buffer[i] = (_buffer[i] | (1 << (7 - x % 8)));
  else
    _buffer[i] = (_buffer[i] & (0xFF ^ (1 << (7 - x % 8))));
}

void GxGDEM029T94::init(uint32_t serial_diag_bitrate)
{
  if (serial_diag_bitrate > 0)
  {
    Serial.begin(serial_diag_bitrate);
    _diag_enabled = true;
  }
  IO.init();
  IO.setFrequency(4000000); // 4MHz
  if (_rst >= 0)
  {
    digitalWrite(_rst, HIGH);
    pinMode(_rst, OUTPUT);
  }
  if (_busy >= 0) pinMode(_busy, INPUT);
  fillScreen(GxEPD_WHITE);
  _current_page = -1;
  _using_partial_mode = false;
}

void GxGDEM029T94::fillScreen(uint16_t color)
{
  uint8_t data = (color == GxEPD_BLACK) ? 0xFF : 0x00;
  for (uint16_t x = 0; x < sizeof(_buffer); x++)
  {
    _buffer[x] = data;
  }
}

void GxGDEM029T94::update(void)
{
  if (_current_page != -1) return;
  _using_partial_mode = false;
  _Init_Full(0x03);
  _writeCommand(0x24);
  for (uint16_t y = 0; y < GxGDEM029T94_HEIGHT; y++)
  {
    for (uint16_t x = 0; x < GxGDEM029T94_WIDTH / 8; x++)
    {
      uint16_t idx = y * (GxGDEM029T94_WIDTH / 8) + x;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      _writeData(~data);
    }
  }
  _writeCommand(0x26); // both buffers same for full b/w
  for (uint16_t y = 0; y < GxGDEM029T94_HEIGHT; y++)
  {
    for (uint16_t x = 0; x < GxGDEM029T94_WIDTH / 8; x++)
    {
      uint16_t idx = y * (GxGDEM029T94_WIDTH / 8) + x;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      _writeData(~data);
    }
  }
  _Update_Full();
  _PowerOff();
}

void  GxGDEM029T94::drawBitmap(const uint8_t *bitmap, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, int16_t mode)
{
  if (mode & bm_default) mode |= bm_flip_x | bm_invert;
  drawBitmapBM(bitmap, x, y, w, h, color, mode);
}

void GxGDEM029T94::drawBitmap(const uint8_t *bitmap, uint32_t size, int16_t mode)
{
  if (_current_page != -1) return;
  // example bitmaps are made for y-decrement, x-increment, for origin on opposite corner
  // bm_flip_x for normal display (bm_flip_y would be rotated)
  if (mode & bm_default) mode |= bm_flip_x;
  uint8_t ram_entry_mode = 0x03; // y-increment, x-increment for normal mode
  if ((mode & bm_flip_y) && (mode & bm_flip_x)) ram_entry_mode = 0x00; // y-decrement, x-decrement
  else if (mode & bm_flip_y) ram_entry_mode = 0x01; // y-decrement, x-increment
  else if (mode & bm_flip_x) ram_entry_mode = 0x02; // y-increment, x-decrement
  if (mode & bm_partial_update)
  {
    _using_partial_mode = true; // remember
    _Init_Part(ram_entry_mode);
    _writeCommand(0x24);
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      uint8_t data = 0xFF; // white is 0xFF on device
      if (i < size)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[i]);
#else
        data = bitmap[i];
#endif
        if (mode & bm_invert) data = ~data;
      }
      _writeData(data);
    }
    _Update_Part();
    delay(GxGDEM029T94_PU_DELAY);
    // update previous buffer
    _writeCommand(0x24);
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      uint8_t data = 0xFF; // white is 0xFF on device
      if (i < size)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[i]);
#else
        data = bitmap[i];
#endif
        if (mode & bm_invert) data = ~data;
      }
      _writeData(data);
    }
    delay(GxGDEM029T94_PU_DELAY);
    _PowerOff();
  }
  else
  {
    _using_partial_mode = false; // remember
    _Init_Full(ram_entry_mode);
    _writeCommand(0x24);
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      uint8_t data = 0xFF; // white is 0xFF on device
      if (i < size)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[i]);
#else
        data = bitmap[i];
#endif
        if (mode & bm_invert) data = ~data;
      }
      _writeData(data);
    }
    _writeCommand(0x26); // both buffers same for full b/w
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      uint8_t data = 0xFF; // white is 0xFF on device
      if (i < size)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[i]);
#else
        data = bitmap[i];
#endif
        if (mode & bm_invert) data = ~data;
      }
      _writeData(data);
    }
    _Update_Full();
    _PowerOff();
  }
}

void GxGDEM029T94::eraseDisplay(bool using_partial_update)
{
  if (_current_page != -1) return;
  if (using_partial_update)
  {
    _using_partial_mode = true; // remember
    _Init_Part(0x01);
    _writeCommand(0x24);
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      _writeData(0xFF);
    }
    _Update_Part();
    delay(GxGDEM029T94_PU_DELAY);
    // update previous buffer
    _writeCommand(0x24);
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      _writeData(0xFF);
    }
    delay(GxGDEM029T94_PU_DELAY);
  }
  else
  {
    _using_partial_mode = false; // remember
    _Init_Full(0x01);
    _writeCommand(0x24);
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      _writeData(0xFF);
    }
    _writeCommand(0x26); // both buffers same for full b/w
    for (uint32_t i = 0; i < GxGDEM029T94_BUFFER_SIZE; i++)
    {
      _writeData(0xFF);
    }
    _Update_Full();
    _PowerOff();
  }
}

void GxGDEM029T94::updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool using_rotation)
{
  if (_current_page != -1) return;
  if (using_rotation) _rotate(x, y, w, h);
  if (x >= GxGDEM029T94_WIDTH) return;
  if (y >= GxGDEM029T94_HEIGHT) return;
  uint16_t xe = gx_uint16_min(GxGDEM029T94_WIDTH, x + w) - 1;
  uint16_t ye = gx_uint16_min(GxGDEM029T94_HEIGHT, y + h) - 1;
  uint16_t xs_d8 = x / 8;
  uint16_t xe_d8 = xe / 8;
  _Init_Part(0x03);
  _SetRamArea(xs_d8, xe_d8, y % 256, y / 256, ye % 256, ye / 256); // X-source area,Y-gate area
  _SetRamPointer(xs_d8, y % 256, y / 256); // set ram
  _waitWhileBusy(0, 100); // needed ?
  _writeCommand(0x24);
  for (int16_t y1 = y; y1 <= ye; y1++)
  {
    for (int16_t x1 = xs_d8; x1 <= xe_d8; x1++)
    {
      uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      _writeData(~data);
    }
  }
  _Update_Part();
  delay(GxGDEM029T94_PU_DELAY);
  // update previous buffer
  _SetRamArea(xs_d8, xe_d8, y % 256, y / 256, ye % 256, ye / 256); // X-source area,Y-gate area
  _SetRamPointer(xs_d8, y % 256, y / 256); // set ram
  _waitWhileBusy(0, 100); // needed ?
  _writeCommand(0x24);
  for (int16_t y1 = y; y1 <= ye; y1++)
  {
    for (int16_t x1 = xs_d8; x1 <= xe_d8; x1++)
    {
      uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      _writeData(~data);
    }
  }
  delay(GxGDEM029T94_PU_DELAY);
}

void GxGDEM029T94::_writeToWindow(uint16_t xs, uint16_t ys, uint16_t xd, uint16_t yd, uint16_t w, uint16_t h)
{
  //Serial.printf("_writeToWindow(%d, %d, %d, %d, %d, %d)\n", xs, ys, xd, yd, w, h);
  // the screen limits are the hard limits
  if (xs >= GxGDEM029T94_WIDTH) return;
  if (ys >= GxGDEM029T94_HEIGHT) return;
  if (xd >= GxGDEM029T94_WIDTH) return;
  if (yd >= GxGDEM029T94_HEIGHT) return;
  w = gx_uint16_min(w, GxGDEM029T94_WIDTH - xs);
  w = gx_uint16_min(w, GxGDEM029T94_WIDTH - xd);
  h = gx_uint16_min(h, GxGDEM029T94_HEIGHT - ys);
  h = gx_uint16_min(h, GxGDEM029T94_HEIGHT - yd);
  uint16_t xds_d8 = xd / 8;
  uint16_t xde_d8 = (xd + w - 1) / 8;
  uint16_t yde = yd + h - 1;
  // soft limits, must send as many bytes as set by _SetRamArea
  uint16_t xse_d8 = xs / 8 + xde_d8 - xds_d8;
  uint16_t yse = ys + h - 1;
  _SetRamArea(xds_d8, xde_d8, yd % 256, yd / 256, yde % 256, yde / 256); // X-source area,Y-gate area
  _SetRamPointer(xds_d8, yd % 256, yd / 256); // set ram
  _waitWhileBusy(0, 100); // needed ?
  _writeCommand(0x24);
  for (int16_t y1 = ys; y1 <= yse; y1++)
  {
    for (int16_t x1 = xs / 8; x1 <= xse_d8; x1++)
    {
      uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      _writeData(~data);
    }
  }
}

void GxGDEM029T94::updateToWindow(uint16_t xs, uint16_t ys, uint16_t xd, uint16_t yd, uint16_t w, uint16_t h, bool using_rotation)
{
  if (using_rotation)
  {
    switch (getRotation())
    {
      case 1:
        swap(xs, ys);
        swap(xd, yd);
        swap(w, h);
        xs = GxGDEM029T94_WIDTH - xs - w - 1;
        xd = GxGDEM029T94_WIDTH - xd - w - 1;
        break;
      case 2:
        xs = GxGDEM029T94_WIDTH - xs - w - 1;
        ys = GxGDEM029T94_HEIGHT - ys - h - 1;
        xd = GxGDEM029T94_WIDTH - xd - w - 1;
        yd = GxGDEM029T94_HEIGHT - yd - h - 1;
        break;
      case 3:
        swap(xs, ys);
        swap(xd, yd);
        swap(w, h);
        ys = GxGDEM029T94_HEIGHT - ys  - h - 1;
        yd = GxGDEM029T94_HEIGHT - yd  - h - 1;
        break;
    }
  }
  _Init_Part(0x03);
  _writeToWindow(xs, ys, xd, yd, w, h);
  _Update_Part();
  delay(GxGDEM029T94_PU_DELAY);
  // update previous buffer
  _writeToWindow(xs, ys, xd, yd, w, h);
  delay(GxGDEM029T94_PU_DELAY);
}

void GxGDEM029T94::powerDown()
{
  _using_partial_mode = false;
  _PowerOff();
}

void GxGDEM029T94::_writeCommand(uint8_t command)
{
  if ((_busy >= 0) && digitalRead(_busy))
  {
    String str = String("command 0x") + String(command, HEX);
    _waitWhileBusy(str.c_str(), 100); // needed?
  }
  IO.writeCommandTransaction(command);
}

void GxGDEM029T94::_writeData(uint8_t data)
{
  IO.writeDataTransaction(data);
}

void GxGDEM029T94::_writeCommandData(const uint8_t* pCommandData, uint8_t datalen)
{
  if ((_busy >= 0) && digitalRead(_busy))
  {
    String str = String("command 0x") + String(pCommandData[0], HEX);
    _waitWhileBusy(str.c_str(), 100); // needed?
  }
  IO.startTransaction();
  IO.writeCommand(*pCommandData++);
  for (uint8_t i = 0; i < datalen - 1; i++)  // sub the command
  {
    IO.writeData(*pCommandData++);
  }
  IO.endTransaction();

}

void GxGDEM029T94::_waitWhileBusy(const char* comment, uint16_t busy_time)
{
  if (_busy >= 0)
  {
    unsigned long start = micros();
    while (1)
    {
      if (!digitalRead(_busy)) break;
      delay(1);
      if (micros() - start > 10000000)
      {
        if (_diag_enabled) Serial.println("Busy Timeout!");
        break;
      }
    }
    if (comment)
    {
#if !defined(DISABLE_DIAGNOSTIC_OUTPUT)
      if (_diag_enabled)
      {
        unsigned long elapsed = micros() - start;
        Serial.print(comment);
        Serial.print(" : ");
        Serial.println(elapsed);
      }
#endif
    }
    (void) start;
  }
  else delay(busy_time);
}

void GxGDEM029T94::_setRamDataEntryMode(uint8_t em)
{
  const uint16_t xPixelsPar = GxGDEM029T94_X_PIXELS - 1;
  const uint16_t yPixelsPar = GxGDEM029T94_Y_PIXELS - 1;
  em = gx_uint16_min(em, 0x03);
  _writeCommand(0x11);
  _writeData(em);
  switch (em)
  {
    case 0x00: // x decrease, y decrease
      _SetRamArea(xPixelsPar / 8, 0x00, yPixelsPar % 256, yPixelsPar / 256, 0x00, 0x00);  // X-source area,Y-gate area
      _SetRamPointer(xPixelsPar / 8, yPixelsPar % 256, yPixelsPar / 256); // set ram
      break;
    case 0x01: // x increase, y decrease : as in demo code
      _SetRamArea(0x00, xPixelsPar / 8, yPixelsPar % 256, yPixelsPar / 256, 0x00, 0x00);  // X-source area,Y-gate area
      _SetRamPointer(0x00, yPixelsPar % 256, yPixelsPar / 256); // set ram
      break;
    case 0x02: // x decrease, y increase
      _SetRamArea(xPixelsPar / 8, 0x00, 0x00, 0x00, yPixelsPar % 256, yPixelsPar / 256);  // X-source area,Y-gate area
      _SetRamPointer(xPixelsPar / 8, 0x00, 0x00); // set ram
      break;
    case 0x03: // x increase, y increase : normal mode
      _SetRamArea(0x00, xPixelsPar / 8, 0x00, 0x00, yPixelsPar % 256, yPixelsPar / 256);  // X-source area,Y-gate area
      _SetRamPointer(0x00, 0x00, 0x00); // set ram
      break;
  }
}

void GxGDEM029T94::_SetRamArea(uint8_t Xstart, uint8_t Xend, uint8_t Ystart, uint8_t Ystart1, uint8_t Yend, uint8_t Yend1)
{
  _writeCommand(0x44);
  _writeData(Xstart);
  _writeData(Xend);
  _writeCommand(0x45);
  _writeData(Ystart);
  _writeData(Ystart1);
  _writeData(Yend);
  _writeData(Yend1);
}

void GxGDEM029T94::_SetRamPointer(uint8_t addrX, uint8_t addrY, uint8_t addrY1)
{
  _writeCommand(0x4e);
  _writeData(addrX);
  _writeCommand(0x4f);
  _writeData(addrY);
  _writeData(addrY1);
}

void GxGDEM029T94::_PowerOn(void)
{
  _writeCommand(0x22);
  _writeData(0xf8);
  _writeCommand(0x20);
  _waitWhileBusy("_PowerOn", power_on_time);
}

void GxGDEM029T94::_PowerOff(void)
{
  _writeCommand(0x22);
  _writeData(0x83);
  _writeCommand(0x20);
  _waitWhileBusy("_PowerOff", power_off_time);
}

void GxGDEM029T94::_InitDisplay(uint8_t em)
{
  _writeCommand(0x12);  //SWRESET
  delay(10); // 10ms according to specs
  _writeCommand(0x01); //Driver output control
  _writeData(0x27);
  _writeData(0x01);
  _writeData(0x00);
  _writeCommand(0x3C); //BorderWavefrom
  _writeData(0x05);
  _writeCommand(0x21); //  Display update control
  _writeData(0x00);
  _writeData(0x80);
  _writeCommand(0x18); //Read built-in temperature sensor
  _writeData(0x80);
  _setRamDataEntryMode(em);
}

void GxGDEM029T94::_Init_Full(uint8_t em)
{
  _InitDisplay(em);
  _PowerOn();
}

void GxGDEM029T94::_Init_Part(uint8_t em)
{
  _InitDisplay(em);
  _PowerOn();
}

void GxGDEM029T94::_Update_Full(void)
{
  _writeCommand(0x22);
  _writeData(0xf4);
  _writeCommand(0x20);
  _waitWhileBusy("_Update_Full", full_refresh_time);
}

void GxGDEM029T94::_Update_Part(void)
{
  _writeCommand(0x22);
  _writeData(0xfc);
  _writeCommand(0x20);
  _waitWhileBusy("_Update_Part", partial_refresh_time);
}

void GxGDEM029T94::drawPaged(void (*drawCallback)(void))
{
  if (_current_page != -1) return;
  _using_partial_mode = false;
  _Init_Full(0x03);
  for (uint8_t cmd = 0x24; ; cmd = 0x26) // both buffers same for full b/w
  {
    _writeCommand(cmd);
    for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback();
      for (int16_t y1 = 0; y1 < GxGDEM029T94_PAGE_HEIGHT; y1++)
      {
        for (int16_t x1 = 0; x1 < GxGDEM029T94_WIDTH / 8; x1++)
        {
          uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
          uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
          _writeData(~data);
        }
      }
    }
    _current_page = -1;
    if (cmd == 0x26) break;
  }
  _Update_Full();
  _PowerOff();
}

void GxGDEM029T94::drawPaged(void (*drawCallback)(uint32_t), uint32_t p)
{
  if (_current_page != -1) return;
  _using_partial_mode = false;
  _Init_Full(0x03);
  for (uint8_t cmd = 0x24; ; cmd = 0x26) // both buffers same for full b/w
  {
    _writeCommand(cmd);
    for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p);
      for (int16_t y1 = 0; y1 < GxGDEM029T94_PAGE_HEIGHT; y1++)
      {
        for (int16_t x1 = 0; x1 < GxGDEM029T94_WIDTH / 8; x1++)
        {
          uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
          uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
          _writeData(~data);
        }
      }
    }
    _current_page = -1;
    if (cmd == 0x26) break;
  }
  _Update_Full();
  _PowerOff();
}

void GxGDEM029T94::drawPaged(void (*drawCallback)(const void*), const void* p)
{
  if (_current_page != -1) return;
  _using_partial_mode = false;
  _Init_Full(0x03);
  for (uint8_t cmd = 0x24; ; cmd = 0x26) // both buffers same for full b/w
  {
    _writeCommand(cmd);
    for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p);
      for (int16_t y1 = 0; y1 < GxGDEM029T94_PAGE_HEIGHT; y1++)
      {
        for (int16_t x1 = 0; x1 < GxGDEM029T94_WIDTH / 8; x1++)
        {
          uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
          uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
          _writeData(~data);
        }
      }
    }
    _current_page = -1;
    if (cmd == 0x26) break;
  }
  _Update_Full();
  _PowerOff();
}

void GxGDEM029T94::drawPaged(void (*drawCallback)(const void*, const void*), const void* p1, const void* p2)
{
  if (_current_page != -1) return;
  _using_partial_mode = false;
  _Init_Full(0x03);
  for (uint8_t cmd = 0x24; ; cmd = 0x26) // both buffers same for full b/w
  {
    _writeCommand(cmd);
    for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p1, p2);
      for (int16_t y1 = 0; y1 < GxGDEM029T94_PAGE_HEIGHT; y1++)
      {
        for (int16_t x1 = 0; x1 < GxGDEM029T94_WIDTH / 8; x1++)
        {
          uint16_t idx = y1 * (GxGDEM029T94_WIDTH / 8) + x1;
          uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
          _writeData(~data);
        }
      }
    }
    _current_page = -1;
    if (cmd == 0x26) break;
  }
  _Update_Full();
  _PowerOff();
}

void GxGDEM029T94::_rotate(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h)
{
  switch (getRotation())
  {
    case 1:
      swap(x, y);
      swap(w, h);
      x = GxGDEM029T94_WIDTH - x - w - 1;
      break;
    case 2:
      x = GxGDEM029T94_WIDTH - x - w - 1;
      y = GxGDEM029T94_HEIGHT - y - h - 1;
      break;
    case 3:
      swap(x, y);
      swap(w, h);
      y = GxGDEM029T94_HEIGHT - y - h - 1;
      break;
  }
}

void GxGDEM029T94::drawPagedToWindow(void (*drawCallback)(void), uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  if (_current_page != -1) return;
  _rotate(x, y, w, h);
  if (!_using_partial_mode)
  {
    eraseDisplay(false);
    eraseDisplay(true);
  }
  _using_partial_mode = true;
  _Init_Part(0x03);
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback();
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  _Update_Part();
  delay(GxGDEM029T94_PU_DELAY);
  // update previous buffer
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback();
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  delay(GxGDEM029T94_PU_DELAY);
  _current_page = -1;
  _PowerOff();
}

void GxGDEM029T94::drawPagedToWindow(void (*drawCallback)(uint32_t), uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t p)
{
  if (_current_page != -1) return;
  _rotate(x, y, w, h);
  if (!_using_partial_mode)
  {
    eraseDisplay(false);
    eraseDisplay(true);
  }
  _using_partial_mode = true;
  _Init_Part(0x03);
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p);
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  _Update_Part();
  delay(GxGDEM029T94_PU_DELAY);
  // update previous buffer
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p);
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  delay(GxGDEM029T94_PU_DELAY);
  _current_page = -1;
  _PowerOff();
}

void GxGDEM029T94::drawPagedToWindow(void (*drawCallback)(const void*), uint16_t x, uint16_t y, uint16_t w, uint16_t h, const void* p)
{
  if (_current_page != -1) return;
  _rotate(x, y, w, h);
  if (!_using_partial_mode)
  {
    eraseDisplay(false);
    eraseDisplay(true);
  }
  _using_partial_mode = true;
  _Init_Part(0x03);
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p);
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  _Update_Part();
  delay(GxGDEM029T94_PU_DELAY);
  // update previous buffer
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p);
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  delay(GxGDEM029T94_PU_DELAY);
  _current_page = -1;
  _PowerOff();
}

void GxGDEM029T94::drawPagedToWindow(void (*drawCallback)(const void*, const void*), uint16_t x, uint16_t y, uint16_t w, uint16_t h, const void* p1, const void* p2)
{
  if (_current_page != -1) return;
  _rotate(x, y, w, h);
  if (!_using_partial_mode)
  {
    eraseDisplay(false);
    eraseDisplay(true);
  }
  _using_partial_mode = true;
  _Init_Part(0x03);
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p1, p2);
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  _Update_Part();
  delay(GxGDEM029T94_PU_DELAY);
  // update previous buffer
  for (_current_page = 0; _current_page < GxGDEM029T94_PAGES; _current_page++)
  {
    uint16_t yds = gx_uint16_max(y, _current_page * GxGDEM029T94_PAGE_HEIGHT);
    uint16_t yde = gx_uint16_min(y + h, (_current_page + 1) * GxGDEM029T94_PAGE_HEIGHT);
    if (yde > yds)
    {
      fillScreen(GxEPD_WHITE);
      drawCallback(p1, p2);
      uint16_t ys = yds % GxGDEM029T94_PAGE_HEIGHT;
      _writeToWindow(x, ys, x, yds, w, yde - yds);
    }
  }
  delay(GxGDEM029T94_PU_DELAY);
  _current_page = -1;
  _PowerOff();
}

void GxGDEM029T94::drawCornerTest(uint8_t em)
{
  if (_current_page != -1) return;
  _Init_Full(em);
  _writeCommand(0x24);
  for (uint32_t y = 0; y < GxGDEM029T94_HEIGHT; y++)
  {
    for (uint32_t x = 0; x < GxGDEM029T94_WIDTH / 8; x++)
    {
      uint8_t data = 0xFF;
      if ((x < 1) && (y < 8)) data = 0x00;
      if ((x > GxGDEM029T94_WIDTH / 8 - 3) && (y < 16)) data = 0x00;
      if ((x > GxGDEM029T94_WIDTH / 8 - 4) && (y > GxGDEM029T94_HEIGHT - 25)) data = 0x00;
      if ((x < 4) && (y > GxGDEM029T94_HEIGHT - 33)) data = 0x00;
      _writeData(data);
    }
  }
  _Update_Full();
  _PowerOff();
}
