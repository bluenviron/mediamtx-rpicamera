#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>

#include <linux/videodev2.h>

#include "encoder_hard_h264.h"

#define DEVICE              "/dev/video11"
#define POLL_TIMEOUT_MS     200

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *encoder_hard_h264_get_error() {
    return errbuf;
}

typedef struct {
    const parameters_t *params;
    int fd;
    void **capture_buffers;
    int cur_buffer;
    encoder_hard_h264_output_cb output_cb;
    pthread_t output_thread;
} encoder_hard_h264_priv_t;

static void *output_thread(void *userdata) {
    encoder_hard_h264_priv_t *encp = (encoder_hard_h264_priv_t *)userdata;

    struct pollfd p = { encp->fd, POLLIN, 0 };
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};

    while (true) {
        int res = poll(&p, 1, POLL_TIMEOUT_MS);
        if (res == -1) {
            fprintf(stderr, "output_thread(): poll() failed\n");
            exit(1);
        }

        if (p.revents & POLLIN) {
            buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            buf.length = 1;
            buf.m.planes = planes;
            int res = ioctl(encp->fd, VIDIOC_DQBUF, &buf);
            if (res != 0) {
                fprintf(stderr, "output_thread(): ioctl(VIDIOC_DQBUF, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) failed\n");
                exit(1);
            }

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.length = 1;
            buf.m.planes = planes;
            res = ioctl(encp->fd, VIDIOC_DQBUF, &buf);
            if (res != 0) {
                fprintf(stderr, "output_thread(): ioctl(VIDIOC_DQBUF, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) failed\n");
                exit(1);
            }

            uint64_t ts = ((uint64_t)buf.timestamp.tv_sec * (uint64_t)1000000) + (uint64_t)buf.timestamp.tv_usec;

            const uint8_t *buf_mem = (const uint8_t *)encp->capture_buffers[buf.index];
            int buf_size = buf.m.planes[0].bytesused;
            encp->output_cb(ts, buf_mem, buf_size);

            res = ioctl(encp->fd, VIDIOC_QBUF, &buf);
            if (res != 0) {
                fprintf(stderr, "output_thread(): ioctl(VIDIOC_QBUF) failed\n");
                exit(1);
            }
        }
    }

    return NULL;
}

static bool fill_dynamic_params(int fd, const parameters_t *params) {
    struct v4l2_control ctrl = {0};
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
    ctrl.value = params->idr_period;
    int res = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
    if (res != 0) {
        set_error("unable to set IDR period");
        return false;
    }

    ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ctrl.value = params->bitrate;
    res = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
    if (res != 0) {
        set_error("unable to set bitrate");
        return false;
    }

    return true;
}

