#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/property_ids.h>
#include <libcamera/transform.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/videodev2.h>

#include "camera.h"

using libcamera::Camera;
using libcamera::CameraConfiguration;
using libcamera::CameraManager;
using libcamera::ColorSpace;
using libcamera::ControlList;
using libcamera::FrameBuffer;
using libcamera::FrameBufferAllocator;
using libcamera::Orientation;
using libcamera::PixelFormat;
using libcamera::Rectangle;
using libcamera::Request;
using libcamera::SharedFD;
using libcamera::Size;
using libcamera::Span;
using libcamera::Stream;
using libcamera::StreamConfiguration;
using libcamera::StreamRole;
using libcamera::Transform;
using libcamera::UniqueFD;

namespace controls = libcamera::controls;
namespace formats = libcamera::formats;
namespace properties = libcamera::properties;

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *camera_get_error() { return errbuf; }

static long timespec_sub(struct timespec *a, struct timespec *b) {
    return ((a->tv_sec * 1000000000L) + a->tv_nsec) -
           ((b->tv_sec * 1000000000L) + b->tv_nsec);
}

static void timespec_add(struct timespec *a, long nanosecs) {
    a->tv_nsec += nanosecs;

    while (a->tv_nsec > 1000000000L) {
        a->tv_sec++;
        a->tv_nsec -= 1000000000L;
    }
};

// https://github.com/raspberrypi/rpicam-apps/blob/6de1ab6a899df35f929b2a15c0831780bd8e750e/core/dma_heaps.cpp
static int create_dma_allocator() {
    static const char *heap_positions[] = {
        "/dev/dma_heap/vidbuf_cached",
        "/dev/dma_heap/linux,cma",
    };

    for (unsigned int i = 0; i < sizeof(heap_positions) / sizeof(const char *);
         i++) {
        int fd = open(heap_positions[i], O_RDWR | O_CLOEXEC, 0);
        if (fd >= 0) {
            return fd;
        }
    }
    return -1;
}

// https://github.com/raspberrypi/libcamera-apps/blob/dd97618a25523c2c4aa58f87af5f23e49aa6069c/core/libcamera_app.cpp#L42
static PixelFormat mode_to_pixel_format(sensor_mode_t *mode) {
    static std::vector<std::pair<std::pair<unsigned int, bool>, PixelFormat>>
        table = {
            {{8, false}, formats::SBGGR8},
            {{8, true}, formats::SBGGR8},
            {{10, false}, formats::SBGGR10},
            {{10, true}, formats::SBGGR10_CSI2P},
            {{12, false}, formats::SBGGR12},
            {{12, true}, formats::SBGGR12_CSI2P},
        };

    auto it = std::find_if(table.begin(), table.end(), [&mode](auto &m) {
        return mode->bit_depth == m.first.first &&
               mode->packed == m.first.second;
    });
    if (it != table.end()) {
        return it->second;
    }

    return formats::SBGGR12_CSI2P;
}

static int get_v4l2_colorspace(std::optional<ColorSpace> const &cs) {
    if (cs == ColorSpace::Rec709) {
        return V4L2_COLORSPACE_REC709;
    }
    return V4L2_COLORSPACE_SMPTE170M;
}

// https://github.com/raspberrypi/libcamera-apps/blob/a6267d51949d0602eedf60f3ddf8c6685f652812/core/options.cpp#L101
static void set_hdr(bool hdr) {
    bool ok = false;
    for (int i = 0; i < 4 && !ok; i++) {
        std::string dev("/dev/v4l-subdev");
        dev += (char)('0' + i);
        int fd = open(dev.c_str(), O_RDWR, 0);
        if (fd < 0)
            continue;

        v4l2_control ctrl{V4L2_CID_WIDE_DYNAMIC_RANGE, hdr};
        ok = !ioctl(fd, VIDIOC_S_CTRL, &ctrl);
        close(fd);
    }
}

// https://github.com/raspberrypi/rpicam-apps/blob/74abee8f2de519fed9cb88d39473d1dd4cf50b62/core/rpicam_app.hpp#L176
static std::vector<std::shared_ptr<Camera>>
get_cameras(const CameraManager *camera_manager) {
    std::vector<std::shared_ptr<Camera>> cameras = camera_manager->cameras();
    auto rem = std::remove_if(cameras.begin(), cameras.end(), [](auto &cam) {
        return cam->id().find("/usb") != std::string::npos;
    });
    cameras.erase(rem, cameras.end());
    std::sort(cameras.begin(), cameras.end(),
              [](auto l, auto r) { return l->id() > r->id(); });
    return cameras;
}

