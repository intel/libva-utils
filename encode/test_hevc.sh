# 8bit
./hevcencode -w 320 -h 320 -n 8 -o ./test.265 -rcmode CQP -srcyuv ~/test/others/yuvstreams/Seeking_320x320_61f.yuv --fourcc IYUV --profile 1 --idr_period 30

# 10bit
./hevcencode -w 1920 -h 1080 -n 8 -o ./test.265 -rcmode CQP -srcyuv ~/yuv/VQEG_HD3_14_30fps_300f_1920x1080_yuv420p10le.yuv --fourcc P010 --profile 2 --idr_period 30
