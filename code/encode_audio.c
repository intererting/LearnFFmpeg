/*
 * Copyright (c) 2001 Fabrice Bellard
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
 * audio encoding with libavcodec API example.
 *
 * @example encode_audio.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>

/* check that a given sample format is supported by the encoder */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

/* just pick the highest supported samplerate */
static int select_sample_rate(const AVCodec *codec) {
    const int *p;
    int best_samplerate = 0;

    if (!codec->supported_samplerates)
        return 44100;

    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

/* select layout with the highest channel count */
static int select_channel_layout(const AVCodec *codec) {
    const uint64_t *p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels = 0;

    if (!codec->channel_layouts)
        return AV_CH_LAYOUT_STEREO;

    p = codec->channel_layouts;
    while (*p) {
        int nb_channels = av_get_channel_layout_nb_channels(*p);

        if (nb_channels > best_nb_channels) {
            best_ch_layout = *p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

int main() {
    const AVCodec *codec;
    AVCodecContext *c = NULL;
    AVFormatContext *fmt_context;
    AVFrame *frame;
    AVPacket *pkt;
    AVStream *av_stream;
    FILE *f;

    const char *in_filename = "../origin.pcm";
    const char *filename = "../encode_aac.aac";

    fmt_context = avformat_alloc_context();
    av_stream = avformat_new_stream(fmt_context, 0);
    if (!av_stream) {
        return 1;
    }
    //Open output URL
    if (avio_open(&fmt_context->pb, filename, AVIO_FLAG_WRITE) < 0) {
        printf("Failed to open output file!\n");
        return -1;
    }
    fmt_context->oformat = av_guess_format(NULL, filename, NULL);
    codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }

    c->sample_fmt = codec->sample_fmts[0];
    /* put sample parameters */
    c->bit_rate = 64000;
    /* select other audio parameters supported by the encoder */
    c->sample_rate = select_sample_rate(codec);
    c->channels = 2;
    c->channel_layout = av_get_default_channel_layout(2);
    /* Allow the use of the experimental AAC encoder. */
    c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    /* Set the sample rate for the container. */
    av_stream->time_base.den = select_sample_rate(codec);
    av_stream->time_base.num = 1;

    /* Some container formats (like MP4) require global headers to be present.
  * Mark the encoder so that it behaves accordingly. */
    if (fmt_context->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    int error = avcodec_parameters_from_context(av_stream->codecpar, c);
    if (error < 0) {
        return 1;
    }


    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    /* packet for holding encoded output */
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        exit(1);
    }

    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        exit(1);
    }

    frame->nb_samples = c->frame_size;
    frame->format = c->sample_fmt;
    frame->channel_layout = c->channel_layout;

    int size = av_samples_get_buffer_size(NULL, c->channels, c->frame_size, c->sample_fmt, 1);
    uint8_t *frame_buf = (uint8_t *) av_malloc(size);
    avcodec_fill_audio_frame(frame, c->channels, c->sample_fmt, (const uint8_t *) frame_buf, size, 1);

    FILE *in_file = fopen(in_filename, "rb");
    int pts = 0;
    int header_ret = avformat_write_header(fmt_context, NULL);
    if (header_ret != 0) {
        return 1;
    }
    while (!feof(in_file)) {
        if (fread(frame_buf, 1, size, in_file) < 0) {
            break;
        }
        frame->data[0] = frame_buf;
        frame->pts = pts;
        pts += frame->nb_samples;
        int ret;
        /* send the frame for encoding */
        ret = avcodec_send_frame(c, frame);
        if (ret < 0) {
            fprintf(stderr, "Error sending the frame to the encoder\n");
            exit(1);
        }

        /* read all the available output packets (in general there may be any
         * number of them */
        while (ret >= 0) {
            ret = avcodec_receive_packet(c, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                fprintf(stderr, "Error encoding audio frame\n");
                exit(1);
            }
            pkt->stream_index = av_stream->index;
            av_write_frame(fmt_context, pkt);
            av_packet_unref(pkt);
        }
    }
    //Clean
    av_free(frame_buf);
    av_free(av_stream);
    av_write_trailer(fmt_context);
    fclose(f);
    fclose(in_file);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&c);
    return 0;
}