struct CameraPriv {
    camera_frame_cb frame_cb;
    camera_error_cb error_cb;
    long secondary_deltat;
    std::unique_ptr<CameraManager> camera_manager;
    std::shared_ptr<Camera> camera;
    Stream *video_stream;
    Stream *secondary_stream;
    std::vector<std::unique_ptr<Request>> requests;
    std::mutex ctrls_mutex;
    std::unique_ptr<ControlList> ctrls;
    std::vector<std::unique_ptr<FrameBuffer>> frame_buffers;
    std::map<FrameBuffer *, uint8_t *> mapped_buffers;
    struct timespec last_secondary_frame_time;
    bool in_error;
};

bool camera_create(const parameters_t *params, camera_frame_cb frame_cb,
                   camera_error_cb error_cb, camera_t **cam) {
    std::unique_ptr<CameraPriv> camp = std::make_unique<CameraPriv>();

    set_hdr(params->hdr);

    if (strcmp(params->log_level, "debug") == 0) {
        setenv("LIBCAMERA_LOG_LEVELS", "*:DEBUG", 1);
    } else if (strcmp(params->log_level, "info") == 0) {
        setenv("LIBCAMERA_LOG_LEVELS", "*:INFO", 1);
    } else if (strcmp(params->log_level, "warn") == 0) {
        setenv("LIBCAMERA_LOG_LEVELS", "*:WARN", 1);
    } else { // error
        setenv("LIBCAMERA_LOG_LEVELS", "*:ERROR", 1);
    }

    // We make sure to set the environment variable before libcamera init
    setenv("LIBCAMERA_RPI_TUNING_FILE", params->tuning_file, 1);

    camp->camera_manager = std::make_unique<CameraManager>();
    int ret = camp->camera_manager->start();
    if (ret != 0) {
        set_error("CameraManager.start() failed");
        return false;
    }

    std::vector<std::shared_ptr<Camera>> cameras =
        get_cameras(camp->camera_manager.get());
    if (params->camera_id >= cameras.size()) {
        set_error("selected camera is not available");
        return false;
    }

    camp->camera = camp->camera_manager->get(cameras[params->camera_id]->id());
    if (camp->camera == NULL) {
        set_error("CameraManager.get() failed");
        return false;
    }

    ret = camp->camera->acquire();
    if (ret != 0) {
        set_error("Camera.acquire() failed");
        return false;
    }

    std::vector<StreamRole> stream_roles = {StreamRole::VideoRecording};
    if (params->mode != NULL) {
        stream_roles.push_back(StreamRole::Raw);
    }
    if (params->secondary_width != 0) {
        stream_roles.push_back(StreamRole::Viewfinder);
    }

    std::unique_ptr<CameraConfiguration> conf =
        camp->camera->generateConfiguration(stream_roles);
    if (conf == NULL) {
        set_error("Camera.generateConfiguration() failed");
        return false;
    }

    int cur_stream = 0;

    StreamConfiguration &video_stream_conf = conf->at(cur_stream++);
    video_stream_conf.size = Size(params->width, params->height);
    video_stream_conf.pixelFormat = formats::YUV420;
    video_stream_conf.bufferCount = params->buffer_count;
    if (params->width >= 1280 || params->height >= 720) {
        video_stream_conf.colorSpace = ColorSpace::Rec709;
    } else {
        video_stream_conf.colorSpace = ColorSpace::Smpte170m;
    }

    if (params->mode != NULL) {
        StreamConfiguration &raw_stream_conf = conf->at(cur_stream++);
        raw_stream_conf.size = Size(params->mode->width, params->mode->height);
        raw_stream_conf.pixelFormat = mode_to_pixel_format(params->mode);
        raw_stream_conf.bufferCount = video_stream_conf.bufferCount;
    }

    if (params->secondary_width != 0) {
        StreamConfiguration &secondary_stream_conf = conf->at(cur_stream++);
        secondary_stream_conf.size =
            Size(params->secondary_width, params->secondary_height);
        secondary_stream_conf.bufferCount = video_stream_conf.bufferCount;
        secondary_stream_conf.pixelFormat = formats::YUV420;
    }

    conf->orientation = Orientation::Rotate0;
    if (params->h_flip) {
        conf->orientation = conf->orientation * Transform::HFlip;
    }
    if (params->v_flip) {
        conf->orientation = conf->orientation * Transform::VFlip;
    }

    CameraConfiguration::Status vstatus = conf->validate();
    if (vstatus == CameraConfiguration::Invalid) {
        set_error("StreamConfiguration.validate() failed");
        return false;
    }

    int res = camp->camera->configure(conf.get());
    if (res != 0) {
        set_error("Camera.configure() failed");
        return false;
    }

    camp->video_stream = video_stream_conf.stream();

    if (params->secondary_width != 0) {
        StreamConfiguration &secondary_stream_conf = conf->at(cur_stream - 1);
        camp->secondary_stream = secondary_stream_conf.stream();
    }

    for (unsigned int i = 0; i < params->buffer_count; i++) {
        std::unique_ptr<Request> request =
            camp->camera->createRequest((uint64_t)camp.get());
        if (request == NULL) {
            set_error("createRequest() failed");
            return false;
        }
        camp->requests.push_back(std::move(request));
    }

    // allocate DMA buffers manually instead of using default buffers provided
    // by libcamera. this improves performance by a lot.
    // https://forums.raspberrypi.com/viewtopic.php?t=352554
    // https://github.com/raspberrypi/rpicam-apps/blob/6de1ab6a899df35f929b2a15c0831780bd8e750e/core/rpicam_app.cpp#L1012

    int allocator_fd = create_dma_allocator();
    if (allocator_fd < 0) {
        set_error("failed to open dma heap allocator");
        return false;
    }

    for (StreamConfiguration &stream_conf : *conf) {
        Stream *stream = stream_conf.stream();

        for (unsigned int i = 0; i < params->buffer_count; i++) {
            struct dma_heap_allocation_data alloc = {};
            alloc.len = stream_conf.frameSize;
            alloc.fd_flags = O_CLOEXEC | O_RDWR;
            int ret = ioctl(allocator_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            if (ret < 0) {
                set_error("failed to allocate buffer in dma heap");
                return false;
            }
            UniqueFD fd(alloc.fd);

            std::vector<FrameBuffer::Plane> plane(1);
            plane[0].fd = SharedFD(std::move(fd));
            plane[0].offset = 0;
            plane[0].length = stream_conf.frameSize;

            camp->frame_buffers.push_back(std::make_unique<FrameBuffer>(plane));
            FrameBuffer *fb = camp->frame_buffers.back().get();

            if (stream == camp->video_stream ||
                stream == camp->secondary_stream) {
                camp->mapped_buffers[fb] = (uint8_t *)mmap(
                    NULL, stream_conf.frameSize, PROT_READ | PROT_WRITE,
                    MAP_SHARED, plane[0].fd.get(), 0);
            }

            res = camp->requests.at(i)->addBuffer(stream, fb);
            if (res != 0) {
                set_error("addBuffer() failed");
                return false;
            }
        }
    }

    close(allocator_fd);

    camp->frame_cb = frame_cb;
    camp->error_cb = error_cb;
    camp->secondary_deltat = (long)(1000000000.0 / params->secondary_fps);
    clock_gettime(CLOCK_MONOTONIC, &camp->last_secondary_frame_time);
    *cam = camp.release();

    return true;
}

static int buffer_size(const std::vector<FrameBuffer::Plane> &planes) {
    int size = 0;
    for (const FrameBuffer::Plane &plane : planes) {
        size += plane.length;
    }
    return size;
}

static void on_request_complete(Request *request) {
    CameraPriv *camp = (CameraPriv *)request->cookie();

    if (camp->in_error) {
        return;
    }

    if (request->status() == Request::RequestCancelled) {
        camp->in_error = true;
        camp->error_cb();
        return;
    }

    FrameBuffer *buffer = request->buffers().at(camp->video_stream);

    uint8_t *secondary_buffer_mapped = NULL;
    if (camp->secondary_stream != NULL) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        long diff = timespec_sub(&now, &camp->last_secondary_frame_time);

        if (diff >= camp->secondary_deltat) {
            timespec_add(&camp->last_secondary_frame_time,
                         camp->secondary_deltat);
            FrameBuffer *secondary_buffer =
                request->buffers().at(camp->secondary_stream);
            secondary_buffer_mapped = camp->mapped_buffers.at(secondary_buffer);
        }
    }

    camp->frame_cb(camp->mapped_buffers.at(buffer),
                   buffer->planes()[0].fd.get(), buffer_size(buffer->planes()),
                   buffer->metadata().timestamp / 1000,
                   secondary_buffer_mapped);

    request->reuse(Request::ReuseFlag::ReuseBuffers);

    {
        std::lock_guard<std::mutex> lock(camp->ctrls_mutex);
        request->controls() = *camp->ctrls;
        camp->ctrls->clear();
    }

    camp->camera->queueRequest(request);
}

