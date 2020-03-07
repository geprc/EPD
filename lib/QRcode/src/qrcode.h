#include <Adafruit_GFX.h>
#include <GxEPD.h>
#include <GxGDEP015OC1/GxGDEP015OC1.h>
class QRcode {
   private:
    GxEPD_Class *display;
    void render(int x, int y, int color);

   public:
    QRcode(GxEPD_Class *display);
    void create(String message);
};
