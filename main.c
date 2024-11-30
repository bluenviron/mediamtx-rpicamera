#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <linux/dma-buf.h>

#include "parameters.h"
#include "pipe.h"
#include "camera.h"
#include "text.h"
#include "encoder.h"

static int pipe_out_fd;
static pthread_mutex_t pipe_out_mutex;
static camera_t *cam;
static text_t *text;
static encoder_t *enc;

static void on_frame(
    uint8_t *buffer_mapped,
    int buffer_fd,
    uint64_t buffer_size,
    uint64_t timestamp) {
    // mapped DMA buffers require a DMA_BUF_IOCTL_SYNC before and after usage.
    // https://forums.raspberrypi.com/viewtopic.php?t=352554
    struct dma_buf_sync dma_sync = {0};
    dma_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    ioctl(buffer_fd, DMA_BUF_IOCTL_SYNC, &dma_sync);

    text_draw(text, buffer_mapped);

    dma_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    ioctl(buffer_fd, DMA_BUF_IOCTL_SYNC, &dma_sync);

    encoder_encode(enc, buffer_mapped, buffer_fd, buffer_size, timestamp);
}

static void on_encoder_output(const uint8_t *buffer_mapped, uint64_t buffer_size, uint64_t timestamp) {
    pthread_mutex_lock(&pipe_out_mutex);
    pipe_write_buf(pipe_out_fd, buffer_mapped, buffer_size, timestamp);
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

    case 'c':
        {
            parameters_t params;
            bool ok = parameters_unserialize(&params, &buf[1], size-1);
            if (!ok) {
                printf("skipping reloading parameters since they are invalid: %s\n", parameters_get_error());
                return true;
            }

            camera_reload_params(cam, &params);
            encoder_reload_params(enc, &params);
            parameters_destroy(&params);
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

    parameters_t params;
    bool ok = parameters_unserialize(&params, &buf[1], n-1);
    free(buf);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "parameters_unserialize(): %s", parameters_get_error());
        return -1;
    }

    pthread_mutex_init(&pipe_out_mutex, NULL);
    pthread_mutex_lock(&pipe_out_mutex);

    ok = camera_create(
        &params,
        on_frame,
        on_error,
        &cam);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "camera_create(): %s", camera_get_error());
        return -1;
    }

    ok = text_create(
        &params,
        camera_get_stride(cam),
        &text);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "text_create(): %s", text_get_error());
        return -1;
    }

    ok = encoder_create(
        &params,
        camera_get_stride(cam),
        camera_get_colorspace(cam),
        on_encoder_output,
        &enc);
    if (!ok) {
        pipe_write_error(pipe_out_fd, "encoder_create(): %s", encoder_get_error());
        return -1;
    }

    ok = camera_start(cam);
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

    return 0;
}
