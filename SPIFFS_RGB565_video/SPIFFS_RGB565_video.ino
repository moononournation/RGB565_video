#define VIDEO_WIDTH 240L
#define VIDEO_HEIGHT 135L
#define FPS 10
#define VIDEO_FILENAME "/output.rgb"

#include <SPI.h>
#include <Arduino_ESP32SPI.h>
#include <Arduino_Display.h>

// TTGO T-Display
#define TFT_BL 4
Arduino_DataBus *bus = new Arduino_ESP32SPI(16 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */, VSPI /* spi_num */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 23 /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

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
    Serial.println(F("ERROR: SPIFFS Mount Failed!"));
    gfx->println(F("ERROR: SPIFFS Mount Failed!"));
  }
  else
  {
    File vFile = SPIFFS.open(VIDEO_FILENAME);
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
