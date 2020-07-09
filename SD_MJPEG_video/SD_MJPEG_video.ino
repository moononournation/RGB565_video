#define MJPEG_FILE "/220_10fps.mjpeg"
// #define MJPEG_FILE "/320_5fps.mjpeg"
#define IMAGE_BUFFER_SIZE (220 * 176 * 2 / 10)
#define READ_BUFFER_SIZE 2048
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
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
#define TFT_BL 22
#define SCK 18
#define MOSI 23
#define MISO 19
#define SS 0
// Arduino_HWSPI *bus = new Arduino_HWSPI(15 /* DC */, 12 /* CS */, SCK, MOSI, MISO);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
Arduino_HWSPI *bus = new Arduino_HWSPI(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);
#endif /* not a specific hardware */

#include <esp_jpg_decode.h>

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
  // if (!SD.begin(SS, SPI, 80000000))
  if (!SD_MMC.begin("/sdcard", true)) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    int t_fstart = 0, t_delay = 0, t_real_delay, res, delay_until;
    // File vFile = SD.open(MJPEG_FILE);
    File vFile = SD_MMC.open(MJPEG_FILE);
    if (!vFile)
    {
      Serial.println(F("ERROR: File open failed!"));
      gfx->println(F("ERROR: File open failed!"));
    }
    else
    {
      uint8_t *image_buf = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
      uint8_t *buf = (uint8_t *)malloc(READ_BUFFER_SIZE);
      int image_buf_offset = 0;

      Serial.println("MJPEG start");
      unsigned long start = millis();
      int r = vFile.read(buf, READ_BUFFER_SIZE);
      int i;
      bool found_FFD9 = false;
      while ((r > 0) && (buf[0] == 0xFF) && (buf[1] == 0xD8) && (buf[2] == 0xFF)) // JPEG image marker
      {
        i = 3;
        while ((r > 0) && (!found_FFD9))
        {
          if ((image_buf_offset > 0) && (image_buf[image_buf_offset - 1] == 0xFF) && (buf[0] == 0xD9)) // JPEG trailer
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
          memcpy(image_buf + image_buf_offset, buf, i);
          image_buf_offset += i;
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
          esp_jpg_decode(image_buf_offset, JPG_SCALE_NONE, buff_reader, gfx_writer, image_buf /* arg */);
        }
        // Serial.println(millis() - start);
        start = millis();
        found_FFD9 = false;
        image_buf_offset = 0;
      }
      Serial.println("MJPEG end");
      vFile.close();
    }
  }
}

void loop()
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
  if (data)
  {
    // Serial.printf("%d, %d, %d, %d\n", x, y, w, h);
    gfx->draw24bitRGBBitmap(x, y, data, w, h);
  }
  return true; // Continue to decompression
}
