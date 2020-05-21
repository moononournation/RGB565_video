# RGB565_video

Simple example for playing RGB565 raw video

`ffmpeg -t 1 -i input.mp4 -vf "fps=12,scale=240:-1" -c:v rawvideo -pix_fmt rgb565le output.rgb`