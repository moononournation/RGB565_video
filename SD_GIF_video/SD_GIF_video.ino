#define VIDEO_WIDTH 128
#define VIDEO_HEIGHT 72
#define GIF_FILE "/128_15fps.gif"
// #define VIDEO_WIDTH 240
// #define VIDEO_HEIGHT 135
// #define GIF_FILE "/240_15fps.gif"
// #define VIDEO_WIDTH 320
// #define VIDEO_HEIGHT 180
// #define GIF_FILE "/320_15fps.gif"

#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <Arduino_ESP32SPI.h>
#include <Arduino_Display.h>
#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
#define SS 4
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9341_M5STACK *gfx = new Arduino_ILI9341_M5STACK(bus, 33 /* RST */, 1 /* rotation */);
#elif defined(ARDUINO_ODROID_ESP32)
#define TFT_BL 14
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
// Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);
#elif defined(ARDUINO_T) // TTGO T-Watch
#define TFT_BL 12
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240, 240, 0, 80);
#else /* not a specific hardware */
#define TFT_BL 22
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 33 /* RST */, 3 /* rotation */, true /* IPS */);
#endif /* not a specific hardware */

#include "gifdec.h"
uint8_t *buf;

// Setup method runs once, when the sketch starts
void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  Serial.println("Starting AnimatedGIFs Sketch");

  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  int32_t s = VIDEO_WIDTH * VIDEO_HEIGHT;
  buf = (uint8_t *)malloc(s);
  if (buf)
  {
    Serial.printf("main() malloc %d.\n", s);
  }
  else
  {
    Serial.printf("main() malloc %d failed!\n", s);
  }

  // Init SD card
  if (!SD.begin(4, SPI, 80000000))
  // if (!SD_MMC.begin())
  // if (!SD_MMC.begin("/sdcard", true))
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    int t_fstart = 0, t_delay = 0, t_real_delay, res, delay_until;
    File fp = SD.open(GIF_FILE);
    // File fp = SD_MMC.open(GIF_FILE);
    if (!fp)
    {
      Serial.println(F("ERROR: File open failed!"));
      gfx->println(F("ERROR: File open failed!"));
    }
    else
    {
      gd_GIF *gif = gd_open_gif(&fp);
      if (!gif)
      {
        Serial.println(F("ERROR: gd_open_gif() failed!"));
        gfx->println(F("ERROR: gd_open_gif() failed!"));
      }
      else
      {
        gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
        Serial.println("GIF start");
        while (1)
        {
          t_delay = gif->gce.delay * 10;
          res = gd_get_frame(gif, buf);
          if (res < 0)
          {
            Serial.println(F("ERROR: gd_get_frame() failed!"));
            break;
          }
          else if (res == 0)
          {
            Serial.println(F("gd_rewind()."));
            gd_rewind(gif);
            t_fstart = 0;
            continue;
          }

          gfx->startWrite();
          gfx->writeIndexedPixels(buf, gif->palette->colors, VIDEO_WIDTH * VIDEO_HEIGHT);
          gfx->endWrite();

          // t_real_delay = t_delay - (millis() - t_fstart);
          // delay_until = millis() + t_real_delay;
          // do
          // {
          //   delay(1);
          // } while (millis() < delay_until);

          t_fstart = millis();
        }
        Serial.println("GIF end");
        gd_close_gif(gif);
      }
    }
  }
}

void loop()
{
}
