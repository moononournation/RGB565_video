#ifndef _MJPEGCLASS_H_
#define _MJPEGCLASS_H_

#define READ_BUFFER_SIZE 1024
#define MAXOUTPUTSIZE (MAX_BUFFERED_PIXELS / 16 / 16)
#define NUMBER_OF_DECODE_BUFFER 2
#define NUMBER_OF_DRAW_BUFFER 4

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_heap_caps.h>
#include <FS.h>

#include <JPEGDEC.h>

typedef struct
{
  int32_t size;
  uint8_t *buf;
} mjpegBuf;

typedef struct
{
  xQueueHandle xqh;
  JPEG_DRAW_CALLBACK *drawFunc;
} paramDrawTask;

typedef struct
{
  xQueueHandle xqh;
  mjpegBuf *mBuf;
  JPEG_DRAW_CALLBACK *drawFunc;
} paramDecodeTask;

static JPEGDRAW jpegdraws[NUMBER_OF_DRAW_BUFFER];
static int _decode_queue_cnt = 0;
static int _decode_cnt = 0;
static int _draw_queue_cnt = 0;
static int _draw_cnt = 0;
static JPEGDEC _jpegDec;
static xQueueHandle _xqh;
static bool _enableDrawMultiTask;
static bool _useBigEndian;
static unsigned long total_decode_video_ms = 0;

static int queueDrawMCU(JPEGDRAW *pDraw)
{
  int len = pDraw->iWidth * pDraw->iHeight * 2;
  JPEGDRAW *j = &jpegdraws[_draw_queue_cnt % NUMBER_OF_DRAW_BUFFER];
  j->x = pDraw->x;
  j->y = pDraw->y;
  j->iWidth = pDraw->iWidth;
  j->iHeight = pDraw->iHeight;
  memcpy(j->pPixels, pDraw->pPixels, len);

  // printf("queueDrawMCU start.\n");
  ++_draw_queue_cnt;
  if ((_draw_queue_cnt - _draw_cnt) > NUMBER_OF_DRAW_BUFFER)
  {
    printf("draw queue overflow!\n");
    while ((_draw_queue_cnt - _draw_cnt) > NUMBER_OF_DRAW_BUFFER)
    {
      vTaskDelay(1);
    }
  }

  xQueueSend(_xqh, &j, 0);
  // printf("queueDrawMCU end.\n");

  return 1;
}

static void decodeTask(void *arg)
{
  paramDecodeTask *p = (paramDecodeTask *)arg;
  mjpegBuf *mBuf;
  printf("decodeTask start.\n");
  while (xQueueReceive(p->xqh, &mBuf, portMAX_DELAY))
  {
    // printf("mBuf->size: %d\n", mBuf->size);
    // printf("mBuf->buf start: %X %X, end: %X, %X.\n", mBuf->buf[0], mBuf->buf[1], mBuf->buf[mBuf->size - 2], mBuf->buf[mBuf->size - 1]);
    unsigned long s = millis();

    _jpegDec.openRAM(mBuf->buf, mBuf->size, p->drawFunc);

    // _jpegDec.setMaxOutputSize(MAXOUTPUTSIZE);
    if (_useBigEndian)
    {
      _jpegDec.setPixelType(RGB565_BIG_ENDIAN);
    }
    _jpegDec.decode(0, 0, 0);
    _jpegDec.close();

    total_decode_video_ms += millis() - s;

    ++_decode_cnt;
  }
  vQueueDelete(p->xqh);
  printf("decodeTask end.\n");
  vTaskDelete(NULL);
}

static void drawTask(void *arg)
{
  paramDrawTask *p = (paramDrawTask *)arg;
  JPEGDRAW *pDraw;
  printf("drawTask start.\n");
  while (xQueueReceive(p->xqh, &pDraw, portMAX_DELAY))
  {
    // printf("drawTask work start: x: %d, y: %d, iWidth: %d, iHeight: %d.\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
    p->drawFunc(pDraw);
    // printf("drawTask work end.\n");
    ++_draw_cnt;
  }
  vQueueDelete(p->xqh);
  printf("drawTask end.\n");
  vTaskDelete(NULL);
}