bool encoder_hard_h264_create(const parameters_t *params, int stride, int colorspace, encoder_hard_h264_output_cb output_cb, encoder_hard_h264_t **enc) {
    *enc = malloc(sizeof(encoder_hard_h264_priv_t));
    encoder_hard_h264_priv_t *encp = (encoder_hard_h264_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_hard_h264_priv_t));

    encp->fd = open(DEVICE, O_RDWR, 0);
    if (encp->fd < 0) {
        set_error("unable to open device");
        goto failed;
    }

    bool res2 = fill_dynamic_params(encp->fd, params);
    if (!res2) {
        goto failed;
    }

    struct v4l2_control ctrl = {0};
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    ctrl.value = params->profile;
    int res = ioctl(encp->fd, VIDIOC_S_CTRL, &ctrl);
    if (res != 0) {
        set_error("unable to set profile");
        goto failed;
    }

    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
    ctrl.value = params->level;
    res = ioctl(encp->fd, VIDIOC_S_CTRL, &ctrl);
    if (res != 0) {
        set_error("unable to set level");
        goto failed;
    }

    ctrl.id = V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER;
    ctrl.value = 0;
    res = ioctl(encp->fd, VIDIOC_S_CTRL, &ctrl);
    if (res != 0) {
        set_error("unable to set REPEAT_SEQ_HEADER");
        goto failed;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = params->width;
    fmt.fmt.pix_mp.height = params->height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = colorspace;
    fmt.fmt.pix_mp.num_planes = 1;
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = stride;
    res = ioctl(encp->fd, VIDIOC_S_FMT, &fmt);
    if (res != 0) {
        set_error("unable to set output format");
        goto failed;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = params->width;
    fmt.fmt.pix_mp.height = params->height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
    fmt.fmt.pix_mp.num_planes = 1;
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;
    res = ioctl(encp->fd, VIDIOC_S_FMT, &fmt);
    if (res != 0) {
        set_error("unable to set capture format");
        goto failed;
    }

    struct v4l2_streamparm parm = {0};
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    parm.parm.output.timeperframe.numerator = 1;
    parm.parm.output.timeperframe.denominator = params->fps;
    res = ioctl(encp->fd, VIDIOC_S_PARM, &parm);
    if (res != 0) {
        set_error("unable to set fps");
        goto failed;
    }

    struct v4l2_requestbuffers reqbufs = {0};
    reqbufs.count = params->buffer_count;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;
    res = ioctl(encp->fd, VIDIOC_REQBUFS, &reqbufs);
    if (res != 0) {
        set_error("unable to set output buffers");
        goto failed;
    }

    reqbufs.count = params->capture_buffer_count;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    res = ioctl(encp->fd, VIDIOC_REQBUFS, &reqbufs);
    if (res != 0) {
        set_error("unable to set capture buffers");
        goto failed;
    }

    encp->capture_buffers = malloc(sizeof(void *) * reqbufs.count);

    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer buffer = {0};

    for (unsigned int i = 0; i < reqbufs.count; i++) {
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        buffer.length = 1;
        buffer.m.planes = planes;
        int res = ioctl(encp->fd, VIDIOC_QUERYBUF, &buffer);
        if (res != 0) {
            set_error("unable to query buffer");
            goto failed;
        }

        encp->capture_buffers[i] = mmap(
            0,
            buffer.m.planes[0].length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            encp->fd,
            buffer.m.planes[0].m.mem_offset);
        if (encp->capture_buffers[i] == MAP_FAILED) {
            set_error("mmap() failed");
            goto failed;
        }

        res = ioctl(encp->fd, VIDIOC_QBUF, &buffer);
        if (res != 0) {
            set_error("ioctl(VIDIOC_QBUF) failed");
            goto failed;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    res = ioctl(encp->fd, VIDIOC_STREAMON, &type);
    if (res != 0) {
        set_error("unable to activate output stream");
        goto failed;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    res = ioctl(encp->fd, VIDIOC_STREAMON, &type);
    if (res != 0) {
        set_error("unable to activate capture stream");
    }

    encp->params = params;
    encp->cur_buffer = 0;
    encp->output_cb = output_cb;

    pthread_create(&encp->output_thread, NULL, output_thread, encp);

    return true;

failed:
    if (encp->capture_buffers != NULL) {
        free(encp->capture_buffers);
    }
    if (encp->fd >= 0) {
        close(encp->fd);
    }
    free(encp);
    return false;
}

void encoder_hard_h264_encode(encoder_hard_h264_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, uint64_t ts) {
    encoder_hard_h264_priv_t *encp = (encoder_hard_h264_priv_t *)enc;

    int index = encp->cur_buffer++;
    encp->cur_buffer %= encp->params->buffer_count;

    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.index = index;
    buf.field = V4L2_FIELD_NONE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.length = 1;
    buf.timestamp.tv_sec = ts / 1000000;
    buf.timestamp.tv_usec = ts % 1000000;
    buf.m.planes = planes;
    buf.m.planes[0].m.fd = buffer_fd;
    buf.m.planes[0].bytesused = size;
    buf.m.planes[0].length = size;
    int res = ioctl(encp->fd, VIDIOC_QBUF, &buf);
    if (res != 0) {
        fprintf(stderr, "encoder_hard_h264_encode(): ioctl(VIDIOC_QBUF) failed\n");
        // it happens when the raspberry is under pressure. do not exit.
    }
}

void encoder_hard_h264_reload_params(encoder_hard_h264_t *enc, const parameters_t *params) {
     encoder_hard_h264_priv_t *encp = (encoder_hard_h264_priv_t *)enc;
     fill_dynamic_params(encp->fd, params);
}
