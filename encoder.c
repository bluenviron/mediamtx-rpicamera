#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "encoder.h"
#include "encoder_hardware_h264.h"
#include "encoder_mjpeg.h"
#include "encoder_software_h264.h"

#define ENCODER_HARDWARE_H264 0
#define ENCODER_SOFTWARE_H264 1
#define ENCODER_MJPEG 2

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *encoder_get_error() { return errbuf; }

typedef void (*encode_cb)(void *enc, uint8_t *mapped_buffer, int buffer_fd,
                          uint64_t dts, uint64_t ntp);

typedef void (*reload_params_cb)(void *enc, const parameters_t *params);

typedef void (*destroy_cb)(void *enc);

typedef struct {
    void *implementation;
    encode_cb encode;
    reload_params_cb reload_params;
    destroy_cb destroy;
} encoder_priv_t;

bool encoder_create(bool is_secondary, const parameters_t *params,
                    int frame_size, int stride, int colorspace,
                    encoder_output_cb output_cb, encoder_t **enc) {
    *enc = malloc(sizeof(encoder_priv_t));
    encoder_priv_t *encp = (encoder_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_priv_t));

    int variant;
    const char *codec =
        (!is_secondary) ? params->codec : params->secondary_codec;

    if (codec != NULL && strcmp(codec, "hardwareH264") == 0) {
        variant = ENCODER_HARDWARE_H264;
    } else if (codec != NULL && strcmp(codec, "softwareH264") == 0) {
        variant = ENCODER_SOFTWARE_H264;
    } else {
        variant = ENCODER_MJPEG;
    }

    if (variant == ENCODER_HARDWARE_H264) {
        fprintf(stderr, "using hardware H264 encoder\n");

        encoder_hardware_h264_t *hardware_h264;
        bool res = encoder_hardware_h264_create(is_secondary, params,
                                                frame_size, stride, colorspace,
                                                output_cb, &hardware_h264);
        if (!res) {
            set_error(encoder_hardware_h264_get_error());
            goto failed;
        }

        encp->implementation = hardware_h264;
        encp->encode = encoder_hardware_h264_encode;
        encp->reload_params = encoder_hardware_h264_reload_params;
        encp->destroy = encoder_hardware_h264_destroy;

    } else if (variant == ENCODER_SOFTWARE_H264) {
        fprintf(stderr, "using software H264 encoder\n");

        encoder_software_h264_t *software_h264;
        bool res =
            encoder_software_h264_create(is_secondary, params, stride,
                                         colorspace, output_cb, &software_h264);
        if (!res) {
            set_error(encoder_software_h264_get_error());
            goto failed;
        }

        encp->implementation = software_h264;
        encp->encode = encoder_software_h264_encode;
        encp->reload_params = encoder_software_h264_reload_params;

    } else {
        fprintf(stderr, "using MJPEG encoder\n");

        encoder_mjpeg_t *mjpeg;
        bool res = encoder_mjpeg_create(is_secondary, params, stride, output_cb,
                                        &mjpeg);
        if (!res) {
            set_error(encoder_mjpeg_get_error());
            goto failed;
        }

        encp->implementation = mjpeg;
        encp->encode = encoder_mjpeg_encode;
        encp->reload_params = encoder_mjpeg_reload_params;
    }

    return true;

failed:
    free(*enc);
    return false;
}

void encoder_encode(encoder_t *enc, uint8_t *mapped_buffer, int buffer_fd,
                    uint64_t dts, uint64_t ntp) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;
    encp->encode(encp->implementation, mapped_buffer, buffer_fd, dts, ntp);
}

void encoder_reload_params(encoder_t *enc, const parameters_t *params) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;
    encp->reload_params(encp->implementation, params);
}

void encoder_destroy(encoder_t *enc) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;

    if (encp->destroy != NULL) {
        encp->destroy(encp->implementation);
    }

    free(encp);
}
