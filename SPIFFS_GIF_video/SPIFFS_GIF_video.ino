/*
 * Please upload SPIFFS data with ESP32 Sketch Data Upload:
 * https://github.com/me-no-dev/arduino-esp32fs-plugin
 */
/* GIF src: https://steamcommunity.com/sharedfiles/filedetails/?id=593882316 */
#define GIF_FILE "/teamMadokaRetro240.gif"

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Arduino_ESP32SPI_DMA.h>
#include <Arduino_Display.h>

// TTGO T-Display
#define TFT_BL 4
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(16 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */, VSPI);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 23 /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);

#include "gifdec.h"

void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);

  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  // Init SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println(F("ERROR: SPIFFS mount failed!"));
    gfx->println(F("ERROR: SPIFFS mount failed!"));
  }
  else
  {
    int t_fstart, t_delay, t_real_delay, res, delay_until;
    File vFile = SPIFFS.open(GIF_FILE);
    if (!vFile || vFile.isDirectory())
    {
      Serial.println(F("ERROR: File open failed!"));
      gfx->println(F("ERROR: File open failed!"));
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
          gfx->setAddrWindow((gfx->width() - gif->width) / 2, (gfx->height() - gif->height) / 2, gif->width, gif->height);
          Serial.println("GIF start");
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
              Serial.println(F("gd_rewind()."));
              gd_rewind(gif);
              continue;
            }

            gfx->startWrite();
            gfx->writeIndexedPixels(buf, gif->palette->colors, s);
            gfx->endWrite();

            t_real_delay = t_delay - (millis() - t_fstart);
            Serial.printf("t_delay: %d, t_real_delay: %d\n", t_delay, t_real_delay);
            delay_until = millis() + t_real_delay;
            do
            {
              delay(1);
            } while (millis() < delay_until);
          }
          Serial.println("GIF end");
          gd_close_gif(gif);
        }
      }
    }
  }

#ifdef TFT_BL
  delay(60000);
  digitalWrite(TFT_BL, LOW);
#endif
}

void loop(void)
{
}