class MjpegClass
{
public:
  bool setup(Stream *input, int32_t mjpegBufufSize, JPEG_DRAW_CALLBACK *pfnDraw,
             bool enableDecodeMultiTask, bool enableDrawMultiTask, bool useBigEndian)
  {
    _input = input;
    _pfnDraw = pfnDraw;
    _enableDecodeMultiTask = enableDecodeMultiTask;
    _enableDrawMultiTask = enableDrawMultiTask;
    _useBigEndian = useBigEndian;

    for (int i = 0; i < NUMBER_OF_DECODE_BUFFER; ++i)
    {
      _mjpegBufs[i].buf = (uint8_t *)malloc(mjpegBufufSize);
      if (_mjpegBufs[i].buf)
      {
        printf("#%d decode buffer allocated.\n", i);
      }
    }
    _mjpeg_buf = _mjpegBufs[_mBufIdx].buf;

    if (!_read_buf)
    {
      _read_buf = (uint8_t *)malloc(READ_BUFFER_SIZE);
    }
    if (_read_buf)
    {
      printf("Read buffer allocated.\n");
    }

    if (_enableDrawMultiTask)
    {
      if (_enableDecodeMultiTask)
      {
        _xqh = xQueueCreate(NUMBER_OF_DRAW_BUFFER, sizeof(JPEGDRAW));
        _pDrawTask.xqh = _xqh;
        _pDrawTask.drawFunc = pfnDraw;
        _pDecodeTask.xqh = xQueueCreate(NUMBER_OF_DECODE_BUFFER, sizeof(mjpegBuf));
        _pDecodeTask.drawFunc = queueDrawMCU;
        xTaskCreatePinnedToCore(decodeTask, "decodeTask", 1600, &_pDecodeTask, 2, &_decodeTask, 0);
        xTaskCreatePinnedToCore(drawTask, "drawTask", 1600, &_pDrawTask, 2, &_drawTask, 1);
      }
      else
      {
        _xqh = xQueueCreate(NUMBER_OF_DRAW_BUFFER, sizeof(JPEGDRAW));
        _pDrawTask.xqh = _xqh;
        _pDrawTask.drawFunc = pfnDraw;
        xTaskCreatePinnedToCore(drawTask, "drawTask", 1600, &_pDrawTask, 2, &_drawTask, 0);
      }
    }
    else
    {
      if (_enableDecodeMultiTask)
      {
        _pDecodeTask.xqh = xQueueCreate(NUMBER_OF_DECODE_BUFFER, sizeof(mjpegBuf));
        _pDrawTask.drawFunc = pfnDraw;
        xTaskCreatePinnedToCore(decodeTask, "decodeTask", 1600, &_pDecodeTask, 2, &_decodeTask, 0);
        xTaskCreatePinnedToCore(drawTask, "drawTask", 1600, &_pDrawTask, 2, &_drawTask, 1);
      }
    }

    for (int i = 0; i < NUMBER_OF_DRAW_BUFFER; i++)
    {
      if (!jpegdraws[i].pPixels)
      {
        jpegdraws[i].pPixels = (uint16_t *)heap_caps_malloc(MAXOUTPUTSIZE * 16 * 16 * 2, MALLOC_CAP_DMA);
      }
      if (jpegdraws[i].pPixels)
      {
        printf("#%d draw buffer allocated.\n", i);
      }
    }

    return true;
  }

