#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "encoder_v4l.h"
#include "encoder_x264.h"
#include "encoder.h"

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *encoder_get_error() {
    return errbuf;
}

typedef void (*encode_cb)(void *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, int64_t timestamp_us);

typedef void (*reload_params_cb)(void *enc, const parameters_t *params);

typedef struct {
    void *implementation;
    encode_cb encode;
    reload_params_cb reload_params;
} encoder_priv_t;

bool encoder_create(const parameters_t *params, int stride, int colorspace, encoder_output_cb output_cb, encoder_t **enc) {
    *enc = malloc(sizeof(encoder_priv_t));
    encoder_priv_t *encp = (encoder_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_priv_t));

    if (false) {
        encoder_v4l_t *v4l;
        bool res = encoder_v4l_create(params, stride, colorspace, output_cb, &v4l);
        if (!res) {
            set_error(encoder_v4l_get_error());
            goto failed;
        }

        encp->implementation = v4l;
        encp->encode = encoder_v4l_encode;
        encp->reload_params = encoder_v4l_reload_params;
    } else {
        encoder_x264_t *x264;
        bool res = encoder_x264_create(params, stride, colorspace, output_cb, &x264);
        if (!res) {
            set_error(encoder_x264_get_error());
            goto failed;
        }

        encp->implementation = x264;
        encp->encode = encoder_x264_encode;
        encp->reload_params = encoder_x264_reload_params;
    }

    return true;

failed:
    free(*enc);
    return false;
}

void encoder_encode(encoder_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, int64_t timestamp_us) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;
    encp->encode(encp->implementation, mapped_buffer, buffer_fd, size, timestamp_us);
}

void encoder_reload_params(encoder_t *enc, const parameters_t *params) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;
    encp->reload_params(encp->implementation, params);
}
