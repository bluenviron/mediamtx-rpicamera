#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/dma-buf.h>

#include "camera.h"
#include "encoder.h"
#include "encoder_jpeg.h"
#include "parameters.h"
#include "pipe.h"
#include "text.h"

static int pipe_out_fd;
static pthread_mutex_t pipe_out_mutex;
static parameters_t *params;
static camera_t *cam;
static text_t *text;
static encoder_t *enc;
static encoder_jpeg_t *enc_jpeg = NULL;

static void on_frame(uint8_t *buffer_mapped, int buffer_fd,
                     uint64_t buffer_size, uint64_t timestamp,
                     uint8_t *secondary_buffer_mapped) {
    // mapped DMA buffers require a DMA_BUF_IOCTL_SYNC before and after usage.
    // https://forums.raspberrypi.com/viewtopic.php?t=352554
    struct dma_buf_sync dma_sync = {0};
    dma_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    ioctl(buffer_fd, DMA_BUF_IOCTL_SYNC, &dma_sync);

    text_draw(text, buffer_mapped);

    dma_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    ioctl(buffer_fd, DMA_BUF_IOCTL_SYNC, &dma_sync);

    encoder_encode(enc, buffer_mapped, buffer_fd, buffer_size, timestamp);

    if (enc_jpeg != NULL && secondary_buffer_mapped != NULL) {
        encoder_jpeg_encode(enc_jpeg, secondary_buffer_mapped, timestamp);
    }
}

static void on_encoder_output(const uint8_t *buffer_mapped,
                              uint64_t buffer_size, uint64_t timestamp) {
    pthread_mutex_lock(&pipe_out_mutex);
    pipe_write_data(pipe_out_fd, buffer_mapped, buffer_size, timestamp);
    pthread_mutex_unlock(&pipe_out_mutex);
}

static void on_jpeg_output(const uint8_t *buf, uint64_t size, uint64_t ts) {
    pthread_mutex_lock(&pipe_out_mutex);
    pipe_write_secondary_data(pipe_out_fd, buf, size, ts);
    pthread_mutex_unlock(&pipe_out_mutex);
}

static void on_error() {
    pthread_mutex_lock(&pipe_out_mutex);
    pipe_write_error(pipe_out_fd, "camera driver exited");
    pthread_mutex_unlock(&pipe_out_mutex);
}

static bool handle_command(const uint8_t *buf, uint32_t size) {
    switch (buf[0]) {
    case 'e':
        return false;

    case 'c': {
        parameters_t *new_params;
        bool ok = parameters_unserialize(&buf[1], size - 1, &new_params);
        if (!ok) {
            printf("skipping reloading parameters since they are invalid: %s\n",
                   parameters_get_error());
            break;
        }

        camera_reload_params(cam, new_params);
        encoder_reload_params(enc, new_params);
        parameters_destroy(params);
        params = new_params;
    }
    }

    return true;
}

int main() {
    if (getenv("TEST") != NULL) {
        printf("test passed\n");
        return 0;
    }

    int pipe_in_fd = atoi(getenv("PIPE_CONF_FD"));
    pipe_out_fd = atoi(getenv("PIPE_VIDEO_FD"));

    uint8_t *buf;
    uint32_t n = pipe_read(pipe_in_fd, &buf);

    bool ok = parameters_unserialize(&buf[1], n - 1, &params);
    free(buf);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "parameters_unserialize(): %s",
                         parameters_get_error());
        return -1;
    }

    pthread_mutex_init(&pipe_out_mutex, NULL);
    pthread_mutex_lock(&pipe_out_mutex);

    ok = camera_create(params, on_frame, on_error, &cam);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "camera_create(): %s",
                         camera_get_error());
        return -1;
    }

    ok = text_create(params, camera_get_stride(cam), &text);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "text_create(): %s", text_get_error());
        return -1;
    }

    ok = encoder_create(params, camera_get_stride(cam),
                        camera_get_colorspace(cam), on_encoder_output, &enc);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "encoder_create(): %s",
                         encoder_get_error());
        return -1;
    }

    if (params->secondary_width != 0) {
        ok = encoder_jpeg_create(
            params->secondary_width, params->secondary_height,
            params->secondary_mjpeg_quality, camera_get_secondary_stride(cam),
            on_jpeg_output, &enc_jpeg);
        if (!ok) {
            pipe_write_error(pipe_out_fd, "encoder_jpeg_create(): %s",
                             encoder_jpeg_get_error());
            return -1;
        }
    }

    ok = camera_start(cam, params);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "camera_start(): %s", camera_get_error());
        return -1;
    }

    pipe_write_ready(pipe_out_fd);
    pthread_mutex_unlock(&pipe_out_mutex);

    while (true) {
        uint8_t *buf;
        uint32_t size = pipe_read(pipe_in_fd, &buf);

        bool ok = handle_command(buf, size);
        free(buf);

        if (!ok) {
            break;
        }
    }

    camera_stop(cam);
    encoder_destroy(enc);
    camera_destroy(cam);

    return 0;
}
