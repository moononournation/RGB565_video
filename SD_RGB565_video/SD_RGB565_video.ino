#define VIDEO_WIDTH 220L
#define VIDEO_HEIGHT 176L
#define RGB565_FILENAME "/220_9fps.rgb"
#define RGB565_BUFFER_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT * 2)
#include <WiFi.h>
#include <SD.h>
#include <SD_MMC.h>

#include <Arduino_HWSPI.h>
#include <Arduino_Display.h>
#define TFT_BRIGHTNESS 128
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
#define SCK 18
#define MOSI 23
#define MISO 19
#define SS 0
#define TFT_BL 22
// ST7789 Display
// Arduino_HWSPI *bus = new Arduino_HWSPI(15 /* DC */, 12 /* CS */, SCK, MOSI, MISO);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
// ILI9225 Display
Arduino_HWSPI *bus = new Arduino_HWSPI(27 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);
#endif /* not a specific hardware */

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

  // Init SD card
  if (!SD.begin(SS, SPI, 80000000)) /* SPI bus mode */
  // if (!SD_MMC.begin()) /* 4-bit SD bus mode */
  // if (!SD_MMC.begin("/sdcard", true)) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    File vFile = SD.open(RGB565_FILENAME);
    // File vFile = SD_MMC.open(RGB565_FILENAME);
    if (!vFile || vFile.isDirectory())
    {
      Serial.println(F("ERROR: Failed to open " RGB565_FILENAME " file for reading"));
      gfx->println(F("ERROR: Failed to open " RGB565_FILENAME " file for reading"));
    }
    else
    {
      uint8_t *buf = (uint8_t *)malloc(RGB565_BUFFER_SIZE);
      if (!buf)
      {
        Serial.println(F("buf malloc failed!"));
      }
      else
      {
        Serial.println(F("RGB565 video start"));
        gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
        while (vFile.available())
        {
          // Read video
          uint32_t l = vFile.read(buf, RGB565_BUFFER_SIZE);

          // Play video
          gfx->startWrite();
          gfx->writeBytes(buf, l);
          gfx->endWrite();
        }
        Serial.println(F("RGB565 video end"));
        vFile.close();
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
