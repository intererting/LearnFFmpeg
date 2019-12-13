#include <string>
#include <iostream>

extern "C"
{
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libavutil/imgutils.h"
};

int main() {
    AVFormatContext *pFormatCtx;
    AVOutputFormat *fmt;
    AVStream *video_st;
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(avcodec_find_encoder(AV_CODEC_ID_H265));
    AVCodec *pCodec;
    AVPacket pkt;
    uint8_t *picture_buf;
    AVFrame *pFrame;
    int picture_size;
    int y_size;
    //获取yuv文件
    FILE *in_file = fopen(R"(C:\Users\user\Desktop\LearnFFmpeg\ds_480x272.yuv)", "rb");
    int in_w = 480, in_h = 272;                              //Input data's width and height
    int framenum = 100;                                   //Frames to encode
    //const char* out_file = "src01.h264";              //Output Filepath
    //const char* out_file = "src01.ts";
//    const char* out_file = "ds.hevc";
    const char *out_file = R"(C:\Users\user\Desktop\LearnFFmpeg\ds.hevc)";

    pFormatCtx = avformat_alloc_context();
    //Guess Format
    fmt = av_guess_format(nullptr, out_file, nullptr);
    //设置输出格式（h264）
    pFormatCtx->oformat = fmt;

    //Open output URL
    if (avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_WRITE) < 0) {
        printf("Failed to open output file! \n");
        return -1;
    }
    video_st = avformat_new_stream(pFormatCtx, nullptr);
    if (nullptr == video_st) {
        return -1;
    }
    //这三个参数必须设置
    video_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    video_st->codecpar->width = in_w;
    video_st->codecpar->height = in_h;
    pCodecCtx->codec_type = video_st->codecpar->codec_type;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtx->width = video_st->codecpar->width;
    pCodecCtx->height = video_st->codecpar->height;
    pCodecCtx->bit_rate = 400000;
    pCodecCtx->gop_size = 250;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;
    //H264
    //pCodecCtx->me_range = 16;
    //pCodecCtx->max_qdiff = 4;
    //pCodecCtx->qcompress = 0.6;
    pCodecCtx->qmin = 10;
    pCodecCtx->qmax = 51;
    //Optional Param
    pCodecCtx->max_b_frames = 3;
    // Set Option
    AVDictionary *param = nullptr;
    //H.264
    if (pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
        //av_dict_set(¶m, "profile", "main", 0);
    }
    //H.265
    if (pCodecCtx->codec_id == AV_CODEC_ID_H265) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }
    //Show some Information
    av_dump_format(pFormatCtx, 0, out_file, 1);
    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Can not find encoder! \n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, &param) < 0) {
        printf("Failed to open encoder! \n");
        return -1;
    }
    pFrame = av_frame_alloc();
    picture_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
    picture_buf = (uint8_t *) av_malloc(picture_size);
    //像素缓冲区
    av_image_fill_arrays(pFrame->data, pFrame->linesize, picture_buf, pCodecCtx->pix_fmt, pCodecCtx->width,
                         pCodecCtx->height, 1);
    //Write File Header
    int headerResult = avformat_write_header(pFormatCtx, nullptr);
    if (headerResult < 0) {
        return -1;
    }
    av_new_packet(&pkt, picture_size);

    y_size = pCodecCtx->width * pCodecCtx->height;

    for (int i = 0; i < framenum; i++) {
        //Read raw YUV data
        if (fread(picture_buf, 1, y_size * 3 / 2, in_file) <= 0) {
            printf("Failed to read raw data! \n");
            return -1;
        } else if (feof(in_file)) {
            break;
        }
        pFrame->data[0] = picture_buf;              // Y
        pFrame->data[1] = picture_buf + y_size;      // U
        pFrame->data[2] = picture_buf + y_size * 5 / 4;  // V
        //PTS
        //pFrame->pts=i;
        //25 位fps
        pFrame->pts = i * (video_st->time_base.den) / ((video_st->time_base.num) * 25);
        //Encode
        avcodec_send_frame(pCodecCtx, pFrame);
        avcodec_receive_packet(pCodecCtx, &pkt);
        pkt.stream_index = video_st->index;
        //写数据
        av_write_frame(pFormatCtx, &pkt);
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(pFormatCtx);
    //Clean
    avcodec_free_context(&pCodecCtx);
    av_free(pFrame);
    av_free(picture_buf);
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
    fclose(in_file);
    return 0;
}

