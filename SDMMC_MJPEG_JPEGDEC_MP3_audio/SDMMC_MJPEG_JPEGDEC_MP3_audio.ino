/*
 * require libraries:
 * https://github.com/moononournation/Arduino_GFX.git
 * https://github.com/earlephilhower/ESP8266Audio.git
 * https://github.com/bitbank2/JPEGDEC.git
 */
#define MP3_FILENAME "/22050.mp3"
#define FPS 30
#define MJPEG_FILENAME "/208_30fps.mjpeg"
#define MJPEG_BUFFER_SIZE (208 * 176 * 2 / 4)
/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       12
 *    D3       13
 *    CMD      15
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      14
 *    VSS      GND
 *    D0       2  (add 1K pull up after flashing)
 *    D1       4
 */

#include <WiFi.h>
#include <SD_MMC.h>

/* Arduino_GFX */
#include <Arduino_GFX_Library.h>
#define TFT_BRIGHTNESS 128
#define TFT_BL 22
Arduino_DataBus *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, -1 /* MISO */);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 3 /* rotation */);

/* MP3 Audio */
#include <AudioFileSourceFS.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
static AudioGeneratorMP3 *mp3;
static AudioOutputI2S *out;

/* MJPEG Video */
#include "MjpegClass.h"
static MjpegClass mjpeg;
uint8_t *mjpeg_buf;

/* variables */
static unsigned long total_play_audio, total_read_video, total_decode_video, total_show_video;
static unsigned long start_ms, curr_ms, next_frame_ms;
static int skipped_frames, next_frame, time_used, total_frames;

// pixel drawing callback
static int drawMCU(JPEGDRAW *pDraw)
{
  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  unsigned long s = millis();
  gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  total_show_video += millis() - s;
  return 1;
} /* drawMCU() */

void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);

  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  ledcSetup(1, 12000, 8);       // 12 kHz PWM, 8-bit resolution
  ledcAttachPin(TFT_BL, 1);     // assign TFT_BL pin to channel 1
  ledcWrite(1, TFT_BRIGHTNESS); // brightness 0 - 255
#endif

  // Init SD card
  // if ((!SD_MMC.begin()) && (!SD_MMC.begin())) /* 4-bit SD bus mode */
  if ((!SD_MMC.begin("/sdcard", true)) && (!SD_MMC.begin("/sdcard", true))) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
    exit(1);
  }

  out = new AudioOutputI2S(0, 1, 64); // Output to builtInDAC
  // out->SetGain(0.5);
  mp3 = new AudioGeneratorMP3();

  mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
  if (!mjpeg_buf)
  {
    Serial.println(F("mjpeg_buf malloc failed!"));
    gfx->println(F("mjpeg_buf malloc failed!"));
    exit(1);
  }
}

void loop()
{
  AudioFileSourceFS *aFile = new AudioFileSourceFS(SD_MMC, MP3_FILENAME);

  File vFile = SD_MMC.open(MJPEG_FILENAME);
  if (!vFile || vFile.isDirectory())
  {
    Serial.println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
    gfx->println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
    exit(1);
  }

  Serial.println(F("PCM audio MJPEG video start"));

  // init Video
  mjpeg.setup(&vFile, mjpeg_buf, drawMCU, true, true);

  // init audio
  mp3->begin(aFile, out);

  skipped_frames = 0;
  total_play_audio = 0;
  total_read_video = 0;
  total_decode_video = 0;
  total_show_video = 0;
  next_frame = 0;
  start_ms = millis();
  curr_ms = start_ms;
  next_frame_ms = start_ms + (++next_frame * 1000 / FPS);

  while (vFile.available() && mjpeg.readMjpegBuf()) // Read video
  {
    total_read_video += millis() - curr_ms;
    curr_ms = millis();

    if (millis() < next_frame_ms) // check show frame or skip frame
    {
      // Play video
      mjpeg.drawJpg();
      total_decode_video += millis() - curr_ms;
    }
    else
    {
      ++skipped_frames;
      Serial.println(F("Skip frame"));
    }
    curr_ms = millis();

    // Play audio
    if ((mp3->isRunning()) && (!mp3->loop()))
    {
      mp3->stop();
    }
    total_play_audio += millis() - curr_ms;

    while (millis() < next_frame_ms)
    {
      vTaskDelay(1);
    }

    curr_ms = millis();
    next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
  }
  time_used = millis() - start_ms;
  Serial.println(F("PCM audio MJPEG video end"));
  vFile.close();
  aFile->close();

  display_stat();

  delay(10000);
}

