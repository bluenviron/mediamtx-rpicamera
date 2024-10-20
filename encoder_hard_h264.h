#ifndef __ENCODER_HARD_H264_H__
#define __ENCODER_HARD_H264_H__

#include "parameters.h"

#define ENCODER_HARD_H264_DEVICE "/dev/video11"

typedef void encoder_hard_h264_t;

typedef void (*encoder_hard_h264_output_cb)(const uint8_t *mapped, uint64_t size, uint64_t ts);

const char *encoder_hard_h264_get_error();
bool encoder_hard_h264_create(const parameters_t *params, int stride, int colorspace, encoder_hard_h264_output_cb output_cb, encoder_hard_h264_t **enc);
void encoder_hard_h264_encode(encoder_hard_h264_t *enc, uint8_t *mapped, int fd, size_t size, uint64_t ts);
void encoder_hard_h264_reload_params(encoder_hard_h264_t *enc, const parameters_t *params);

#endif
