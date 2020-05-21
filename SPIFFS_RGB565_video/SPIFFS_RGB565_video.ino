#define VIDEO_WIDTH 240
#define VIDEO_HEIGHT 135

#include "SPI.h"
#include "Arduino_ESP32SPI.h"
#include "Arduino_ST7789.h" // Hardware-specific library for ST7789

// TTGO T-Display
#define TFT_BL 4
Arduino_DataBus *bus = new Arduino_ESP32SPI(16 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */, VSPI /* spi_num */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 23 /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);

#include "FS.h"
#include "SPIFFS.h"

void setup()
{
  Serial.begin(115200);

  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  File file = SPIFFS.open("/output.rgb");
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  uint8_t *buf = (uint8_t*)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * 2);

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
