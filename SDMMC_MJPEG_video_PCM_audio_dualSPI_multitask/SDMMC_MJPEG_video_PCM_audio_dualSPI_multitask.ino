#define AUDIO_FILENAME "/48000_u16le.pcm"
#define VIDEO_WIDTH 220L
#define VIDEO_HEIGHT 176L
#define FPS 10
#define VIDEO_FILENAME "/220_10fps.mjpeg"
#define FRAMEBUFFER_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT * 2)
#define IMAGE_BUFFER_SIZE (FRAMEBUFFER_SIZE / 4)
#define READ_BUFFER_SIZE 2048

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
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, 33 /* RST */, 3 /* rotation */, true /* IPS */);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);

#include <esp_jpg_decode.h>

uint8_t *aBuf;
uint8_t *vBuf1;
int vBuf1Size = 0;
uint8_t *vBuf2;
int vBuf2Size = 0;
uint16_t *framebuffer;
uint8_t loaded_buffer_idx = 2;

int next_frame = 0;
int played_frames = 0;
unsigned long total_sd_pcm = 0;
unsigned long total_push_audio = 0;
unsigned long total_sd_mjpeg = 0;
unsigned long total_decode_mjpeg = 0;
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
      uint8_t *buf;
      int bufSize;
      if (loaded_buffer_idx == 1)
      {
        buf = vBuf1;
        bufSize = vBuf1Size;
      }
      else
      {
        buf = vBuf2;
        bufSize = vBuf2Size;
      }

      if (bufSize)
      {
        unsigned long ms = millis();
        // esp_jpg_decode(bufSize, JPG_SCALE_NONE, buff_reader, gfx_writer, buf /* arg */);
        esp_jpg_decode(bufSize, JPG_SCALE_NONE, buff_reader, framebuffer_writer, buf /* arg */);
        total_decode_mjpeg += millis() - ms;
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
        .dma_buf_count = 7,
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
        gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
        if (!vFile || vFile.isDirectory())
        {
          Serial.println(F("ERROR: Failed to open video file for reading"));
          gfx->println(F("ERROR: Failed to open video file for reading"));
        }
        else
        {
          aBuf = (uint8_t *)malloc(6400);
          if (!aBuf)
          {
            Serial.println(F("aBuf malloc failed!"));
          }
          else
          {
            vBuf1 = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
            if (!vBuf1)
            {
              Serial.println(F("vBuf1 malloc failed!"));
            }
            else
            {
              vBuf2 = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
              if (!vBuf2)
              {
                Serial.println(F("vBuf2 malloc failed!"));
              }
              else
              {
                framebuffer = (uint16_t *)malloc(FRAMEBUFFER_SIZE);
                if (!framebuffer)
                {
                  Serial.println(F("framebuffer malloc failed!"));
                }
                else
                {
                  uint8_t *buf = (uint8_t *)malloc(READ_BUFFER_SIZE);
                  int r = vFile.read(buf, READ_BUFFER_SIZE);
                  int i = 3;
                  bool found_FFD9 = false;
                  if (!buf)
                  {
                    Serial.println(F("buf malloc failed!"));
                  }
                  else
                  {
                    xTaskCreatePinnedToCore(&videoTask, "videoTask", 2048, NULL, 0, NULL, 0);

                    int vBufOffset = 0;
                    uint8_t *vBuf;

                    Serial.println(F("Start audio video"));
                    start_ms = millis();
                    curr_ms = millis();
                    next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
                    while (vFile.available() && aFile.available())
                    {
                      // Dump audio
                      aFile.read(aBuf, 9600);
                      total_sd_pcm += millis() - curr_ms;
                      curr_ms = millis();

                      i2s_write_bytes((i2s_port_t)0, (char *)aBuf, 1600, 0);
                      i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 1600), 1600, 0);
                      i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 3200), 1600, 0);
                      i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 4800), 1600, 0);
                      i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 6400), 1600, 0);
                      i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 8000), 1600, 0);
                      total_push_audio += millis() - curr_ms;
                      curr_ms = millis();

                      // Load MJPEG frame
                      if (loaded_buffer_idx == 1)
                      {
                        vBuf = vBuf2;
                        vBuf2Size = 0;
                      }
                      else
                      {
                        vBuf = vBuf1;
                        vBuf1Size = 0;
                      }

                      while ((r > 0) && (!found_FFD9))
                      {
                        if ((vBufOffset > 0) && (vBuf[vBufOffset - 1] == 0xFF) && (buf[0] == 0xD9)) // JPEG trailer
                        {
                          found_FFD9 = true;
                        }
                        else
                        {
                          while ((i < r) && (!found_FFD9))
                          {
                            if ((buf[i] == 0xFF) && (buf[i + 1] == 0xD9)) // JPEG trailer
                            {
                              found_FFD9 = true;
                              ++i;
                            }
                            ++i;
                          }
                        }

                        // Serial.printf("i: %d\n", i);
                        memcpy(vBuf + vBufOffset, buf, i);
                        vBufOffset += i;
                        size_t o = r - i;
                        if (o > 0)
                        {
                          // Serial.printf("o: %d\n", o);
                          memcpy(buf, buf + i, o);
                          r = vFile.read(buf + o, READ_BUFFER_SIZE - o) + o;
                          // Serial.printf("r: %d\n", r);
                        }
                        else
                        {
                          r = vFile.read(buf, READ_BUFFER_SIZE);
                        }
                        i = 0;
                      }
                      if (found_FFD9)
                      {
                        if (loaded_buffer_idx == 1)
                        {
                          vBuf2Size = vBufOffset;
                          loaded_buffer_idx = 2;
                        }
                        else
                        {
                          vBuf1Size = vBufOffset;
                          loaded_buffer_idx = 1;
                        }
                      }

                      found_FFD9 = false;
                      vBufOffset = 0;

                      total_sd_mjpeg += millis() - curr_ms;
                      curr_ms = millis();

                      gfx->startWrite();
                      gfx->writePixels((uint16_t *)framebuffer, VIDEO_WIDTH * VIDEO_HEIGHT);
                      gfx->endWrite();
                      total_push_video += millis() - curr_ms;

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
                    Serial.printf("SD MJPEG: %d ms (%f %%)\n", total_sd_mjpeg, 100.0 * total_sd_mjpeg / time_used);
                    Serial.printf("Decode MJPEG: %d ms (%f %%)\n", total_decode_mjpeg, 100.0 * total_decode_mjpeg / time_used);
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
                    gfx->printf("SD MJPEG: %d ms (%f %%)\n", total_sd_mjpeg, 100.0 * total_sd_mjpeg / time_used);
                    gfx->printf("Decode MJPEG: %d ms (%f %%)\n", total_decode_mjpeg, 100.0 * total_decode_mjpeg / time_used);
                    gfx->printf("Push video: %d ms (%f %%)\n", total_push_video, 100.0 * total_push_video / time_used);
                    gfx->printf("Remain: %d ms (%f %%)\n", total_remain, 100.0 * total_remain / time_used);

                    i2s_driver_uninstall((i2s_port_t)0); //stop & destroy i2s driver
                  }
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
  digitalWrite(TFT_BL, LOW);
