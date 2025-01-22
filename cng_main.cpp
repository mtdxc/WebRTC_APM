#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "test_common.h"
#include "cng.h"

enum {
    kSidShortIntervalUpdate = 1,
    kSidNormalIntervalUpdate = 100,
    kSidLongIntervalUpdate = 10000
};

enum : size_t {
    kCNGNumParamsLow = 0,
    kCNGNumParamsNormal = 8,
    kCNGNumParamsHigh = WEBRTC_CNG_MAX_LPC_ORDER,
    kCNGNumParamsTooHigh = WEBRTC_CNG_MAX_LPC_ORDER + 1
};

enum {
    kNoSid,
    kForceSid
};

int main(int argc, char *argv[]) {
    printf("WebRtc Comfort Noise Generator\n");
    printf("博客:http://cpuimage.cnblogs.com/\n");
    printf("舒适噪音生成器\n");
    int sample_rate_hz = 8000;//  8000, 16000,  32000,  48000, 64000
    int16_t speech_data_[640];  // Max size of CNG internal buffers.
    const size_t num_samples_10ms = (sample_rate_hz / 100);
    rtc::Buffer sid_data;
    int quality = kCNGNumParamsNormal;
    ComfortNoiseEncoder cng_encoder(sample_rate_hz, kSidNormalIntervalUpdate,
                                    quality);
    size_t ret = cng_encoder.Encode(ArrayView<const int16_t>(
            speech_data_, num_samples_10ms),
                                    kNoSid, &sid_data);
    if (ret == 0) {
        size_t size = cng_encoder.Encode(ArrayView<const int16_t>(speech_data_, num_samples_10ms),
                                         kForceSid, &sid_data);
        if (size == (quality + 1))
            printf("done \n");
        wavWrite_int16("cng.wav", speech_data_, sample_rate_hz, num_samples_10ms, 1);
    }
    printf("按任意键退出程序 \n");
    getchar();
    return 0;
}
