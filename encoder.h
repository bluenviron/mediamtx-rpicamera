#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "parameters.h"

typedef void encoder_t;

typedef void (*encoder_output_cb)(const uint8_t *mapped, uint64_t size, uint64_t ts);

const char *encoder_get_error();
bool encoder_create(const parameters_t *params, int stride, int colorspace, encoder_output_cb output_cb, encoder_t **enc);
void encoder_encode(encoder_t *enc, uint8_t *mapped_buffer, int buffer_fd, size_t size, uint64_t ts);
void encoder_reload_params(encoder_t *enc, const parameters_t *params);

#endif
