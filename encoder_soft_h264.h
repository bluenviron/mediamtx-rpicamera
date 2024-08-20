#ifndef __ENCODER_SOFT_X264_H__
#define __ENCODER_SOFT_X264_H__

#include "parameters.h"

typedef void encoder_soft_h264_t;

typedef void (*encoder_soft_h264_output_cb)(uint64_t ts, const uint8_t *buf, uint64_t size);

const char *encoder_soft_h264_get_error();
bool encoder_soft_h264_create(const parameters_t *params, int stride, int colorspace, encoder_soft_h264_output_cb output_cb, encoder_soft_h264_t **enc);
void encoder_soft_h264_encode(encoder_soft_h264_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, int64_t timestamp_us);
void encoder_soft_h264_reload_params(encoder_soft_h264_t *enc, const parameters_t *params);

#endif
