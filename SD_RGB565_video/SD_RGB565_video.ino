//#define VIDEO_WIDTH 192
//#define VIDEO_HEIGHT 108
//#define FILENAME "/192_15fps.rgb"
#define VIDEO_WIDTH 240
#define VIDEO_HEIGHT 135
#define FILENAME "/240_10fps.rgb"

#include "SPI.h"
#include "Arduino_ESP32SPI.h"
#include "Arduino_Display.h"

#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
#define SS 4
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO /* MISO */);
Arduino_ILI9341_M5STACK *gfx = new Arduino_ILI9341_M5STACK(bus, 33 /* RST */, 1 /* rotation */);
#elif defined(ARDUINO_ODROID_ESP32)
#define TFT_BL 14
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */, SCK, MOSI, -1 /* MISO */);
// Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);
#elif defined(ARDUINO_T) // TTGO T-Watch
#define TFT_BL 12
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, SCK, MOSI, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240, 240, 0, 80);
#else /* not a specific hardware */
#define TFT_BL 2
#define SCK 21
#define MOSI 19
#define MISO 22
#define SS 0
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(15 /* DC */, 12 /* CS */, SCK, MOSI, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
#endif /* not a specific hardware */

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

  SPIClass spi = SPIClass(VSPI);
  spi.begin(SCK, MISO, MOSI, SS);

  if (!SD.begin(SS, spi, 80000000))
  {
    Serial.println("Card Mount Failed");
    return;
  }

  File file = SD.open(FILENAME);
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
    gfx->writePixels((uint16_t *)buf, l >> 1);
    gfx->endWrite();
  }
  Serial.println("End video");
#ifdef TFT_BL
  delay(1000);
  digitalWrite(TFT_BL, LOW);
#endif
}

void loop(void)
{
}
