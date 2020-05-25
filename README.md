# RGB565_video

Simple example for playing RGB565 raw video

`ffmpeg -t 1 -i input.mp4 -vf "fps=12,scale=240:-1" -c:v rawvideo -pix_fmt rgb565le output.rgb`

`ffmpeg -i input.mp4 -vf "fps=15,scale=192:-1" -c:v rawvideo -pix_fmt rgb565le 192_15fps.rgb`

`ffmpeg -i input.mp4 -vf "fps=10,scale=240:-1" -c:v rawvideo -pix_fmt rgb565le 240_10fps.rgb`
