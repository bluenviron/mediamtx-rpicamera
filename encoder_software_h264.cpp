#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/videodev2.h>
#include <wels/codec_api.h>

#include "encoder_software_h264.h"

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *encoder_software_h264_get_error() { return errbuf; }

typedef struct {
    const parameters_t *params;
    encoder_software_h264_output_cb output_cb;
    ISVCEncoder *encoder;
    SEncParamExt enc_params;
    SSourcePicture pic;
    SFrameBSInfo info;
    pthread_mutex_t mutex;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    pthread_t thread;
    bool data_queued;
    uint8_t *data_buffer;
    uint64_t data_timestamp;
} encoder_software_h264_priv_t;

static void encode(encoder_software_h264_priv_t *encp, uint8_t *buffer,
                   uint64_t timestamp) {
    pthread_mutex_lock(&encp->mutex);

    encp->pic.pData[0] = buffer; // Y
    encp->pic.pData[1] =
        encp->pic.pData[0] + encp->pic.iStride[0] * encp->params->height; // U
    encp->pic.pData[2] =
        encp->pic.pData[1] +
        (encp->pic.iStride[0] / 2) * (encp->params->height / 2); // V
    encp->pic.uiTimeStamp = timestamp / 1000000;

    memset(&encp->info, 0, sizeof(SFrameBSInfo));
    int res = encp->encoder->EncodeFrame(&encp->pic, &encp->info);
    if (res != 0) {
        fprintf(stderr, "EncodeFrame() failed\n");
        pthread_mutex_unlock(&encp->mutex);
        return;
    }

    pthread_mutex_unlock(&encp->mutex);

    if (encp->info.eFrameType != videoFrameTypeSkip) {
        for (int i = 0; i < encp->info.iLayerNum; i++) {
            const SLayerBSInfo *layer_info = &encp->info.sLayerInfo[i];

            uint64_t frame_size = 0;
            for (int j = 0; j < layer_info->iNalCount; j++) {
                frame_size += layer_info->pNalLengthInByte[j];
            }

            encp->output_cb(layer_info->pBsBuf, frame_size, timestamp);
        }
    }
}

static void *thread_main(void *userdata) {
    encoder_software_h264_priv_t *encp =
        (encoder_software_h264_priv_t *)userdata;

    while (true) {
        pthread_mutex_lock(&encp->queue_mutex);

        while (!encp->data_queued) {
            pthread_cond_wait(&encp->queue_cond, &encp->queue_mutex);
        }

        uint8_t *buffer = encp->data_buffer;
        uint64_t timestamp = encp->data_timestamp;
        encp->data_queued = false;

        pthread_cond_signal(&encp->queue_cond);

        pthread_mutex_unlock(&encp->queue_mutex);

        encode(encp, buffer, timestamp);
    }

    return NULL;
}

