#ifndef __ENCODER_V4L_H__
#define __ENCODER_V4L_H__

#include "parameters.h"

typedef void encoder_v4l_t;

typedef void (*encoder_v4l_output_cb)(uint64_t ts, const uint8_t *buf, uint64_t size);

const char *encoder_v4l_get_error();
bool encoder_v4l_create(const parameters_t *params, int stride, int colorspace, encoder_v4l_output_cb output_cb, encoder_v4l_t **enc);
void encoder_v4l_encode(encoder_v4l_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, int64_t timestamp_us);
void encoder_v4l_reload_params(encoder_v4l_t *enc, const parameters_t *params);

#endif
