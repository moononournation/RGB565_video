#define AUDIO_FILENAME "/48000_u16le.pcm"
#define VIDEO_WIDTH 224L
#define VIDEO_HEIGHT 126L
#define FPS 12
#define VIDEO_FILENAME "/224_12fps.rgb"
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
#include <FS.h>
#include <SD_MMC.h>
#include <driver/i2s.h>
#include <Arduino_ESP32SPI.h>
#include <Arduino_Display.h>
#define TFT_BL 22
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */, VSPI);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 33 /* RST */, 3 /* rotation */, true /* IPS */);

uint8_t *aBuf;
uint8_t *vBuf1;
uint8_t *vBuf2;
uint8_t loaded_buffer_idx = 2;

int next_frame = 0;
int played_frames = 0;
unsigned long total_sd_pcm = 0;
unsigned long total_push_audio = 0;
unsigned long total_sd_rgb = 0;
unsigned long total_push_video = 0;
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
        gfx->writePixels((uint16_t *)buf, VIDEO_WIDTH * VIDEO_HEIGHT);
        gfx->endWrite();
        total_push_video += millis() - ms;
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
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  // Init SD card
  // if (!SD_MMC.begin()) /* 4-bit SD bus mode */
  if (!SD_MMC.begin("/sdcard", true)) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: Card Mount Failed!"));
    gfx->println(F("ERROR: Card Mount Failed!"));
  }
  else
  {
    // Init Audio
    i2s_config_t i2s_config_dac = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = 48000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
        .dma_buf_count = 6,
        .dma_buf_len = 800,
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
        Serial.println(F("ERROR: Failed to open audio file for reading!"));
        gfx->println(F("ERROR: Failed to open audio file for reading!"));
      }
      else
      {
        File vFile = SD_MMC.open(VIDEO_FILENAME);
        if (!vFile || vFile.isDirectory())
        {
          Serial.println(F("ERROR: Failed to open video file for reading"));
          gfx->println(F("ERROR: Failed to open video file for reading"));
        }
        else
        {
          aBuf = (uint8_t *)malloc(8000);
          if (!aBuf)
          {
            Serial.println(F("aBuf malloc failed!"));
          }
          vBuf1 = (uint8_t *)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * 2);
          if (!vBuf1)
          {
            Serial.println(F("vBuf1 malloc failed!"));
          }
          vBuf2 = (uint8_t *)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * 2);
          if (!vBuf2)
          {
            Serial.println(F("vBuf2 malloc failed!"));
          }

          xTaskCreatePinnedToCore(&videoTask, "videoTask", 2048, NULL, 1, NULL, 0);

          Serial.println(F("Start audio video"));
          start_ms = millis();
          curr_ms = millis();
          next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
          gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
          while (vFile.available() && aFile.available())
          {
            // Dump audio
            aFile.read(aBuf, 8000);
            total_sd_pcm += millis() - curr_ms;
            curr_ms = millis();

            i2s_write_bytes((i2s_port_t)0, (char *)aBuf, 1600, 0);
            i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 1600), 1600, 0);
            i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 3200), 1600, 0);
            i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 4800), 1600, 0);
            i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 6400), 1600, 0);
            total_push_audio += millis() - curr_ms;
            curr_ms = millis();

            // Load video to buf
            uint8_t *buf = (loaded_buffer_idx == 1) ? vBuf2 : vBuf1;
            uint32_t l = vFile.read(buf, VIDEO_WIDTH * VIDEO_HEIGHT * 2);
            loaded_buffer_idx = (loaded_buffer_idx == 1) ? 2 : 1;
            total_sd_rgb += millis() - curr_ms;
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
          int skipped_frames = next_frame - 1 - played_frames;
          float fps = 1000.0 * played_frames / time_used;
          Serial.printf("Played frame: %d\n", played_frames);
          Serial.printf("Skipped frames: %d (%f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
          Serial.printf("Time used: %d ms\n", time_used);
          Serial.printf("Expected FPS: %d\n", FPS);
          Serial.printf("Actual FPS: %f\n", fps);
          Serial.printf("SD PCM: %d ms (%f %%)\n", total_sd_pcm, 100.0 * total_sd_pcm / time_used);
          Serial.printf("Push audio: %d ms (%f %%)\n", total_push_audio, 100.0 * total_push_audio / time_used);
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
          gfx->printf("SD PCM: %d ms (%f %%)\n", total_sd_pcm, 100.0 * total_sd_pcm / time_used);
          gfx->printf("Push audio: %d ms (%f %%)\n", total_push_audio, 100.0 * total_push_audio / time_used);
          gfx->printf("SD RGB: %d ms (%f %%)\n", total_sd_rgb, 100.0 * total_sd_rgb / time_used);
          gfx->printf("Push video: %d ms (%f %%)\n", total_push_video, 100.0 * total_push_video / time_used);
          gfx->printf("Remain: %d ms (%f %%)\n", total_remain, 100.0 * total_remain / time_used);

          i2s_driver_uninstall((i2s_port_t)0); //stop & destroy i2s driver
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
