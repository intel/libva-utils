# 8bit
./av1encode -n 8 -f 30 -intra_period 4 --rcmode CQP --srcyuv ~/test/others/yuvstreams/Life_1280x720_yuv420p.yuv --fourcc IYUV --recyuv ./rec.yuv  --level 8 --width 1280 --height 720 --base_q_idx 128 -o ./out.av1 --normal_mode

# 10bit
./av1encode -n 8 -f 30 -intra_period 4 --rcmode CQP --srcyuv ~/yuv/VQEG_HD3_14_30fps_300f_1920x1080_yuv420p10le.yuv --fourcc P010 --level 16 --width 1920 --height 1080 --base_q_idx 128 -o ./test.av1 --normal_mode
