#ifndef _MJPEGCLASS_H_
#define _MJPEGCLASS_H_

#define READ_BUFFER_SIZE 1024
#define MAXOUTPUTSIZE 8
#define NUMBER_OF_DRAW_BUFFER 4

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_heap_caps.h>
#include <FS.h>

#include <JPEGDEC.h>

typedef struct
{
  JPEG_DRAW_CALLBACK *drawFunc;
} paramDrawTask;

static xQueueHandle xqh = 0;
static JPEGDRAW jpegdraws[NUMBER_OF_DRAW_BUFFER];
static int queue_cnt, draw_cnt;

static int queueDrawMCU(JPEGDRAW *pDraw)
{
  ++queue_cnt;
  while ((queue_cnt - draw_cnt) > NUMBER_OF_DRAW_BUFFER)
  {
    delay(1);
  }

  int len = pDraw->iWidth * pDraw->iHeight * 2;
  JPEGDRAW *j = &jpegdraws[queue_cnt % NUMBER_OF_DRAW_BUFFER];
  j->x = pDraw->x;
  j->y = pDraw->y;
  j->iWidth = pDraw->iWidth;
  j->iHeight = pDraw->iHeight;
  memcpy(j->pPixels, pDraw->pPixels, len);

  xQueueSend(xqh, &j, 0);
  return 1;
}

static void drawTask(void *arg)
{
  paramDrawTask *p = (paramDrawTask *)arg;
  for (int i = 0; i < NUMBER_OF_DRAW_BUFFER; i++)
  {
    jpegdraws[i].pPixels = (uint16_t *)heap_caps_malloc(MAXOUTPUTSIZE * 16 * 16 * 2, MALLOC_CAP_DMA);
    Serial.printf("#%d draw buffer allocated\n", i);
  }
  JPEGDRAW *pDraw;
  Serial.println("drawTask start");
  while (xQueueReceive(xqh, &pDraw, portMAX_DELAY))
  {
    // Serial.printf("task work: x: %d, y: %d, iWidth: %d, iHeight: %d\r\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
    p->drawFunc(pDraw);
    // Serial.println("task work done");
    ++draw_cnt;
  }
  vQueueDelete(xqh);
  Serial.println("drawTask end");
  vTaskDelete(NULL);
}

class MjpegClass
{
public:
  bool setup(Stream *input, uint8_t *mjpeg_buf, JPEG_DRAW_CALLBACK *pfnDraw, bool enableMultiTask, bool useBigEndian)
  {
    _input = input;
    _mjpeg_buf = mjpeg_buf;
    _pfnDraw = pfnDraw;
    _enableMultiTask = enableMultiTask;
    _useBigEndian = useBigEndian;

    _mjpeg_buf_offset = 0;
    _inputindex = 0;
    _remain = 0;

    queue_cnt = 0;
    draw_cnt = 0;

    if (!_read_buf)
    {
      _read_buf = (uint8_t *)malloc(READ_BUFFER_SIZE);
    }

    if (_enableMultiTask)
    {
      if (!xqh)
      {
        TaskHandle_t task;
        _p.drawFunc = pfnDraw;
        xqh = xQueueCreate(NUMBER_OF_DRAW_BUFFER, sizeof(JPEGDRAW));
        xTaskCreatePinnedToCore(drawTask, "drawTask", 1600, &_p, 1, &task, 0);
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
          // Serial.printf("Found FFD8 at: %d.\n", i);
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
          // Serial.printf("Found FFD9 at: %d.\n", i);
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

        // Serial.printf("i: %d\n", i);
        memcpy(_mjpeg_buf + _mjpeg_buf_offset, _p, i);
        _mjpeg_buf_offset += i;
        size_t o = _buf_read - i;
        if (o > 0)
        {
          // Serial.printf("o: %d\n", o);
          memcpy(_read_buf, _p + i, o);
          _buf_read = _input->readBytes(_read_buf + o, READ_BUFFER_SIZE - o);
          _p = _read_buf;
          _inputindex += _buf_read;
          _buf_read += o;
          // Serial.printf("_buf_read: %d\n", _buf_read);
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
        return true;
      }
    }

    return false;
  }

  int getWidth()
  {
    return _jpeg.getWidth();
  }

  int getHeight()
  {
    return _jpeg.getHeight();
  }

  bool drawJpg()
  {
    _remain = _mjpeg_buf_offset;

    if (_enableMultiTask)
    {
      _jpeg.openRAM(_mjpeg_buf, _remain, queueDrawMCU);
    }
    else
    {
      _jpeg.openRAM(_mjpeg_buf, _remain, _pfnDraw);
    }

    _jpeg.setMaxOutputSize(MAXOUTPUTSIZE);
    if (_useBigEndian)
    {
      _jpeg.setPixelType(RGB565_BIG_ENDIAN);
    }
    _jpeg.decode(0, 0, 0);
    _jpeg.close();

    return true;
  }

private:
  Stream *_input;
  uint8_t *_mjpeg_buf;
  JPEG_DRAW_CALLBACK *_pfnDraw;
  bool _enableMultiTask;
  bool _useBigEndian;

  uint8_t *_read_buf;
  int32_t _mjpeg_buf_offset;

  JPEGDEC _jpeg;
  paramDrawTask _p;

  int32_t _inputindex;
  int32_t _buf_read;
  int32_t _remain;
};

#endif // _MJPEGCLASS_H_
