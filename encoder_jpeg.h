#ifndef __ENCODER_JPEG_H__
#define __ENCODER_JPEG_H__

#include "parameters.h"

typedef void encoder_jpeg_t;

typedef void (*encoder_jpeg_output_cb)(const uint8_t *mapped, uint64_t size,
                                       uint64_t ts);

const char *encoder_jpeg_get_error();
bool encoder_jpeg_create(int width, int height, int quality, int stride,
                         encoder_jpeg_output_cb output_cb,
                         encoder_jpeg_t **enc);
void encoder_jpeg_encode(encoder_jpeg_t *enc, uint8_t *mapped, uint64_t ts);

#endif
