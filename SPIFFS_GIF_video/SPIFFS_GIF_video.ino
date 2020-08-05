/*
 * Please upload SPIFFS data with ESP32 Sketch Data Upload:
 * https://github.com/me-no-dev/arduino-esp32fs-plugin
 */
/* GIF src: https://steamcommunity.com/sharedfiles/filedetails/?id=593882316 */
// #define GIF_FILENAME "/teamMadokaRetro240.gif"
#define GIF_FILENAME "/teamMadokaRetro220.gif"

#include <WiFi.h>
#include <SPIFFS.h>

#include <Arduino_ESP32SPI_DMA.h>
#include <Arduino_Display.h>
#define TFT_BRIGHTNESS 128
#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
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
// ST7789 Display
// #define TFT_BL 22
// Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(15 /* DC */, 12 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
// ILI9225 Display
#define TFT_BL 22
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);
// TTGO T-Display
// #define TFT_BL 4
// Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(16 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 23 /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);
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

  // Init SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println(F("ERROR: SPIFFS mount failed!"));
    gfx->println(F("ERROR: SPIFFS mount failed!"));
  }
  else
  {
    File vFile = SPIFFS.open(GIF_FILENAME);
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
        Serial.println(F("gd_open_gif() failed!"));
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
          int duration = 0, remain = 0;
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
              Serial.printf("rewind, duration: %d, remain: %d (%0.1f %%)\n", duration, remain, 100.0 * remain / duration);
              duration = 0;
              remain = 0;
              gd_rewind(gif);
              continue;
            }

            gfx->startWrite();
            gfx->writeIndexedPixels(buf, gif->palette->colors, s);
            gfx->endWrite();

            t_real_delay = t_delay - (millis() - t_fstart);
            duration += t_delay;
            remain += t_real_delay;
            delay_until = millis() + t_real_delay;
            do
            {
              delay(1);
            } while (millis() < delay_until);
          }
          Serial.println(F("GIF video end"));
          Serial.printf("duration: %d, remain: %d (%0.1f %%)\n", duration, remain, 100.0 * remain / duration);
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
