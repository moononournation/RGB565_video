#define AUDIO_FILENAME "/44100_u16le.pcm"
#define FPS 15
#define VIDEO_FILENAME "/220_15fps.gif"
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
#include <driver/i2s.h>

#include <Arduino_GFX_Library.h>
#define TFT_BRIGHTNESS 128
// ST7789 Display
// #define TFT_BL 22
// Arduino_DataBus *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 33 /* RST */, 3 /* rotation */, true /* IPS */);
// ILI9225 Display
#define TFT_BL 22
Arduino_DataBus *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 3 /* rotation */);

#include "gifdec.h"
gd_GIF *gif;
uint8_t *aBuf;
uint8_t *vBuf1;
uint8_t *vBuf2;
uint8_t loaded_buffer_idx = 2;

int next_frame = 0;
int played_frames = 0;
unsigned long total_read_audio = 0;
unsigned long total_play_audio = 0;
unsigned long total_read_video = 0;
unsigned long total_play_video = 0;
unsigned long total_remain = 0;
unsigned long start_ms, curr_ms, next_frame_ms;

static void videoTask(void *arg)
{
  uint8_t showed_buffer_idx = 2;

  while (1)
  {
    if (showed_buffer_idx != loaded_buffer_idx)
    {
      showed_buffer_idx = loaded_buffer_idx;
      uint8_t *buf = (loaded_buffer_idx == 1) ? vBuf1 : vBuf2;

      if (buf)
      {
        unsigned long ms = millis();
        gfx->startWrite();
        gfx->writeIndexedPixels(buf, gif->palette->colors, gif->width * gif->height);
        gfx->endWrite();
        total_play_video += millis() - ms;
        played_frames++;
      }
    }
    else
    {
      delay(1);
    }
  }
}

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
  }
  else
  {
    // Init Audio
    i2s_config_t i2s_config_dac = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
        .dma_buf_count = 7,
        .dma_buf_len = 588,
        .use_apll = false,
    };
    Serial.printf("%p\n", &i2s_config_dac);
    if (i2s_driver_install(I2S_NUM_0, &i2s_config_dac, 0, NULL) != ESP_OK)
    {
      Serial.println(F("ERROR: Unable to install I2S drives!"));
      gfx->println(F("ERROR: Unable to install I2S drives!"));
    }
    else
    {
      i2s_set_pin((i2s_port_t)0, NULL);
      i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
      i2s_zero_dma_buffer((i2s_port_t)0);

      File aFile = SD_MMC.open(AUDIO_FILENAME);
      if (!aFile || aFile.isDirectory())
      {
        Serial.println(F("ERROR: Failed to open " AUDIO_FILENAME " file for reading!"));
        gfx->println(F("ERROR: Failed to open " AUDIO_FILENAME " file for reading!"));
      }
      else
      {
        File vFile = SD_MMC.open(VIDEO_FILENAME);
        if (!vFile || vFile.isDirectory())
        {
          Serial.println(F("ERROR: Failed to open " VIDEO_FILENAME " file for reading"));
          gfx->println(F("ERROR: Failed to open " VIDEO_FILENAME " file for reading"));
        }
        else
        {
          aBuf = (uint8_t *)malloc(5880);
          if (!aBuf)
          {
            Serial.println(F("aBuf malloc failed!"));
          }
          else
          {
            gif = gd_open_gif(&vFile);
            if (!gif)
            {
              Serial.println(F("ERROR: gd_open_gif() failed!"));
              gfx->println(F("ERROR: gd_open_gif() failed!"));
            }
            else
            {
              int32_t s = gif->width * gif->height;
              vBuf1 = (uint8_t *)malloc(s);
              if (!vBuf1)
              {
                Serial.println(F("vBuf1 malloc failed!"));
              }
              else
              {
                vBuf2 = (uint8_t *)malloc(s);
                if (!vBuf2)
                {
                  Serial.println(F("vBuf2 malloc failed!"));
                }
                else
                {

                  xTaskCreatePinnedToCore(&videoTask, "videoTask", 2048, NULL, 1, NULL, 0);

                  Serial.println(F("PCM audio GIF video start"));
                  int res;
                  start_ms = millis();
                  curr_ms = millis();
                  next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
                  gfx->setAddrWindow((gfx->width() - gif->width) / 2, (gfx->height() - gif->height) / 2, gif->width, gif->height);

                  // prefetch first audio buffer
                  aFile.read(aBuf, 1176);
                  i2s_write_bytes((i2s_port_t)0, (char *)aBuf, 1176, 0);

                  while (vFile.available() && aFile.available())
                  {
                    // Read audio
                    aFile.read(aBuf, 5880);
                    total_read_audio += millis() - curr_ms;
                    curr_ms = millis();

                    // Play audio
                    i2s_write_bytes((i2s_port_t)0, (char *)aBuf, 1176, 0);
                    i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 1176), 1176, 0);
                    i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 2352), 1176, 0);
                    i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 3528), 1176, 0);
                    i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 4704), 1176, 0);
                    total_play_audio += millis() - curr_ms;
                    curr_ms = millis();

                    // Read GIF frame
                    if (loaded_buffer_idx == 1)
                    {
                      memcpy(vBuf2, vBuf1, s);
                      res = gd_get_frame(gif, vBuf2);
                      loaded_buffer_idx = 2;
                    }
                    else
                    {
                      memcpy(vBuf1, vBuf2, s);
                      res = gd_get_frame(gif, vBuf1);
                      loaded_buffer_idx = 1;
                    }

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
                    total_read_video += millis() - curr_ms;
                    int remain_ms = next_frame_ms - millis();
                    total_remain += remain_ms;
                    if (remain_ms > 0)
                    {
                      delay(remain_ms);
                    }

                    curr_ms = millis();
                    next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
                  }
                  int time_used = millis() - start_ms;
                  Serial.println(F("End audio video"));
                  vFile.close();
                  aFile.close();
                  int skipped_frames = next_frame - 1 - played_frames;
                  float fps = 1000.0 * played_frames / time_used;
                  Serial.printf("Played frames: %d\n", played_frames);
                  Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
                  Serial.printf("Time used: %d ms\n", time_used);
                  Serial.printf("Expected FPS: %d\n", FPS);
                  Serial.printf("Actual FPS: %0.1f\n", fps);
                  Serial.printf("SDMMC read PCM: %d ms (%0.1f %%)\n", total_read_audio, 100.0 * total_read_audio / time_used);
                  Serial.printf("Play audio: %d ms (%0.1f %%)\n", total_play_audio, 100.0 * total_play_audio / time_used);
                  Serial.printf("SDMMC read GIF: %d ms (%0.1f %%)\n", total_read_video, 100.0 * total_read_video / time_used);
                  Serial.printf("Play video: %d ms (%0.1f %%)\n", total_play_video, 100.0 * total_play_video / time_used);
                  Serial.printf("Remain: %d ms (%0.1f %%)\n", total_remain, 100.0 * total_remain / time_used);