  bool readMjpegBuf()
  {
    if (_inputindex == 0)
    {
      _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
      _inputindex += _buf_read;
    }
    _mjpeg_buf_offset = 0;
    int i = 0;
    bool found_FFD8 = false;
    while ((_buf_read > 0) && (!found_FFD8))
    {
      i = 0;
      while ((i < _buf_read) && (!found_FFD8))
      {
        if ((_read_buf[i] == 0xFF) && (_read_buf[i + 1] == 0xD8)) // JPEG header
        {
          // printf("Found FFD8 at: %d.\n", i);
          found_FFD8 = true;
        }
        ++i;
      }
      if (found_FFD8)
      {
        --i;
      }
      else
      {
        _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
      }
    }
    uint8_t *_p = _read_buf + i;
    _buf_read -= i;
    bool found_FFD9 = false;
    if (_buf_read > 0)
    {
      i = 3;
      while ((_buf_read > 0) && (!found_FFD9))
      {
        if ((_mjpeg_buf_offset > 0) && (_mjpeg_buf[_mjpeg_buf_offset - 1] == 0xFF) && (_p[0] == 0xD9)) // JPEG trailer
        {
          found_FFD9 = true;
        }
        else
        {
          while ((i < _buf_read) && (!found_FFD9))
          {
            if ((_p[i] == 0xFF) && (_p[i + 1] == 0xD9)) // JPEG trailer
            {
              found_FFD9 = true;
              ++i;
            }
            ++i;
          }
        }

        // printf("i: %d\n", i);
        memcpy(_mjpeg_buf + _mjpeg_buf_offset, _p, i);
        _mjpeg_buf_offset += i;
        int32_t o = _buf_read - i;
        if (o > 0)
        {
          // printf("o: %d\n", o);
          memcpy(_read_buf, _p + i, o);
          _buf_read = _input->readBytes(_read_buf + o, READ_BUFFER_SIZE - o);
          _p = _read_buf;
          _inputindex += _buf_read;
          _buf_read += o;
          // printf("_buf_read: %d\n", _buf_read);
        }
        else
        {
          _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
          _p = _read_buf;
          _inputindex += _buf_read;
        }
        i = 0;
      }
      if (found_FFD9)
      {
        // printf("Found FFD9 at: %d.\n", _mjpeg_buf_offset);
        return true;
      }
    }

    return false;
  }

  int getWidth()
  {
    return _jpegDec.getWidth();
  }

  int getHeight()
  {
    return _jpegDec.getHeight();
  }

  bool drawJpg()
  {
    if (_enableDecodeMultiTask)
    {
      ++_decode_queue_cnt;
      if ((_decode_queue_cnt - _decode_cnt) > NUMBER_OF_DECODE_BUFFER)
      {
        printf("decode queue overflow!\n");
        while ((_decode_queue_cnt - _decode_cnt) > NUMBER_OF_DECODE_BUFFER)
        {
          vTaskDelay(1);
        }
      }
      // printf("queue decodeTask start\n");
      mjpegBuf *mBuf = &_mjpegBufs[_mBufIdx];
      mBuf->size = _mjpeg_buf_offset;
      // printf("_mjpegBufs[%d].size: %d.\n", _mBufIdx, _mjpegBufs[_mBufIdx].size);
      // printf("_mjpegBufs[%d].buf start: %X %X, end: %X, %X.\n", _mjpegBufs, _mjpegBufs[_mBufId].buf[0], _mjpegBufs[_mBufIdx].buf[1], _mjpegBufs[_mBufIdx].buf[_mjpeg_buf_offset - 2], _mjpegBufs[_mBufIdx].buf[_mjpeg_buf_offset - 1]);
      xQueueSend(_pDecodeTask.xqh, &mBuf, 0);
      ++_mBufIdx;
      if (_mBufIdx >= NUMBER_OF_DECODE_BUFFER)
      {
        _mBufIdx = 0;
      }
      _mjpeg_buf = _mjpegBufs[_mBufIdx].buf;
      // printf("queue decodeTask end\n");
    }
    else
    {
      unsigned long s = millis();

      _remain = _mjpeg_buf_offset;

      _jpegDec.openRAM(_mjpegBufs[_mBufIdx].buf, _remain, _pDrawTask.drawFunc);

      // _jpegDec.setMaxOutputSize(MAXOUTPUTSIZE);
      if (_useBigEndian)
      {
        _jpegDec.setPixelType(RGB565_BIG_ENDIAN);
      }
      _jpegDec.decode(0, 0, 0);
      _jpegDec.close();

      total_decode_video_ms += millis() - s;
    }

    return true;
  }

private:
  Stream *_input;
  JPEG_DRAW_CALLBACK *_pfnDraw;
  bool _enableDecodeMultiTask;

  uint8_t *_read_buf;
  int32_t _mjpeg_buf_offset = 0;

  TaskHandle_t _decodeTask;
  TaskHandle_t _drawTask;
  paramDecodeTask _pDecodeTask;
  paramDrawTask _pDrawTask;
  uint8_t *_mjpeg_buf;
  uint8_t _mBufIdx = 0;

  int32_t _inputindex = 0;
  int32_t _buf_read;
  int32_t _remain = 0;
  mjpegBuf _mjpegBufs[NUMBER_OF_DECODE_BUFFER];
};

#endif // _MJPEGCLASS_H_
