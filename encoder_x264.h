#ifndef __ENCODER_X264_H__
#define __ENCODER_X264_H__

#include "parameters.h"

typedef void encoder_x264_t;

typedef void (*encoder_x264_output_cb)(uint64_t ts, const uint8_t *buf, uint64_t size);

const char *encoder_x264_get_error();
bool encoder_x264_create(const parameters_t *params, int stride, int colorspace, encoder_x264_output_cb output_cb, encoder_x264_t **enc);
void encoder_x264_encode(encoder_x264_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, int64_t timestamp_us);
void encoder_x264_reload_params(encoder_x264_t *enc, const parameters_t *params);

#endif