#endif
}

void loop(void)
{
}

static size_t file_reader(void *arg, size_t index, uint8_t *buf, size_t len)
{
  // Serial.printf("index: %d, len: %d\n", index, len);
  File *vFile = (File *)arg;
  size_t l;
  if (buf)
  {
    return vFile->read(buf, len);
  }
  else
  {
    vFile->seek(len, SeekCur);
    return len;
  }
}

static size_t buff_reader(void *arg, size_t index, uint8_t *buf, size_t len)
{
  // Serial.printf("index: %d, len: %d\n", index, len);
  uint8_t *b = (uint8_t *)arg;

  if (buf)
  {
    memcpy(buf, b + index, len);
  }
  return len; // Returns number of bytes read
}

static bool gfx_writer(void *arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
  // Serial.printf("%d, %d, %d, %d\n", x, y, w, h);
  if (data)
  {
    gfx->draw24bitRGBBitmap(x, y, data, w, h);
  }
  return true; // Continue to decompression
}

static bool framebuffer_writer(void *arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
  // Serial.printf("%d, %d, %d, %d\n", x, y, w, h);
  if (data)
  {
    for (int i = 0; i < h; ++i)
    {
      for (int j = 0; j < w; ++j)
      {
        framebuffer[(y + i) * VIDEO_WIDTH + x + j] = gfx->color565(*(data++), *(data++), *(data++));
      }
    }
  }
  return true; // Continue to decompression
}
