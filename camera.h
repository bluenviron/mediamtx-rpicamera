#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "parameters.h"

typedef void camera_t;

typedef void (*camera_frame_cb)(uint8_t *buffer_mapped, int buffer_fd,
                                uint64_t buffer_size, uint64_t timestamp,
                                uint8_t *secondary_buffer_mapped);

typedef void (*camera_error_cb)();

#ifdef __cplusplus
extern "C" {
#endif

const char *camera_get_error();
bool camera_create(const parameters_t *params, camera_frame_cb frame_cb,
                   camera_error_cb error_cb, camera_t **cam);
int camera_get_stride(camera_t *cam);
int camera_get_secondary_stride(camera_t *cam);
int camera_get_colorspace(camera_t *cam);
bool camera_start(camera_t *cam, parameters_t *params);
void camera_reload_params(camera_t *cam, const parameters_t *params);

#ifdef __cplusplus
}
#endif

#endif
