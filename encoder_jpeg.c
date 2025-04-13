#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include <jpeglib.h>

#include "encoder_jpeg.h"

static char errbuf[256];

const char *encoder_jpeg_get_error() {
    return errbuf;
}

typedef struct {
    int width;
    int height;
    int quality;
    int stride;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;
    bool data_queued;
    uint8_t *data_buffer;
    uint64_t data_timestamp;
    encoder_jpeg_output_cb output_cb;
} encoder_jpeg_priv_t;

static void save_as_jpeg(
    unsigned int width,
    unsigned int height,
    unsigned int quality,
    unsigned int stride,
    uint8_t *in_buf,
    uint8_t **out_buf,
    unsigned long *out_size) {
    struct jpeg_error_mgr jerr;
    struct jpeg_compress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_mem_dest(&cinfo, out_buf, out_size);

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    uint8_t *row_buf = malloc(width * 3);
    row_pointer[0] = row_buf;

    uint8_t *Y = in_buf;
    uint8_t *U = Y + stride * height;
    uint8_t *V = U + (stride / 2) * (height / 2);

    while (cinfo.next_scanline < height) {
        for (unsigned int j = 0; j < width; j++) {
            int i1 = cinfo.next_scanline*stride + j;
            int i2 = cinfo.next_scanline/2*stride/2 + j/2;

            row_buf[j*3 + 0] = Y[i1];
            row_buf[j*3 + 1] = U[i2];
            row_buf[j*3 + 2] = V[i2];
        }

        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    free(row_buf);

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}

static void *thread_main(void *userdata) {
    encoder_jpeg_priv_t *encp = (encoder_jpeg_priv_t *)userdata;

    while (true) {
        pthread_mutex_lock(&encp->mutex);

        while (!encp->data_queued) {
            pthread_cond_wait(&encp->cond, &encp->mutex);
        }

        uint8_t *buffer = encp->data_buffer;
        uint64_t timestamp = encp->data_timestamp;
        encp->data_queued = false;

        pthread_cond_signal(&encp->cond);

        pthread_mutex_unlock(&encp->mutex);

        uint8_t *out_buf;
        unsigned long out_size = 0;
        save_as_jpeg(encp->width, encp->height, encp->quality, encp->stride, buffer, &out_buf, &out_size);

        encp->output_cb(out_buf, out_size, timestamp);

        free(out_buf);
    }

    return NULL;
}

bool encoder_jpeg_create(
    int width,
    int height,
    int quality,
    int stride,
    encoder_jpeg_output_cb output_cb,
    encoder_jpeg_t **enc) {
    *enc = malloc(sizeof(encoder_jpeg_priv_t));
    encoder_jpeg_priv_t *encp = (encoder_jpeg_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_jpeg_priv_t));

    encp->width = width;
    encp->height = height;
    encp->quality = quality;
    encp->stride = stride;
    pthread_mutex_init(&encp->mutex, NULL);
    pthread_cond_init(&encp->cond, NULL);
    pthread_create(&encp->thread, NULL, thread_main, encp);
    encp->output_cb = output_cb;

    return true;
}

void encoder_jpeg_encode(encoder_jpeg_t *enc, uint8_t *buffer, uint64_t timestamp) {
    encoder_jpeg_priv_t *encp = (encoder_jpeg_priv_t *)enc;

    pthread_mutex_lock(&encp->mutex);

    while (encp->data_queued) {
        pthread_cond_wait(&encp->cond, &encp->mutex);
    }

    encp->data_queued = true;
    encp->data_buffer = buffer;
    encp->data_timestamp = timestamp;

    pthread_cond_signal(&encp->cond);

    pthread_mutex_unlock(&encp->mutex);
}
