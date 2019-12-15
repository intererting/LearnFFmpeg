/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * API example for decoding and filtering
 * @example filtering_video.c
 */

#define _XOPEN_SOURCE 600 /* for usleep */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>

//lutyuv='u=128:v=128'
//boxblur
//        hflip
//hue='h=60:s=-3'
//crop=2/3*in_w:2/3*in_h
//        drawbox=x=100:y=100:w=100:h=100:color=pink@0.5
//drawtext=fontfile=arial.ttf:fontcolor=green:fontsize=30:text='Lei Xiaohua'
//const char *filter_descr = "drawtext=fontfile=mingliub.ttf:fontcolor=green:fontsize=30:text='yu li yang'";


//1.从左到右滚动播出Hello, this is drawtext function,any more questsion can concat su.gao(sugao_cn@163.com)!的内容，字体颜色为黄色，字体透明度100%，字体大小为36，字体FreeSerif。且将视频输出管道打上标签text；
//————————————————
//版权声明：本文为CSDN博主「smartavs」的原创文章，遵循 CC 4.0 BY-SA 版权协议，转载请附上原文出处链接及本声明。
//原文链接：https://blog.csdn.net/weixin_35804181/article/details/54931647

//将图片test.png叠加到标签为text的视频上，位置为左上角
const char *filter_descr = "drawtext=fontfile=mingliub.ttf:\
        fontsize=15: \
        fontcolor=green: \
x=0:\
y=100:\
        text='Hello, this is drawtext function' [text]; \
        movie='c\\:/Users/yuliyang/Desktop/LearnFFmpeg/cover.png',scale=128:128 [wm]; \
        [text] [wm] overlay=0:0 [out]";

//const char *filter_descr = "movie=cover.png,scale=120:120 [wm]; \
//      [in] [wm] overlay=0:0 [out]";
/* other way:
   scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads respectively
 */

static AVFormatContext *fmt_ctx;
static AVCodecContext *dec_ctx;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
static int video_stream_index = -1;
static int64_t last_pts = AV_NOPTS_VALUE;

void save_rgb_file(AVFrame *filt_frame, FILE *out_file);

static int open_input_file(const char *filename) {
    int ret;
    AVCodec *dec;

    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;

    /* create decoding context */
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    /* init the video decoder */
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }

    return 0;
}

static int init_filters(const char *filters_descr) {
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE};

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
             time_base.num, time_base.den,
             dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static void display_frame(const AVFrame *frame, AVRational time_base, FILE *fp_yuv) {
//    int x, y;
//    uint8_t *p0, *p;
//    int64_t delay;
//
//    if (frame->pts != AV_NOPTS_VALUE) {
//        if (last_pts != AV_NOPTS_VALUE) {
//            /* sleep roughly the right amount of time;
//             * usleep is in microseconds, just like AV_TIME_BASE. */
//            delay = av_rescale_q(frame->pts - last_pts,
//                                 time_base, AV_TIME_BASE_Q);
//            if (delay > 0 && delay < 1000000)
//                usleep(delay);
//        }
//        last_pts = frame->pts;
//    }

    //Y, U, V
    for (int i = 0; i < frame->height; i++) {
        fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp_yuv);
    }
    for (int i = 0; i < frame->height / 2; i++) {
        fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp_yuv);
    }
    for (int i = 0; i < frame->height / 2; i++) {
        fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp_yuv);
    }

    //添加水印图片

//    /* Trivial ASCII grayscale display. */
//    p0 = frame->data[0];
//    puts("\033c");
//    for (y = 0; y < frame->height; y++) {
//        p = p0;
//        for (x = 0; x < frame->width; x++)
//            putchar(" .-+#"[*(p++) / 52]);
//        putchar('\n');
//        p0 += frame->linesize[0];
//    }
//    fflush(stdout);
}

int main() {
    int ret;
    AVPacket packet;
    AVFrame *frame;
    AVFrame *filt_frame;

    frame = av_frame_alloc();
    filt_frame = av_frame_alloc();
    if (!frame || !filt_frame) {
        perror("Could not allocate frame");
        exit(1);
    }

    const char *in_file = "../ds.264";

    if ((ret = open_input_file(in_file)) < 0)
        goto end;
    if ((ret = init_filters(filter_descr)) < 0)
        goto end;
    FILE *fp_yuv = fopen("../encode_video.yuv", "wb+");
    FILE *fp_rgb = fopen("../encode_video.rgb", "wb+");
    int pts = 0;
    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(fmt_ctx, &packet)) < 0)
            break;

        if (packet.stream_index == video_stream_index) {
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                    goto end;
                }

                frame->pts = pts;
                pts++;

                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    break;
                }

                /* pull filtered frames from the filtergraph */
                while (1) {
                    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        goto end;
                    display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base, fp_yuv);
                    save_rgb_file(filt_frame, fp_rgb);
                    av_frame_unref(filt_frame);

                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(&packet);
    }
    end:
    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    fclose(fp_yuv);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        exit(1);
    }

    exit(0);
}

void save_rgb_file(AVFrame *filt_frame, FILE *out_file) {
    fwrite(filt_frame->data[0], 1, filt_frame->width * filt_frame->height * 3, out_file);
}
