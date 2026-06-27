#ifndef __ENCODER_MJPEG_H__
#define __ENCODER_MJPEG_H__

#include "parameters.h"

typedef void encoder_mjpeg_t;

typedef void (*encoder_mjpeg_output_cb)(const uint8_t *buffer, uint64_t size,
                                        uint64_t dts, uint64_t ntp);

const char *encoder_mjpeg_get_error();
bool encoder_mjpeg_create(bool is_secondary, const parameters_t *params,
                          int stride, encoder_mjpeg_output_cb output_cb,
                          encoder_mjpeg_t **enc);
void encoder_mjpeg_encode(encoder_mjpeg_t *enc, uint8_t *buffer_mapped,
                          int buffer_fd, uint64_t dts, uint64_t ntp);
void encoder_mjpeg_reload_params(encoder_mjpeg_t *enc,
                                 const parameters_t *params);
#endif
