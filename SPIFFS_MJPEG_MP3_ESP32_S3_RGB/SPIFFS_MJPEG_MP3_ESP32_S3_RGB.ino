/*
   require libraries:
   https://github.com/moononournation/Arduino_GFX.git
   https://github.com/earlephilhower/ESP8266Audio.git
   https://github.com/bitbank2/JPEGDEC.git
*/
#define MP3_FILENAME "/44100.mp3"
#define FPS 15
#define MJPEG_FILENAME "/480_15fps.mjpeg"
#define MJPEG_BUFFER_SIZE (480 * 270 * 2 / 4)
/*
   Connect the SD card to the following pins:

   SD Card | ESP32
      D2       12
      D3       13
      CMD      15
      VSS      GND
      VDD      3.3V
      CLK      14
      VSS      GND
      D0       2  (add 1K pull up after flashing)
      D1       4
*/

#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <FFat.h>
#include <SD.h>
#include <SD_MMC.h>

#include <Wire.h>
#include "es8311.h"

/* Arduino_GFX */
#include <Arduino_GFX_Library.h>

//#define TFT_BL 22
//Arduino_DataBus *bus = new Arduino_ESP32I2S8(27 /* DC */, 5 /* CS */, 25 /* WR */, 32 /* RD */, 23 /* D0 */, 19 /* D1 */, 18 /* D2 */, 26 /* D3 */, 21 /* D4 */, 4 /* D5 */, 0 /* D6 */, 2 /* D7 */);
//Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, 33 /* RST */, 1 /* rotation */, true /* IPS */);

#define TFT_BL 38
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  39 /* CS */, 48 /* SCK */, 47 /* SDA */,
  18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
  4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
  10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
  15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */
);
Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(bus, GFX_NOT_DEFINED, 480, 480);

/* variables */
static int next_frame = 0;
static int skipped_frames = 0;
static unsigned long total_play_audio_ms = 0;
static unsigned long total_read_video_ms = 0;
static unsigned long total_show_video_ms = 0;
static unsigned long start_ms, curr_ms, next_frame_ms;

/* MP3 audio */
#include "esp32_audio.h"

/* MJPEG Video */
#include "MjpegClass.h"
static MjpegClass mjpeg;

// pixel drawing callback
static int drawMCU(JPEGDRAW *pDraw)
{
  // Serial.printf("Draw pos = (%d, %d), size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  unsigned long s = millis();
  gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  total_show_video_ms += millis() - s;
  return 1;
} /* drawMCU() */

