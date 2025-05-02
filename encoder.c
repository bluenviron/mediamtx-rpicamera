#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "encoder_hard_h264.h"
#include "encoder_soft_h264.h"
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

typedef void (*encode_cb)(void *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, uint64_t ts);

typedef void (*reload_params_cb)(void *enc, const parameters_t *params);

static bool supports_hardware_h264() {
    int fd = open(ENCODER_HARD_H264_DEVICE, O_RDWR, 0);
    if (fd < 0) {
        return false;
    }

    struct v4l2_capability caps = {0};
    int res = ioctl(fd, VIDIOC_QUERYCAP, &caps);
    if (res != 0) {
        close(fd);
        return false;
    }

    close(fd);

    if (strncmp("bcm2835-codec", (char *)caps.card, strlen("bcm2835-codec")) == 0) {
        return true;
    }

    return false;
}

typedef struct {
    void *implementation;
    encode_cb encode;
    reload_params_cb reload_params;
} encoder_priv_t;

bool encoder_create(const parameters_t *params, int stride, int colorspace, encoder_output_cb output_cb, encoder_t **enc) {
    *enc = malloc(sizeof(encoder_priv_t));
    encoder_priv_t *encp = (encoder_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_priv_t));

    bool hardH264;

    if (strcmp(params->codec, "hardwareH264") == 0) {
        hardH264 = true;
    } else if (strcmp(params->codec, "softwareH264") == 0) {
        hardH264 = false;
    } else { // auto
        hardH264 = supports_hardware_h264();
    }

    if (hardH264) {
        printf("using hardware H264 encoder\n");

        encoder_hard_h264_t *hard_h264;
        bool res = encoder_hard_h264_create(params, stride, colorspace, output_cb, &hard_h264);
        if (!res) {
            set_error(encoder_hard_h264_get_error());
            goto failed;
        }

        encp->implementation = hard_h264;
        encp->encode = encoder_hard_h264_encode;
        encp->reload_params = encoder_hard_h264_reload_params;
    } else {
        printf("using software H264 encoder\n");

        encoder_soft_h264_t *soft_h264;
        bool res = encoder_soft_h264_create(params, stride, colorspace, output_cb, &soft_h264);
        if (!res) {
            set_error(encoder_soft_h264_get_error());
            goto failed;
        }

        encp->implementation = soft_h264;
        encp->encode = encoder_soft_h264_encode;
        encp->reload_params = encoder_soft_h264_reload_params;
    }

    return true;

failed:
    free(*enc);
    return false;
}

void encoder_encode(encoder_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, uint64_t ts) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;
    encp->encode(encp->implementation, mapped_buffer, buffer_fd, size, ts);
}

void encoder_reload_params(encoder_t *enc, const parameters_t *params) {
    encoder_priv_t *encp = (encoder_priv_t *)enc;
    encp->reload_params(encp->implementation, params);
}