bool encoder_software_h264_create(const parameters_t *params, int stride,
                                  int colorspace,
                                  encoder_software_h264_output_cb output_cb,
                                  encoder_software_h264_t **enc) {
    *enc = malloc(sizeof(encoder_software_h264_priv_t));
    encoder_software_h264_priv_t *encp = (encoder_software_h264_priv_t *)(*enc);
    memset(encp, 0, sizeof(encoder_software_h264_priv_t));

    int videoFormat;

    int res = WelsCreateSVCEncoder(&encp->encoder);
    if (res != 0) {
        set_error("WelsCreateSVCEncoder() failed");
        goto failed;
    }

    encp->encoder->GetDefaultParams(&encp->enc_params);

    encp->enc_params.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encp->enc_params.fMaxFrameRate = params->fps;
    encp->enc_params.iPicWidth = params->width;
    encp->enc_params.iPicHeight = params->height;
    encp->enc_params.iTargetBitrate = params->bitrate;
    encp->enc_params.iMaxBitrate = (int)((double)params->bitrate * 1.2f);
    encp->enc_params.iRCMode = RC_BITRATE_MODE;
    encp->enc_params.iTemporalLayerNum = 1;
    encp->enc_params.iSpatialLayerNum = 1;
    encp->enc_params.bEnableDenoise = false;
    encp->enc_params.bEnableBackgroundDetection = false;
    encp->enc_params.bEnableAdaptiveQuant = false;
    encp->enc_params.bEnableFrameSkip = false;
    encp->enc_params.bEnableSceneChangeDetect = false;
    encp->enc_params.bEnableLongTermReference = false;
    encp->enc_params.iLtrMarkPeriod = 0;
    encp->enc_params.iComplexityMode = LOW_COMPLEXITY;
    encp->enc_params.uiIntraPeriod = params->idr_period;
    encp->enc_params.eSpsPpsIdStrategy = CONSTANT_ID;
    encp->enc_params.bPrefixNalAddingCtrl = false;
    encp->enc_params.iLoopFilterDisableIdc = 0;
    encp->enc_params.iLoopFilterAlphaC0Offset = 0;
    encp->enc_params.iLoopFilterBetaOffset = 0;
    encp->enc_params.iMultipleThreadIdc = 0;
    encp->enc_params.bUseLoadBalancing = false;

    encp->enc_params.sSpatialLayers[0].iVideoWidth = params->width;
    encp->enc_params.sSpatialLayers[0].iVideoHeight = params->height;
    encp->enc_params.sSpatialLayers[0].fFrameRate = params->fps;
    encp->enc_params.sSpatialLayers[0].iSpatialBitrate = params->bitrate;
    encp->enc_params.sSpatialLayers[0].iMaxSpatialBitrate =
        (int)((double)params->bitrate * 1.2f);
    if (strcmp(params->software_h264_profile, "high") == 0) {
        encp->enc_params.sSpatialLayers[0].uiProfileIdc = PRO_HIGH;
    } else if (strcmp(params->software_h264_profile, "main") == 0) {
        encp->enc_params.sSpatialLayers[0].uiProfileIdc = PRO_MAIN;
    } else {
        encp->enc_params.sSpatialLayers[0].uiProfileIdc = PRO_BASELINE;
    }
    if (strcmp(params->software_h264_level, "4.2") == 0) {
        encp->enc_params.sSpatialLayers[0].uiLevelIdc = LEVEL_4_2;
    } else if (strcmp(params->software_h264_level, "4.1") == 0) {
        encp->enc_params.sSpatialLayers[0].uiLevelIdc = LEVEL_4_1;
    } else {
        encp->enc_params.sSpatialLayers[0].uiLevelIdc = LEVEL_4_0;
    }
    encp->enc_params.sSpatialLayers[0].iDLayerQp = 24;
    if (colorspace == V4L2_COLORSPACE_REC709) {
        encp->enc_params.sSpatialLayers[0].uiColorMatrix = 1;
    } else { // SMPTE170M
        encp->enc_params.sSpatialLayers[0].uiColorMatrix = 6;
    }
    encp->enc_params.sSpatialLayers[0].sSliceArgument.uiSliceMode =
        SM_FIXEDSLCNUM_SLICE;

    res = encp->encoder->InitializeExt(&encp->enc_params);
    if (res != cmResultSuccess) {
        set_error("InitializeExt() failed");
        encp->encoder->Uninitialize();
        WelsDestroySVCEncoder(encp->encoder);
        goto failed;
    }

    videoFormat = videoFormatI420;
    encp->encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);

    memset(&encp->pic, 0, sizeof(SSourcePicture));
    encp->pic.iPicWidth = params->width;
    encp->pic.iPicHeight = params->height;
    encp->pic.iColorFormat = videoFormatI420;
    encp->pic.iStride[0] = stride;
    encp->pic.iStride[1] = stride >> 1;
    encp->pic.iStride[2] = stride >> 1;

    encp->params = params;
    encp->output_cb = output_cb;
    pthread_mutex_init(&encp->mutex, NULL);

    pthread_mutex_init(&encp->queue_mutex, NULL);
    pthread_cond_init(&encp->queue_cond, NULL);
    pthread_create(&encp->thread, NULL, thread_main, encp);

    return true;

failed:
    free(*enc);
    return false;
}

void encoder_software_h264_encode(encoder_software_h264_t *enc,
                                  uint8_t *buffer_mapped, int buffer_fd,
                                  size_t buffer_size, uint64_t timestamp) {
    encoder_software_h264_priv_t *encp = (encoder_software_h264_priv_t *)enc;

    pthread_mutex_lock(&encp->queue_mutex);

    while (encp->data_queued) {
        pthread_cond_wait(&encp->queue_cond, &encp->queue_mutex);
    }

    encp->data_queued = true;
    encp->data_buffer = buffer_mapped;
    encp->data_timestamp = timestamp;

    pthread_cond_signal(&encp->queue_cond);

    pthread_mutex_unlock(&encp->queue_mutex);
}

void encoder_software_h264_reload_params(encoder_software_h264_t *enc,
                                         const parameters_t *params) {
    encoder_software_h264_priv_t *encp = (encoder_software_h264_priv_t *)enc;

    pthread_mutex_lock(&encp->mutex);

    int32_t idrInterval = params->idr_period;
    encp->encoder->SetOption(ENCODER_OPTION_IDR_INTERVAL, &idrInterval);

    if (params->bitrate != encp->params->bitrate) {
        if (params->bitrate > encp->params->bitrate) {
            SBitrateInfo bitrateInfo;
            bitrateInfo.iLayer = SPATIAL_LAYER_0;
            bitrateInfo.iBitrate = (int)((double)params->bitrate * 1.2f);
            encp->encoder->SetOption(ENCODER_OPTION_MAX_BITRATE, &bitrateInfo);

            bitrateInfo.iBitrate = params->bitrate;
            encp->encoder->SetOption(ENCODER_OPTION_BITRATE, &bitrateInfo);
        } else {
            SBitrateInfo bitrateInfo;
            bitrateInfo.iLayer = SPATIAL_LAYER_0;
            bitrateInfo.iBitrate = params->bitrate;
            encp->encoder->SetOption(ENCODER_OPTION_BITRATE, &bitrateInfo);

            bitrateInfo.iBitrate = (int)((double)params->bitrate * 1.2f);
            encp->encoder->SetOption(ENCODER_OPTION_MAX_BITRATE, &bitrateInfo);
        }
    }

    encp->params = params;

    pthread_mutex_unlock(&encp->mutex);
}