int camera_get_stride(camera_t *cam) {
    CameraPriv *camp = (CameraPriv *)cam;
    return camp->video_stream->configuration().stride;
}

int camera_get_secondary_stride(camera_t *cam) {
    CameraPriv *camp = (CameraPriv *)cam;
    return camp->secondary_stream->configuration().stride;
}

int camera_get_colorspace(camera_t *cam) {
    CameraPriv *camp = (CameraPriv *)cam;
    return get_v4l2_colorspace(camp->video_stream->configuration().colorSpace);
}

static void fill_dynamic_controls(ControlList *ctrls,
                                  const parameters_t *params) {
    ctrls->set(controls::Brightness, params->brightness);
    ctrls->set(controls::Contrast, params->contrast);
    ctrls->set(controls::Saturation, params->saturation);
    ctrls->set(controls::Sharpness, params->sharpness);

    int exposure_mode;
    if (strcmp(params->exposure, "short") == 0) {
        exposure_mode = controls::ExposureShort;
    } else if (strcmp(params->exposure, "long") == 0) {
        exposure_mode = controls::ExposureLong;
    } else if (strcmp(params->exposure, "custom") == 0) {
        exposure_mode = controls::ExposureCustom;
    } else {
        exposure_mode = controls::ExposureNormal;
    }
    ctrls->set(controls::AeExposureMode, exposure_mode);

    if (params->flicker_period != 0) {
        ctrls->set(controls::AeFlickerMode, controls::FlickerManual);
        ctrls->set(controls::AeFlickerPeriod, params->flicker_period);
    } else {
        ctrls->set(controls::AeFlickerMode, controls::FlickerOff);
    }

    int awb_mode;
    if (strcmp(params->awb, "incandescent") == 0) {
        awb_mode = controls::AwbIncandescent;
    } else if (strcmp(params->awb, "tungsten") == 0) {
        awb_mode = controls::AwbTungsten;
    } else if (strcmp(params->awb, "fluorescent") == 0) {
        awb_mode = controls::AwbFluorescent;
    } else if (strcmp(params->awb, "indoor") == 0) {
        awb_mode = controls::AwbIndoor;
    } else if (strcmp(params->awb, "daylight") == 0) {
        awb_mode = controls::AwbDaylight;
    } else if (strcmp(params->awb, "cloudy") == 0) {
        awb_mode = controls::AwbCloudy;
    } else if (strcmp(params->awb, "custom") == 0) {
        awb_mode = controls::AwbCustom;
    } else {
        awb_mode = controls::AwbAuto;
    }
    ctrls->set(controls::AwbMode, awb_mode);

    if (params->awb_gain_red > 0 && params->awb_gain_blue > 0) {
        ctrls->set(controls::ColourGains,
                   Span<const float, 2>(
                       {params->awb_gain_red, params->awb_gain_blue}));
    }

    int denoise_mode;
    if (strcmp(params->denoise, "cdn_off") == 0) {
        denoise_mode = controls::draft::NoiseReductionModeMinimal;
    } else if (strcmp(params->denoise, "cdn_hq") == 0) {
        denoise_mode = controls::draft::NoiseReductionModeHighQuality;
    } else if (strcmp(params->denoise, "cdn_fast") == 0) {
        denoise_mode = controls::draft::NoiseReductionModeFast;
    } else {
        denoise_mode = controls::draft::NoiseReductionModeOff;
    }
    ctrls->set(controls::draft::NoiseReductionMode, denoise_mode);

    if (params->shutter != 0.f) {
        ctrls->set(controls::ExposureTimeMode,
                   controls::ExposureTimeModeManual);
        ctrls->set(controls::ExposureTime, params->shutter);
    } else {
        ctrls->set(controls::ExposureTimeMode, controls::ExposureTimeModeAuto);
    }

    int metering_mode;
    if (strcmp(params->metering, "spot") == 0) {
        metering_mode = controls::MeteringSpot;
    } else if (strcmp(params->metering, "matrix") == 0) {
        metering_mode = controls::MeteringMatrix;
    } else if (strcmp(params->metering, "custom") == 0) {
        metering_mode = controls::MeteringCustom;
    } else {
        metering_mode = controls::MeteringCentreWeighted;
    }
    ctrls->set(controls::AeMeteringMode, metering_mode);

    if (params->gain != 0.f) {
        ctrls->set(controls::AnalogueGainMode,
                   controls::AnalogueGainModeManual);
        ctrls->set(controls::AnalogueGain, params->gain);
    } else {
        ctrls->set(controls::AnalogueGainMode, controls::AnalogueGainModeAuto);
    }

    ctrls->set(controls::ExposureValue, params->ev);

    int64_t frame_time = (int64_t)(((float)1000000) / params->fps);
    ctrls->set(controls::FrameDurationLimits,
               Span<const int64_t, 2>({frame_time, frame_time}));
}

