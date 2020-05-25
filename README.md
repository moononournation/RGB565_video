# RGB565_video

Simple example for playing RGB565 raw video

## Convert video for SPIFFS

`ffmpeg -t 1 -i input.mp4 -vf "fps=12,scale=240:-1" -c:v rawvideo -pix_fmt rgb565le output.rgb`

## Convert video for SD card

### 192x108@15fps
`ffmpeg -i input.mp4 -vf "fps=15,scale=192:-1" -c:v rawvideo -pix_fmt rgb565le 192_15fps.rgb`

### 240x135@15FPS

`ffmpeg -i input.mp4 -vf "fps=10,scale=240:-1" -c:v rawvideo -pix_fmt rgb565le 240_10fps.rgb`

## Convert audio + video for SD card

### audio

`ffmpeg -i input.mp4 -f u16be -acodec pcm_u16le -ar 32000 -ac 1 -af "volume=0.5" 32000_u16le.pcm`

### video

`ffmpeg -i input.mp4 -vf "fps=10,scale=224:-1" -c:v rawvideo -pix_fmt rgb565le 224_10fps.rgb`
