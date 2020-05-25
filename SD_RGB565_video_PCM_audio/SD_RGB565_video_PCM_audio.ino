#define AUDIO_FILENAME "/32000_u16le.pcm"
#define VIDEO_WIDTH 224
#define VIDEO_HEIGHT 126
#define VIDEO_FILENAME "/224_10fps.rgb"

#include "SPI.h"
#include "Arduino_ESP32SPI.h"
#include "Arduino_Display.h"

#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)
#define TFT_BL 32
#define SS 4
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO /* MISO */);
Arduino_ILI9341_M5STACK *gfx = new Arduino_ILI9341_M5STACK(bus, 33 /* RST */, 1 /* rotation */);
#elif defined(ARDUINO_ODROID_ESP32)
#define TFT_BL 14
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */, SCK, MOSI, -1 /* MISO */);
// Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);
#elif defined(ARDUINO_T) // TTGO T-Watch
#define TFT_BL 12
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, SCK, MOSI, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240, 240, 0, 80);
#else /* not a specific hardware */
#define TFT_BL 2
#define SCK 21
#define MOSI 19
#define MISO 22
#define SS 0
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(15 /* DC */, 12 /* CS */, SCK, MOSI, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
#endif /* not a specific hardware */

#include "FS.h"
#include "SD.h"
#include <driver/i2s.h>

void setup()
{
  Serial.begin(115200);

  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  // Init Audio
  i2s_config_t i2s_config_dac = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = 32000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
      .dma_buf_count = 5,
      .dma_buf_len = 800,
      .use_apll = false,
  };
  Serial.printf("%p\n", &i2s_config_dac);
  if (i2s_driver_install(I2S_NUM_0, &i2s_config_dac, 0, NULL) != ESP_OK)
  {
    Serial.println("ERROR: Unable to install I2S drives\n");
  }
  i2s_set_pin((i2s_port_t)0, NULL);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
  i2s_zero_dma_buffer((i2s_port_t)0);

  // Init SD card
  SPIClass spi = SPIClass(VSPI);
  spi.begin(SCK, MISO, MOSI, SS);

  if (!SD.begin(SS, spi, 80000000))
  {
    Serial.println("Card Mount Failed");
  }
  else
  {
    File aFile = SD.open(AUDIO_FILENAME);
    if (!aFile || aFile.isDirectory())
    {
      Serial.println("- failed to open audio file for reading");
    }
    else
    {
      File vFile = SD.open(VIDEO_FILENAME);
      if (!vFile || vFile.isDirectory())
      {
        Serial.println("- failed to open video file for reading");
      }
      else
      {
        Serial.println("- read from video file:");
        uint8_t *buf = (uint8_t *)malloc(VIDEO_WIDTH * VIDEO_HEIGHT * 2);

        Serial.println("Start audio video");
        gfx->setAddrWindow((gfx->width() - VIDEO_WIDTH) / 2, (gfx->height() - VIDEO_HEIGHT) / 2, VIDEO_WIDTH, VIDEO_HEIGHT);
        while (vFile.available() && aFile.available())
        {
          // Dump video
          int l = vFile.read(buf, VIDEO_WIDTH * VIDEO_HEIGHT * 2);
          gfx->startWrite();
          gfx->writePixels((uint16_t *)buf, l >> 1);
          gfx->endWrite();

          // Dump audio
          aFile.read(buf, 6400);
          i2s_write_bytes((i2s_port_t)0, (char *)buf, 1600, 0);
          i2s_write_bytes((i2s_port_t)0, (char *)(buf + 1600), 1600, 0);
          i2s_write_bytes((i2s_port_t)0, (char *)(buf + 3200), 1600, 0);
          i2s_write_bytes((i2s_port_t)0, (char *)(buf + 4800), 1600, 0);
        }
        Serial.println("End audio video");

        i2s_driver_uninstall((i2s_port_t)0); //stop & destroy i2s driver

#ifdef TFT_BL
        delay(1000);
        digitalWrite(TFT_BL, LOW);
#endif
      }
    }
  }
}

void loop(void)
{
}