#define CHART_MARGIN 24
#define LEGEND_A_COLOR 0xE0C3
#define LEGEND_B_COLOR 0x33F7
#define LEGEND_C_COLOR 0x4D69
#define LEGEND_D_COLOR 0x9A74
#define LEGEND_E_COLOR 0xFBE0
#define LEGEND_F_COLOR 0xFFE6
#define LEGEND_G_COLOR 0xA2A5
                  gfx->setCursor(0, 0);
                  gfx->setTextColor(WHITE);
                  gfx->printf("Played frames: %d\n", played_frames);
                  gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
                  gfx->printf("Actual FPS: %0.1f\n\n", fps);
                  int16_t r1 = ((gfx->height() - CHART_MARGIN - CHART_MARGIN) / 2);
                  int16_t r2 = r1 / 2;
                  int16_t cx = gfx->width() - gfx->height() + CHART_MARGIN + CHART_MARGIN - 1 + r1;
                  int16_t cy = r1 + CHART_MARGIN;
                  float arc_start = 0;
                  float arc_end = max(2.0, 360.0 * total_read_audio / time_used);
                  for (int i = arc_start + 1; i < arc_end; i += 2)
                  {
                    gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_D_COLOR);
                  }
                  gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_D_COLOR);
                  gfx->setTextColor(LEGEND_D_COLOR);
                  gfx->printf("Read PCM:\n%0.1f %%\n", 100.0 * total_read_audio / time_used);

                  arc_start = arc_end;
                  arc_end += max(2.0, 360.0 * total_play_audio / time_used);
                  for (int i = arc_start + 1; i < arc_end; i += 2)
                  {
                    gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_C_COLOR);
                  }
                  gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_C_COLOR);
                  gfx->setTextColor(LEGEND_C_COLOR);
                  gfx->printf("Play audio:\n%0.1f %%\n", 100.0 * total_play_audio / time_used);

                  arc_start = arc_end;
                  arc_end += max(2.0, 360.0 * total_read_video / time_used);
                  for (int i = arc_start + 1; i < arc_end; i += 2)
                  {
                    gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_B_COLOR);
                  }
                  gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_B_COLOR);
                  gfx->setTextColor(LEGEND_B_COLOR);
                  gfx->printf("Read GIF:\n%0.1f %%\n", 100.0 * total_read_video / time_used);

                  arc_start = arc_end;
                  arc_end += max(2.0, 360.0 * total_play_video / time_used);
                  for (int i = arc_start + 1; i < arc_end; i += 2)
                  {
                    gfx->fillArc(cx, cy, r2 - 1, 0, arc_start - 90.0, i - 90.0, LEGEND_A_COLOR);
                  }
                  gfx->fillArc(cx, cy, r2 - 1, 0, arc_start - 90.0, arc_end - 90.0, LEGEND_A_COLOR);
                  gfx->setTextColor(LEGEND_A_COLOR);
                  gfx->printf("Play video:\n%0.1f %%\n", 100.0 * total_play_video / time_used);

                  i2s_driver_uninstall((i2s_port_t)0); // stop & destroy i2s driver
                  // avoid unexpected output at audio pins
                  pinMode(25, OUTPUT);
                  digitalWrite(25, LOW);
                  pinMode(26, OUTPUT);
                  digitalWrite(26, LOW);
                }
              }
            }
          }
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
