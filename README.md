# RGB565_video

Simple example for playing RGB565 raw video

## Convert video for SPIFFS

#### 220x124@12fps

`ffmpeg -t 1 -i input.mp4 -vf "fps=12,scale=220:-1" -c:v rawvideo -pix_fmt rgb565le output.rgb`

## Convert audio + video for SD card

### audio

#### 48 kHz

`ffmpeg -i input.mp4 -f u16be -acodec pcm_u16le -ar 48000 -ac 1 -af "volume=0.5" 48000_u16le.pcm`

### video

#### 220x176@10fps

`ffmpeg -i input.mp4 -vf "fps=12,scale=-1:176,crop=220:in_h:(in_w-220)/2:0" -c:v rawvideo -pix_fmt rgb565le 220_10fps.rgb`

#### 220x176@8fps

`ffmpeg -i input.mp4 -vf "fps=8,scale=-1:176,crop=220:in_h:(in_w-220)/2:0" -c:v rawvideo -pix_fmt rgb565le 220_8fps.rgb`

### Animated GIF

#### 220x176@15fps

`ffmpeg -i input.mp4 -vf "fps=15,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -loop 0 220_15fps.gif`

#### 288x162@15fps

`ffmpeg -i input.mp4 -vf "fps=15,scale=288:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -loop 0 288_15fps.gif`
