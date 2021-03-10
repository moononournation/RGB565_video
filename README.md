# RGB565_video

Simple example for playing RGB565 raw video

![Prototype](https://content.instructables.com/ORIG/FXF/C099/KDFYF9XB/FXFC099KDFYF9XB.jpg?crop=1.2%3A1&width=306)

Please find more details at instructables:
https://www.instructables.com/id/Play-Video-With-ESP32/

## Convert video for SPIFFS

#### 220x124@12fps

`ffmpeg -t 2 -i input.mp4 -vf "fps=15,scale=-1:124:flags=lanczos,crop=220:in_h:(in_w-220)/2:0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -c:v rawvideo -pix_fmt rgb565be output.rgb`

## Convert audio + video for SD card

### audio

#### 44.1 kHz

`ffmpeg -i input.mp4 -f u16le -acodec pcm_u16le -ar 44100 -ac 1 44100_u16le.pcm`

#### MP3

`ffmpeg -i input.mp4 -ar 44100 -ac 1 -q:a 9 44100.mp3`

### video

#### 220x176@7fps

`ffmpeg -i input.mp4 -vf "fps=7,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0" -c:v rawvideo -pix_fmt rgb565be 220_7fps.rgb`

#### 220x176@9fps

`ffmpeg -i input.mp4 -vf "fps=9,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0" -c:v rawvideo -pix_fmt rgb565be 220_9fps.rgb`

### Animated GIF

#### 220x176@12fps

`ffmpeg -i input.mp4 -vf "fps=12,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -loop -1 220_12fps.gif`

#### 220x176@15fps

`ffmpeg -i input.mp4 -vf "fps=15,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -loop -1 220_15fps.gif`

### Motion JPEG

#### 320x240@12fps

`ffmpeg -i input.mp4 -vf "fps=12,scale=-1:240:flags=lanczos,crop=320:in_h:(in_w-320)/2:0" -q:v 9 320_12fps.mjpeg`

#### 320x240@15fps

`ffmpeg -i input.mp4 -vf "fps=15,scale=-1:240:flags=lanczos,crop=320:in_h:(in_w-320)/2:0" -q:v 9 320_15fps.mjpeg`

#### 220x176@24fps

`ffmpeg -i input.mp4 -vf "fps=24,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0" -q:v 9 220_24fps.mjpeg`

#### 220x176@30fps

`ffmpeg -i input.mp4 -vf "fps=30,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0" -q:v 9 220_30fps.mjpeg`

#### 208x176@30fps

`ffmpeg -i input.mp4 -vf "fps=30,scale=-1:176:flags=lanczos,crop=208:in_h:(in_w-208)/2:0" -q:v 9 208_30fps.mjpeg`

## Sample Video Source

[https://youtu.be/upjTmKXDnFU](https://youtu.be/upjTmKXDnFU){:target="_blank"}
[https://steamcommunity.com/sharedfiles/filedetails/?id=593882316](https://steamcommunity.com/sharedfiles/filedetails/?id=593882316){:target="_blank"}