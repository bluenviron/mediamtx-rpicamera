#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#include <stdbool.h>
#include <stdint.h>

#include "sensor_mode.h"
#include "window.h"

typedef struct {
    char *log_level;
    unsigned int camera_id;
    unsigned int width;
    unsigned int height;
    bool h_flip;
    bool v_flip;
    float brightness;
    float contrast;
    float saturation;
    float sharpness;
    char *exposure;
    char *awb;
    float awb_gain_red;
    float awb_gain_blue;
    char *denoise;
    unsigned int shutter;
    char *metering;
    float gain;
    float ev;
    window_t *roi;
    bool hdr;
    char *tuning_file;
    sensor_mode_t *mode;
    float fps;
    char *af_mode;
    char *af_range;
    char *af_speed;
    float lens_position;
    window_t *af_window;
    unsigned int flicker_period;
    bool text_overlay_enable;
    char *text_overlay;
    char *codec;
    unsigned int idr_period;
    unsigned int bitrate;
    char *hardware_h264_profile;
    char *hardware_h264_level;
    char *software_h264_profile;
    char *software_h264_level;
    unsigned int secondary_width;
    unsigned int secondary_height;
    float secondary_fps;
    unsigned int secondary_mjpeg_quality;

    // private
    unsigned int buffer_count;
    unsigned int capture_buffer_count;
} parameters_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *parameters_get_error();
bool parameters_unserialize(parameters_t *params, const uint8_t *buf,
                            size_t buf_size);
void parameters_destroy(parameters_t *params);

#ifdef __cplusplus
}
#endif

#endif
