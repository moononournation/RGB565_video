#define MJPEG_FILE "/220_24fps.mjpeg"
#define MJPEG_BUFFER_SIZE (220 * 176 * 2 / 4)
// #define MJPEG_FILE "/320_12fps.mjpeg"
// #define MJPEG_BUFFER_SIZE (320 * 240 * 2 / 4)
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

#include "MjpegClass.h"
static MjpegClass mjpeg;

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
  // if (!SD_MMC.begin("/sdcard", true)) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    int t_fstart = 0, t_delay = 0, t_real_delay, res, delay_until;
    File vFile = SD.open(MJPEG_FILE);
    // File vFile = SD_MMC.open(MJPEG_FILE);
    if (!vFile)
    {
      Serial.println(F("ERROR: File open failed!"));
      gfx->println(F("ERROR: File open failed!"));
    }
    else
    {
      uint8_t *mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);

      mjpeg.setup(vFile, mjpeg_buf, gfx, true);

      Serial.println("MJPEG start");
      while (mjpeg.readMjpegBuf())
      {
        mjpeg.drawJpg();
      }

      Serial.println("MJPEG end");
      vFile.close();
    }
  }
#ifdef TFT_BL
  delay(60000);
  digitalWrite(TFT_BL, LOW);
#endif
}

void loop()
{
}