void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  // while(!Serial);

  // Init Display
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  gfx->println("Init Wire");
  Wire.begin(40 /* SDA */, 41 /* SCL */);

  gfx->println("Init ES8311");
  es8311_codec_config(AUDIO_HAL_44K_SAMPLES);
  es8311_codec_set_voice_volume(60);

  gfx->println("Init I2S");
  i2s_init(I2S_NUM_0, 44100,
           42 /* MCLK */, 46 /* SCLK */, 45 /* LRCK */, 43 /* DOUT */, 44 /* DIN */);
  i2s_zero_dma_buffer(I2S_NUM_0);

  gfx->println("Init FS");
  // if (!LittleFS.begin(false, "/root"))
  // if (!SPIFFS.begin(false, "/root"))
  if (!FFat.begin(false, "/root"))
    // if ((!SD_MMC.begin("/root")) && (!SD_MMC.begin("/root"))) /* 4-bit SD bus mode */
    // if ((!SD_MMC.begin("/root", true)) && (!SD_MMC.begin("/root", true))) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: File system mount failed!"));
    gfx->println(F("ERROR: File system mount failed!"));
  }
  else
  {
    gfx->println("Open video file: " MJPEG_FILENAME);
    // File vFile = LittleFS.open(MJPEG_FILENAME);
    // File vFile = SPIFFS.open(MJPEG_FILENAME);
    File vFile = FFat.open(MJPEG_FILENAME);
    // File vFile = SD_MMC.open(MJPEG_FILENAME);
    if (!vFile || vFile.isDirectory())
    {
      Serial.println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
      gfx->println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
    }
    else
    {
      uint8_t *mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
      if (!mjpeg_buf)
      {
        Serial.println(F("mjpeg_buf malloc failed!"));
      }
      else
      {
        Serial.println(F("MP3 audio MJPEG video start"));

        gfx->println("Init video");
        mjpeg.setup(
          &vFile, mjpeg_buf, drawMCU, true /* useBigEndian */,
          0 /* x */, 0 /* y */, gfx->width() /* widthLimit */, gfx->height() /* heightLimit */);

        start_ms = millis();
        curr_ms = millis();
        next_frame_ms = start_ms + (++next_frame * 1000 / FPS / 2);

        gfx->println("Start play audio file: " MP3_FILENAME);
        mp3_player_task_start((char *)MP3_FILENAME);

        gfx->println("Start play video");
        while (vFile.available() && mjpeg.readMjpegBuf()) // Read video
        {
          total_read_video_ms += millis() - curr_ms;

          if (millis() < next_frame_ms) // check show frame or skip frame
          {
            // Play video
            mjpeg.drawJpg();
          }
          else
          {
            ++skipped_frames;
            Serial.println(F("Skip frame"));
          }

          while (millis() < next_frame_ms)
          {
            vTaskDelay(1);
          }

          curr_ms = millis();
          next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
        }
        int time_used = millis() - start_ms;
        int total_frames = next_frame - 1;
        Serial.println(F("MP3 audio MJPEG video end"));
        vFile.close();
        int played_frames = total_frames - skipped_frames;
        float fps = 1000.0 * played_frames / time_used;
        Serial.printf("Played frames: %d\n", played_frames);
        Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / total_frames);
        Serial.printf("Time used: %d ms\n", time_used);
        Serial.printf("Expected FPS: %d\n", FPS);
        Serial.printf("Actual FPS: %0.1f\n", fps);
        Serial.printf("Play MP3: %lu ms (%0.1f %%)\n", total_play_audio_ms, 100.0 * total_play_audio_ms / time_used);
        Serial.printf("SDMMC read MJPEG: %lu ms (%0.1f %%)\n", total_read_video_ms, 100.0 * total_read_video_ms / time_used);
        // Serial.printf("Decode video: %lu ms (%0.1f %%)\n", total_decode_video_ms, 100.0 * total_decode_video_ms / time_used);
        Serial.printf("Show video: %lu ms (%0.1f %%)\n", total_show_video_ms, 100.0 * total_show_video_ms / time_used);

        // wait last frame finished
        delay(200);

#define CHART_MARGIN 24
#define LEGEND_A_COLOR 0xE0C3
#define LEGEND_B_COLOR 0x33F7
#define LEGEND_C_COLOR 0x4D69
#define LEGEND_D_COLOR 0x9A74
#define LEGEND_E_COLOR 0xFBE0
#define LEGEND_F_COLOR 0xFFE6
#define LEGEND_G_COLOR 0xA2A5
        //        gfx->setCursor(0, 0);
        gfx->setTextColor(WHITE);
        gfx->printf("Played frames: %d\n", played_frames);
        gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / total_frames);
        gfx->printf("Actual FPS: %0.1f\n\n", fps);
        int16_t r1 = ((gfx->height() - CHART_MARGIN - CHART_MARGIN) / 2);
        int16_t r2 = r1 / 2;
        int16_t cx = gfx->width() - gfx->height() + CHART_MARGIN + CHART_MARGIN - 1 + r1;
        int16_t cy = r1 + CHART_MARGIN;

        float arc_start1 = 0;
        float arc_end1 = arc_start1 + max(2.0, 360.0 * total_play_audio_ms / time_used);
        for (int i = arc_start1 + 1; i < arc_end1; i += 2)
        {
          gfx->fillArc(cx, cy, r1, r2, arc_start1 - 90.0, i - 90.0, LEGEND_A_COLOR);
        }
        gfx->fillArc(cx, cy, r1, r2, arc_start1 - 90.0, arc_end1 - 90.0, LEGEND_A_COLOR);
        gfx->setTextColor(LEGEND_A_COLOR);
        gfx->printf("Play MP3:\n%0.1f %%\n", 100.0 * total_play_audio_ms / time_used);

        float arc_start2 = arc_end1;
        float arc_end2 = arc_start2 + max(2.0, 360.0 * total_read_video_ms / time_used);
        for (int i = arc_start2 + 1; i < arc_end2; i += 2)
        {
          gfx->fillArc(cx, cy, r1, r2, arc_start2 - 90.0, i - 90.0, LEGEND_B_COLOR);
        }
        gfx->fillArc(cx, cy, r1, r2, arc_start2 - 90.0, arc_end2 - 90.0, LEGEND_B_COLOR);
        gfx->setTextColor(LEGEND_B_COLOR);
        gfx->printf("Read MJPEG:\n%0.1f %%\n", 100.0 * total_read_video_ms / time_used);

        float arc_start3 = arc_end2;
        float arc_end3 = arc_start3; // + max(2.0, 360.0 * total_decode_video_ms / time_used);
        for (int i = arc_start3 + 1; i < arc_end3; i += 2)
        {
          gfx->fillArc(cx, cy, r2, 0, arc_start3 - 90.0, i - 90.0, LEGEND_C_COLOR);
        }
        gfx->fillArc(cx, cy, r2, 0, arc_start3 - 90.0, arc_end3 - 90.0, LEGEND_C_COLOR);
        gfx->setTextColor(LEGEND_C_COLOR);
        // gfx->printf("Decode video:\n%0.1f %%\n", 100.0 * total_decode_video_ms / time_used);

        float arc_start4 = arc_end2;
        float arc_end4 = arc_start4 + max(2.0, 360.0 * total_show_video_ms / time_used);
        for (int i = arc_start4 + 1; i < arc_end4; i += 2)
        {
          gfx->fillArc(cx, cy, r1, r2, arc_start4 - 90.0, i - 90.0, LEGEND_D_COLOR);
        }
        gfx->fillArc(cx, cy, r1, r2, arc_start4 - 90.0, arc_end4 - 90.0, LEGEND_D_COLOR);
        gfx->setTextColor(LEGEND_D_COLOR);
        gfx->printf("Play video:\n%0.1f %%\n", 100.0 * total_show_video_ms / time_used);
      }
    }
  }
#ifdef TFT_BL
  delay(60000);
  digitalWrite(TFT_BL, LOW);
#endif
  //  gfx->displayOff();
  esp_deep_sleep_start();
}

void loop()
{
}