#define CHART_MARGIN 24
#define LEGEND_A_COLOR 0xE0C3
#define LEGEND_B_COLOR 0x33F7
#define LEGEND_C_COLOR 0x4D69
#define LEGEND_D_COLOR 0x9A74
#define LEGEND_E_COLOR 0xFBE0
#define LEGEND_F_COLOR 0xFFE6
#define LEGEND_G_COLOR 0xA2A5

void display_stat()
{
  int total_frames = next_frame - 1;
  int played_frames = total_frames - skipped_frames;
  float fps = 1000.0 * played_frames / time_used;
  Serial.printf("Played frames: %d\n", played_frames);
  Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / total_frames);
  Serial.printf("Time used: %d ms\n", time_used);
  Serial.printf("Expected FPS: %d\n", FPS);
  Serial.printf("Actual FPS: %0.1f\n", fps);
  Serial.printf("Play MP3: %lu ms (%0.1f %%)\n", total_play_audio, 100.0 * total_play_audio / time_used);
  Serial.printf("SDMMC read MJPEG: %lu ms (%0.1f %%)\n", total_read_video, 100.0 * total_read_video / time_used);
  Serial.printf("Decode video: %lu ms (%0.1f %%)\n", total_decode_video, 100.0 * total_decode_video / time_used);
  Serial.printf("Show video: %lu ms (%0.1f %%)\n", total_show_video, 100.0 * total_show_video / time_used);

  gfx->setCursor(0, 0);
  gfx->setTextColor(WHITE);
  gfx->printf("Played frames: %d\n", played_frames);
  gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / total_frames);
  gfx->printf("Actual FPS: %0.1f\n\n", fps);
  int16_t r1 = ((gfx->height() - CHART_MARGIN - CHART_MARGIN) / 2);
  int16_t r2 = r1 / 2;
  int16_t cx = gfx->width() - gfx->height() + CHART_MARGIN + CHART_MARGIN - 1 + r1;
  int16_t cy = r1 + CHART_MARGIN;
  float arc_start = 0;
  float arc_end = max(2.0, 360.0 * total_play_audio / time_used);
  for (int i = arc_start + 1; i < arc_end; i += 2)
  {
    gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_D_COLOR);
  }
  gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_D_COLOR);
  gfx->setTextColor(LEGEND_D_COLOR);
  gfx->printf("Play MP3:\n%0.1f %%\n", 100.0 * total_play_audio / time_used);

  arc_start = arc_end;
  arc_end += max(2.0, 360.0 * total_read_video / time_used);
  for (int i = arc_start + 1; i < arc_end; i += 2)
  {
    gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_C_COLOR);
  }
  gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_C_COLOR);
  gfx->setTextColor(LEGEND_C_COLOR);
  gfx->printf("Read MJPEG:\n%0.1f %%\n", 100.0 * total_read_video / time_used);

  arc_start = arc_end;
  arc_end += max(2.0, 360.0 * total_decode_video / time_used);
  for (int i = arc_start + 1; i < arc_end; i += 2)
  {
    gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_B_COLOR);
  }
  gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_B_COLOR);
  gfx->setTextColor(LEGEND_B_COLOR);
  gfx->printf("Decode video:\n%0.1f %%\n", 100.0 * total_decode_video / time_used);

  arc_start = arc_end;
  arc_end += max(2.0, 360.0 * total_show_video / time_used);
  for (int i = arc_start + 1; i < arc_end; i += 2)
  {
    gfx->fillArc(cx, cy, r2, 0, arc_start - 90.0, i - 90.0, LEGEND_A_COLOR);
  }
  gfx->fillArc(cx, cy, r2, 0, arc_start - 90.0, arc_end - 90.0, LEGEND_A_COLOR);
  gfx->setTextColor(LEGEND_A_COLOR);
  gfx->printf("Play video:\n%0.1f %%\n", 100.0 * total_show_video / time_used);
}