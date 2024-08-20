#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#include <linux/videodev2.h>
#include <x264.h>

#include "encoder_soft_h264.h"

#define PRESET      "ultrafast"
#define TUNE        "zerolatency"
#define PROFILE     "baseline"

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *encoder_soft_h264_get_error() {
    return errbuf;
}

typedef struct {
    const parameters_t *params;
    encoder_soft_h264_output_cb output_cb;
    x264_param_t x_params;
    x264_t *x_handler;
    x264_picture_t x_pic_in;
    x264_picture_t x_pic_out;
    uint64_t next_pts;
    pthread_mutex_t mutex;
} encoder_soft_h264_priv_t;

bool encoder_soft_h264_create(const parameters_t *params, int stride, int colorspace, encoder_soft_h264_output_cb output_cb, encoder_soft_h264_t **enc) {
    *enc = malloc(sizeof(encoder_soft_h264_priv_t));
    encoder_soft_h264_priv_t *encp = (encoder_soft_h264_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_soft_h264_priv_t));

    if (stride != (int)params->width) {
        set_error("unsupported stride: expected %d, got %d", params->width, stride);
        goto failed;
    }

    int res = x264_param_default_preset(&encp->x_params, PRESET, TUNE);
    if (res < 0) {
        set_error("x264_param_default_preset() failed");
        goto failed;
    }

    encp->x_params.i_width  = params->width;
    encp->x_params.i_height = params->height;
    encp->x_params.i_csp = X264_CSP_I420;
    encp->x_params.i_bitdepth = 8;
    encp->x_params.b_vfr_input = 0;
    encp->x_params.i_fps_num = params->fps;
    encp->x_params.i_fps_den = 1;
    encp->x_params.b_repeat_headers = 1;
    encp->x_params.b_intra_refresh = 0;
    encp->x_params.b_annexb = 1;
    encp->x_params.i_keyint_max = params->idr_period;
    if (colorspace == V4L2_COLORSPACE_REC709) {
        encp->x_params.vui.i_colmatrix = 1;
    } else { // SMPTE170M
        encp->x_params.vui.i_colmatrix = 6;
    }
    encp->x_params.rc.i_bitrate = params->bitrate / 1000;
    encp->x_params.rc.i_vbv_buffer_size = params->bitrate / 1000;
    encp->x_params.rc.i_vbv_max_bitrate = params->bitrate / 1000;
    encp->x_params.rc.i_rc_method = X264_RC_ABR;
    encp->x_params.i_bframe = 0;

    res = x264_param_apply_profile(&encp->x_params, PROFILE);
    if (res < 0) {
        set_error("x264_param_apply_profile() failed");
        goto failed;
    }

    encp->x_handler = x264_encoder_open(&encp->x_params);

    x264_picture_init(&encp->x_pic_in);
    encp->x_pic_in.img.i_csp = encp->x_params.i_csp;
    encp->x_pic_in.img.i_plane = 3;
    encp->x_pic_in.img.i_stride[0] = encp->x_params.i_width;
    encp->x_pic_in.img.i_stride[1] = ((encp->x_params.i_width * 256/2) >> 8) * 1;
    encp->x_pic_in.img.i_stride[2] = ((encp->x_params.i_width * 256/2) >> 8) * 1;

    x264_picture_alloc(&encp->x_pic_out, encp->x_params.i_csp, encp->x_params.i_width, encp->x_params.i_height);

    encp->params = params;
    encp->output_cb = output_cb;
    pthread_mutex_init(&encp->mutex, NULL);

    return true;

failed:
    free(*enc);
    return false;
}

void encoder_soft_h264_encode(encoder_soft_h264_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, uint64_t ts) {
    encoder_soft_h264_priv_t *encp = (encoder_soft_h264_priv_t *)enc;

    encp->x_pic_in.img.plane[0] = mapped_buffer; // Y
    encp->x_pic_in.img.plane[1] = encp->x_pic_in.img.plane[0] + encp->x_pic_in.img.i_stride[0] * encp->params->height; // U
    encp->x_pic_in.img.plane[2] = encp->x_pic_in.img.plane[1] + (encp->x_pic_in.img.i_stride[0] / 2) * (encp->params->height / 2); // V
    encp->x_pic_in.i_pts = encp->next_pts++;

    pthread_mutex_lock(&encp->mutex);

    x264_nal_t *nal;
    int nal_count;
    int frame_size = x264_encoder_encode(encp->x_handler, &nal, &nal_count, &encp->x_pic_in, &encp->x_pic_out);

    pthread_mutex_unlock(&encp->mutex);

    encp->output_cb(ts, nal->p_payload, frame_size);
}

void encoder_soft_h264_reload_params(encoder_soft_h264_t *enc, const parameters_t *params) {
    encoder_soft_h264_priv_t *encp = (encoder_soft_h264_priv_t *)enc;

    pthread_mutex_lock(&encp->mutex);

    encp->x_params.i_keyint_max = params->idr_period;
    encp->x_params.rc.i_bitrate = params->bitrate / 1000;
    encp->x_params.rc.i_vbv_buffer_size = params->bitrate / 1000;
    encp->x_params.rc.i_vbv_max_bitrate = params->bitrate / 1000;

    int res = x264_encoder_reconfig(encp->x_handler, &encp->x_params);
    if (res != 0) {
        fprintf(stderr, "x264_encoder_reconfig() failed\n");
    }

    encp->params = params;

    pthread_mutex_unlock(&encp->mutex);
}
