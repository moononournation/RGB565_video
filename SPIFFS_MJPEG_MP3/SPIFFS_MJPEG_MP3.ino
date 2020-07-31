/* Video src: https://youtu.be/iN9PV8e-Rh0 */
#define MP3_FILENAME "/44100.mp3"
#define MJPEG_FILENAME "/220_24fps.mjpeg"
#define FPS 24
#define MJPEG_BUFFER_SIZE (220 * 176 * 2 / 4)
// #define MJPEG_FILENAME "/320_12fps.mjpeg"
// #define MJPEG_BUFFER_SIZE (320 * 240 * 2 / 4)
// #define FPS 12
#include <WiFi.h>
#include <SPIFFS.h>

#include <AudioFileSourceSPIFFS.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2S *out;

#include <Arduino_ESP32SPI.h>
#include <Arduino_Display.h>
#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
#define SS 4
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9341_M5STACK *gfx = new Arduino_ILI9341_M5STACK(bus, 33 /* RST */, 1 /* rotation */);
#elif defined(ARDUINO_ODROID_ESP32)
#define TFT_BL 14
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
// Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);
#elif defined(ARDUINO_T) // TTGO T-Watch
#define TFT_BL 12
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240, 240, 0, 80);
#else /* not a specific hardware */
#define SCK 18
#define MOSI 23
#define MISO 19
#define SS 0
#define TFT_BL 22
#define TFT_BRIGHTNESS 128
// ST7789 Display
// Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(15 /* DC */, 12 /* CS */, SCK, MOSI, MISO);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
// ILI9225 Display
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);
#endif /* not a specific hardware */

#include "MjpegClass.h"
static MjpegClass mjpeg;

int next_frame = 0;
int skipped_frames = 0;
unsigned long total_sd_mjpeg = 0;
unsigned long total_decode_video = 0;
unsigned long total_remain = 0;
unsigned long start_ms, curr_ms, next_frame_ms;

static void playMp3Task(void *arg)
{
  while (true)
  {
    AudioGeneratorMP3 *mp3 = (AudioGeneratorMP3 *)arg;
    if (mp3->isRunning())
    {
      if (!mp3->loop())
      {
        mp3->stop();
        vTaskDelete(NULL);
      }
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
    ledcAttachPin(TFT_BL, 1); // assign TFT_BL pin to channel 1
    ledcSetup(1, 12000, 8);   // 12 kHz PWM, 8-bit resolution
    ledcWrite(1, TFT_BRIGHTNESS);  // brightness 0 - 255
#endif

  // Init SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    file = new AudioFileSourceSPIFFS(MP3_FILENAME);
    out = new AudioOutputI2S(0, 1); // Output to builtInDAC
    // out->SetGain(0.5);
    mp3 = new AudioGeneratorMP3();

    File vFile = SPIFFS.open(MJPEG_FILENAME);
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
        start_ms = millis();
        curr_ms = millis();
        next_frame_ms = start_ms + (++next_frame * 1000 / FPS / 2);

        mjpeg.setup(vFile, mjpeg_buf, gfx, false);
        mp3->begin(file, out);
        xTaskCreatePinnedToCore(&playMp3Task, "playMp3Task", 2048, mp3, 1, NULL, 0);

        unsigned long start = millis();
        // Read video
        while (mjpeg.readMjpegBuf())
        {
          total_sd_mjpeg += millis() - curr_ms;
          curr_ms = millis();

          if (millis() < next_frame_ms) // check show frame or skip frame
          {
            // Play video
            mjpeg.drawJpg();
            total_decode_video += millis() - curr_ms;

            int remain_ms = next_frame_ms - millis();
            total_remain += remain_ms;
            if (remain_ms > 0)
            {
              delay(remain_ms);
            }
          }
          else
          {
            ++skipped_frames;
            Serial.println(F("Skip frame"));
          }

          next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
        }
        Serial.println(F("MP3 audio MJPEG video end"));
        vFile.close();

        int time_used = millis() - start_ms;
        Serial.println(F("End audio video"));
        int played_frames = next_frame - 1 - skipped_frames;
        float fps = 1000.0 * played_frames / time_used;
        Serial.printf("Played frame: %d\n", played_frames);
        Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
        Serial.printf("Time used: %d ms\n", time_used);
        Serial.printf("Expected FPS: %d\n", FPS);
        Serial.printf("Actual FPS: %0.1f\n", fps);
        Serial.printf("SD MJPEG: %d ms (%0.1f %%)\n", total_sd_mjpeg, 100.0 * total_sd_mjpeg / time_used);
        Serial.printf("Decode video: %d ms (%0.1f %%)\n", total_decode_video, 100.0 * total_decode_video / time_used);
        Serial.printf("Remain: %d ms (%0.1f %%)\n", total_remain, 100.0 * total_remain / time_used);

        gfx->setCursor(0, 0);
        gfx->setTextColor(WHITE, BLACK);
        gfx->printf("Played frame: %d\n", played_frames);
        gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
        gfx->printf("Time used: %d ms\n", time_used);
        gfx->printf("Expected FPS: %d\n", FPS);
        gfx->printf("Actual FPS: %0.1f\n", fps);
        gfx->printf("SD MJPEG: %d ms (%0.1f %%)\n", total_sd_mjpeg, 100.0 * total_sd_mjpeg / time_used);
        gfx->printf("Decode video: %d ms (%0.1f %%)\n", total_decode_video, 100.0 * total_decode_video / time_used);
        gfx->printf("Remain: %d ms (%0.1f %%)\n", total_remain, 100.0 * total_remain / time_used);
      }
    }
  }
#ifdef TFT_BL
  delay(60000);
  ledcDetachPin(TFT_BL);
#endif
}

void loop()
{
}
