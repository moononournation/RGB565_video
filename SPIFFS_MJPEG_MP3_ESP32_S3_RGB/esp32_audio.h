#include "driver/i2s.h"

#include "MP3DecoderHelix.h"

static i2s_port_t _i2s_num;
static esp_err_t i2s_init(i2s_port_t i2s_num, uint32_t sample_rate,
                          int mck_io_num,   /*!< MCK in out pin. Note that ESP32 supports setting MCK on GPIO0/GPIO1/GPIO3 only*/
                          int bck_io_num,   /*!< BCK in out pin*/
                          int ws_io_num,    /*!< WS in out pin*/
                          int data_out_num, /*!< DATA out pin*/
                          int data_in_num   /*!< DATA in pin*/
)
{
    _i2s_num = i2s_num;

    esp_err_t ret_val = ESP_OK;

    i2s_config_t i2s_config;
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    i2s_config.sample_rate = sample_rate;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 6;
    i2s_config.dma_buf_len = 160;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = true;
    i2s_config.fixed_mclk = 0;
    i2s_config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    i2s_config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

    i2s_pin_config_t pin_config;
    pin_config.mck_io_num = mck_io_num;
    pin_config.bck_io_num = bck_io_num;
    pin_config.ws_io_num = ws_io_num;
    pin_config.data_out_num = data_out_num;
    pin_config.data_in_num = data_in_num;

    ret_val |= i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    ret_val |= i2s_set_pin(i2s_num, &pin_config);

    return ret_val;
}

static int _samprate = 0;
static void audioDataCallback(MP3FrameInfo &info, int16_t *pwm_buffer, size_t len)
{
    unsigned long s = millis();

    if (_samprate != info.samprate)
    {
        log_d("bitrate: %d, nChans: %d, samprate: %d, bitsPerSample: %d, outputSamps: %d, layer: %d, version: %d",
              info.bitrate, info.nChans, info.samprate, info.bitsPerSample, info.outputSamps, info.layer, info.version);
        i2s_set_clk(I2S_NUM_0, info.samprate /* sample_rate */, info.bitsPerSample /* bits_cfg */, (info.nChans == 2) ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO /* channel */);
        _samprate = info.samprate;
    }
    size_t i2s_bytes_written = 0;
    i2s_write(I2S_NUM_0, pwm_buffer, len * 2, &i2s_bytes_written, portMAX_DELAY);
    // log_d("len: %d, i2s_bytes_written: %d", len, i2s_bytes_written);
    total_play_audio_ms += millis() - s;
}

static libhelix::MP3DecoderHelix _mp3(audioDataCallback);
static uint8_t _frame[MP3_MAX_FRAME_SIZE];
static void mp3_player_task(void *pvParam)
{
    char *file_path = (char *)pvParam;

    _mp3.begin();

    File mp3_file = FFat.open(file_path);
    size_t r, w;
    while (r = mp3_file.read(_frame, MP3_MAX_FRAME_SIZE))
    {
        while (r > 0)
        {
            w = _mp3.write(_frame, r);
            // log_d("r: %d, w: %d\n", r, w);
            if (w == 0)
            {
                vTaskDelay(1);
            }
            else
            {
                r -= w;
            }
        }
        vTaskDelay(1);
    }
    mp3_file.close();
    vTaskDelete(NULL);
}

static void mp3_player_task_start(char *file_path)
{
    BaseType_t ret_val = xTaskCreatePinnedToCore(
        (TaskFunction_t)mp3_player_task,
        (const char *const)"MP3 Player Task",
        (const uint32_t)160 * 1024,
        (void *const)file_path,
        (UBaseType_t)configMAX_PRIORITIES - 1,
        (TaskHandle_t *const)NULL,
        (const BaseType_t)0);
}
