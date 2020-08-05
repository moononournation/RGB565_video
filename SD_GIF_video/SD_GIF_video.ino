#define GIF_FILENAME "/220_12fps.gif"
#include <WiFi.h>
#include <SD.h>
#include <SD_MMC.h>

#include <Arduino_ESP32SPI_DMA.h>
#include <Arduino_Display.h>
#define TFT_BRIGHTNESS 128
#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
#define SS 4
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9341_M5STACK *gfx = new Arduino_ILI9341_M5STACK(bus, 33 /* RST */, 1 /* rotation */);
#elif defined(ARDUINO_ODROID_ESP32)
#define TFT_BL 14
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(21 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
// Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);
#elif defined(ARDUINO_T) // TTGO T-Watch
#define TFT_BL 12
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240, 240, 0, 80);
#else /* not a specific hardware */
#define SCK 18
#define MOSI 23
#define MISO 19
#define SS 0
#define TFT_BL 22
// ST7789 Display
// Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(15 /* DC */, 12 /* CS */, SCK, MOSI, MISO);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
// ILI9225 Display
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);
#endif /* not a specific hardware */

#include "gifdec.h"

void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);

  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
    ledcAttachPin(TFT_BL, 1); // assign TFT_BL pin to channel 1
    ledcSetup(1, 12000, 8);   // 12 kHz PWM, 8-bit resolution
    ledcWrite(1, TFT_BRIGHTNESS);  // brightness 0 - 255
#endif

  // Init SD card
  if (!SD.begin(SS, SPI, 80000000)) /* SPI bus mode */
  // if (!SD_MMC.begin()) /* 4-bit SD bus mode */
  // if (!SD_MMC.begin("/sdcard", true)) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    File vFile = SD.open(GIF_FILENAME);
    // File vFile = SD_MMC.open(GIF_FILENAME);
    if (!vFile || vFile.isDirectory())
    {
      Serial.println(F("ERROR: Failed to open "GIF_FILENAME" file for reading"));
      gfx->println(F("ERROR: Failed to open "GIF_FILENAME" file for reading"));
    }
    else
    {
      gd_GIF *gif = gd_open_gif(&vFile);
      if (!gif)
      {
              Serial.println(F("ERROR: gd_open_gif() failed!"));
              gfx->println(F("ERROR: gd_open_gif() failed!"));
      }
      else
      {
        int32_t s = gif->width * gif->height;
        uint8_t *buf = (uint8_t *)malloc(s);
        if (!buf)
        {
          Serial.println(F("buf malloc failed!"));
        }
        else
        {
          Serial.println(F("GIF video start"));
          gfx->setAddrWindow((gfx->width() - gif->width) / 2, (gfx->height() - gif->height) / 2, gif->width, gif->height);
          int t_fstart, t_delay = 0, t_real_delay, res, delay_until;
          while (1)
          {
            t_fstart = millis();
            t_delay = gif->gce.delay * 10;
            res = gd_get_frame(gif, buf);
            if (res < 0)
            {
              Serial.println(F("ERROR: gd_get_frame() failed!"));
              break;
            }
            else if (res == 0)
            {
              // Serial.println(F("gd_rewind()."));
              // gd_rewind(gif);
              // continue;
              break; // exit while loop instead of loop back
            }

            gfx->startWrite();
            gfx->writeIndexedPixels(buf, gif->palette->colors, s);
            gfx->endWrite();

            t_real_delay = t_delay - (millis() - t_fstart);
            delay_until = millis() + t_real_delay;
            // do
            // {
            //   delay(1);
            // } while (millis() < delay_until);
          }
          Serial.println(F("GIF video end"));
          gd_close_gif(gif);
        }
      }
    }
  }
#ifdef TFT_BL
  delay(60000);
  ledcDetachPin(TFT_BL);
#endif
  gfx->displayOff();
  esp_deep_sleep_start();
}

void loop()
{
}
