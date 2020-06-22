// #define VIDEO_WIDTH 192L
// #define VIDEO_HEIGHT 108L
// #define FPS 15
// #define VIDEO_FILENAME "/192_15fps.rgb"
#define VIDEO_WIDTH 240L
#define VIDEO_HEIGHT 135L
#define FPS 10
#define VIDEO_FILENAME "/240_10fps.rgb"

#include <SPI.h>
#include <Arduino_HWSPI.h>
#include <Arduino_Display.h>

#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
#define SS 4
Arduino_HWSPI *bus = new Arduino_HWSPI(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9341_M5STACK *gfx = new Arduino_ILI9341_M5STACK(bus, 33 /* RST */, 1 /* rotation */);
#elif defined(ARDUINO_ODROID_ESP32)
#define TFT_BL 14
Arduino_HWSPI *bus = new Arduino_HWSPI(21 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
// Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);
#elif defined(ARDUINO_T) // TTGO T-Watch
#define TFT_BL 12
Arduino_HWSPI *bus = new Arduino_HWSPI(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240, 240, 0, 80);
#else /* not a specific hardware */
#define TFT_BL 2
#define SCK 21
#define MOSI 19
#define MISO 22
#define SS 0
Arduino_HWSPI *bus = new Arduino_HWSPI(15 /* DC */, 12 /* CS */, SCK, MOSI, MISO);
// Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(15 /* DC */, 12 /* CS */, SCK, MOSI, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
#endif /* not a specific hardware */

#include <WiFi.h>
#include <FS.h>
#include <SD.h>

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

  // Init SD card
  if (!SD.begin(SS, SPI, 80000000))
  {
    Serial.println(F("ERROR: Card Mount Failed!"));
    gfx->println(F("ERROR: Card Mount Failed!"));
  }
  else
  {
    File vFile = SD.open(VIDEO_FILENAME);
    if (!vFile || vFile.isDirectory())
    {
      Serial.println(F("ERROR: Failed to open video file for reading"));
      gfx->println(F("ERROR: Failed to open video file for reading"));
    }
    else
    {
      uint8_t *buf = (uint8_t *)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * 2);
      if (!buf)
      {
        Serial.println(F("buf malloc failed!"));
      }

      Serial.println("Start video");
      gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
      int next_frame = 0;
      int skipped_frames = 0;
      unsigned long total_sd_rgb = 0;
      unsigned long total_push_video = 0;
      unsigned long total_remain = 0;
      unsigned long start_ms = millis();
      unsigned long curr_ms = millis();
      unsigned long next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
      while (vFile.available())
      {
        // Load video to buf
        uint32_t l = vFile.read(buf, VIDEO_WIDTH * VIDEO_HEIGHT * 2);
        total_sd_rgb += millis() - curr_ms;
        curr_ms = millis();

        if (millis() < next_frame_ms) // check show frame or skip frame
        {
          gfx->startWrite();
          gfx->writePixels((uint16_t *)buf, l >> 1);
          gfx->endWrite();
          total_push_video += millis() - curr_ms;
          int remain_ms = next_frame_ms - millis();
          if (remain_ms > 0)
          {
            total_remain += remain_ms;
            delay(remain_ms);
          }
        }
        else
        {
          ++skipped_frames;
          Serial.println(F("Skip frame"));
        }

        curr_ms = millis();
        next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
      }
      int time_used = millis() - start_ms;
      Serial.println(F("End video"));
      int played_frames = next_frame - 1 - skipped_frames;
      float fps = 1000.0 * played_frames / time_used;
      Serial.printf("Played frame: %d\n", played_frames);
      Serial.printf("Skipped frames: %d (%f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
      Serial.printf("Time used: %d ms\n", time_used);
      Serial.printf("Expected FPS: %d\n", FPS);
      Serial.printf("Actual FPS: %f\n", fps);
      Serial.printf("SD RGB: %d ms (%f %%)\n", total_sd_rgb, 100.0 * total_sd_rgb / time_used);
      Serial.printf("Push video: %d ms (%f %%)\n", total_push_video, 100.0 * total_push_video / time_used);
      Serial.printf("Remain: %d ms (%f %%)\n", total_remain, 100.0 * total_remain / time_used);

      gfx->setCursor(0, 0);
      gfx->setTextColor(WHITE, BLACK);
      gfx->printf("Played frame: %d\n", played_frames);
      gfx->printf("Skipped frames: %d (%f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
      gfx->printf("Time used: %d ms\n", time_used);
      gfx->printf("Expected FPS: %d\n", FPS);
      gfx->printf("Actual FPS: %f\n", fps);
      gfx->printf("SD RGB: %d ms (%f %%)\n", total_sd_rgb, 100.0 * total_sd_rgb / time_used);
      gfx->printf("Push video: %d ms (%f %%)\n", total_push_video, 100.0 * total_push_video / time_used);
      gfx->printf("Remain: %d ms (%f %%)\n", total_remain, 100.0 * total_remain / time_used);
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
