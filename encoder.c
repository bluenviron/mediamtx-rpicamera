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
#include "encoder_software_h264.h"

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

bool encoder_create(const parameters_t *params, int frame_size, int stride,
                    int colorspace, encoder_output_cb output_cb,
                    encoder_t **enc) {
    *enc = malloc(sizeof(encoder_priv_t));
    encoder_priv_t *encp = (encoder_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_priv_t));

    bool hardH264;

    if (strcmp(params->codec, "hardwareH264") == 0) {
        hardH264 = true;
    } else {
        hardH264 = false;
    }

    if (hardH264) {
        printf("using hardware H264 encoder\n");

        encoder_hardware_h264_t *hardware_h264;
        bool res = encoder_hardware_h264_create(
            params, frame_size, stride, colorspace, output_cb, &hardware_h264);
        if (!res) {
            set_error(encoder_hardware_h264_get_error());
            goto failed;
        }

        encp->implementation = hardware_h264;
        encp->encode = encoder_hardware_h264_encode;
        encp->reload_params = encoder_hardware_h264_reload_params;
        encp->destroy = encoder_hardware_h264_destroy;
    } else {
        printf("using software H264 encoder\n");

        encoder_software_h264_t *software_h264;
        bool res = encoder_software_h264_create(params, stride, colorspace,
                                                output_cb, &software_h264);
        if (!res) {
            set_error(encoder_software_h264_get_error());
            goto failed;
        }

        encp->implementation = software_h264;
        encp->encode = encoder_software_h264_encode;
        encp->reload_params = encoder_software_h264_reload_params;
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