bool camera_start(camera_t *cam, parameters_t *params) {
    CameraPriv *camp = (CameraPriv *)cam;

    camp->ctrls = std::make_unique<ControlList>(controls::controls);

    fill_dynamic_controls(camp->ctrls.get(), params);

    if (camp->camera->controls().count(&controls::AfMode) > 0) {
        if (params->af_window != NULL) {
            std::optional<Rectangle> opt =
                camp->camera->properties().get(properties::ScalerCropMaximum);
            Rectangle sensor_area;
            try {
                sensor_area = opt.value();
            } catch (const std::bad_optional_access &exc) {
                set_error("get(ScalerCropMaximum) failed");
                return false;
            }

            Rectangle afwindows_rectangle[1];

            afwindows_rectangle[0] =
                Rectangle(params->af_window->x * sensor_area.width,
                          params->af_window->y * sensor_area.height,
                          params->af_window->width * sensor_area.width,
                          params->af_window->height * sensor_area.height);

            afwindows_rectangle[0].translateBy(sensor_area.topLeft());
            camp->ctrls->set(controls::AfMetering, controls::AfMeteringWindows);
            camp->ctrls->set(controls::AfWindows, afwindows_rectangle);
        }

        int af_mode;
        if (strcmp(params->af_mode, "manual") == 0) {
            af_mode = controls::AfModeManual;
        } else if (strcmp(params->af_mode, "continuous") == 0) {
            af_mode = controls::AfModeContinuous;
        } else {
            af_mode = controls::AfModeAuto;
        }
        camp->ctrls->set(controls::AfMode, af_mode);

        int af_range;
        if (strcmp(params->af_range, "macro") == 0) {
            af_range = controls::AfRangeMacro;
        } else if (strcmp(params->af_range, "full") == 0) {
            af_range = controls::AfRangeFull;
        } else {
            af_range = controls::AfRangeNormal;
        }
        camp->ctrls->set(controls::AfRange, af_range);

        int af_speed;
        if (strcmp(params->af_range, "fast") == 0) {
            af_speed = controls::AfSpeedFast;
        } else {
            af_speed = controls::AfSpeedNormal;
        }
        camp->ctrls->set(controls::AfSpeed, af_speed);

        if (strcmp(params->af_mode, "auto") == 0) {
            camp->ctrls->set(controls::AfTrigger, controls::AfTriggerStart);
        } else if (strcmp(params->af_mode, "manual") == 0) {
            camp->ctrls->set(controls::LensPosition, params->lens_position);
        }
    }

    if (params->roi != NULL) {
        std::optional<Rectangle> opt =
            camp->camera->properties().get(properties::ScalerCropMaximum);
        Rectangle sensor_area;
        try {
            sensor_area = opt.value();
        } catch (const std::bad_optional_access &exc) {
            set_error("get(ScalerCropMaximum) failed");
            return false;
        }

        Rectangle crop(params->roi->x * sensor_area.width,
                       params->roi->y * sensor_area.height,
                       params->roi->width * sensor_area.width,
                       params->roi->height * sensor_area.height);
        crop.translateBy(sensor_area.topLeft());
        camp->ctrls->set(controls::ScalerCrop, crop);
    }

    int res = camp->camera->start(camp->ctrls.get());
    if (res != 0) {
        set_error("Camera.start() failed");
        return false;
    }

    camp->ctrls->clear();

    camp->camera->requestCompleted.connect(on_request_complete);

    for (std::unique_ptr<Request> &request : camp->requests) {
        int res = camp->camera->queueRequest(request.get());
        if (res != 0) {
            set_error("Camera.queueRequest() failed");
            return false;
        }
    }

    return true;
}

void camera_reload_params(camera_t *cam, const parameters_t *params) {
    CameraPriv *camp = (CameraPriv *)cam;

    std::lock_guard<std::mutex> lock(camp->ctrls_mutex);
    fill_dynamic_controls(camp->ctrls.get(), params);
}
