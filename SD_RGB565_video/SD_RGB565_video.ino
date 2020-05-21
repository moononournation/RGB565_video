#define VIDEO_WIDTH 144
#define VIDEO_HEIGHT 81

#include "SPI.h"
#include "Arduino_ESP32SPI.h"
#include "Arduino_ST7789.h" // Hardware-specific library for ST7789

#define TFT_BL 14
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);

#include "FS.h"
#include "SD.h"

void setup()
{
  Serial.begin(115200);

  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  if (!SD.begin())
  {
    Serial.println("Card Mount Failed");
    return;
  }

  File file = SD.open("/output.rgb");
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  uint8_t *buf = (uint8_t *)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * 2);

  Serial.println("Start video");
  gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
  while (file.available())
  {
    int l = file.read(buf, VIDEO_WIDTH * VIDEO_HEIGHT * 2);
    gfx->startWrite();
    gfx->writePixels((uint16_t *)buf, VIDEO_WIDTH * VIDEO_HEIGHT);
    gfx->endWrite();
  }
  Serial.println("End video");
}

void loop(void)
{
}
