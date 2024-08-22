# 8bit
./h264encode -w 1920 -h 1088 -n 8 -o ./test.h264 -rcmode CQP -srcyuv ~/yuv/BFBC2_576f_1920x1088.yuv --fourcc IYUV --profile MP --idr_period 30

# 10bit input then output 8bit
./h264encode -w 1920 -h 1080 -n 8 -o ./test.h264 -rcmode CQP -srcyuv ~/yuv/VQEG_HD3_14_30fps_300f_1920x1080_yuv420p10le.yuv --fourcc P010 --profile HP
