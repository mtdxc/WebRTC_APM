#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "test_common.h"
#include "timing.h"

#include "aecm/echo_control_mobile.h"

int aecProcess(int16_t *far_frame, int16_t *near_frame, uint32_t sampleRate, size_t samplesCount, int16_t nMode,
               int16_t msInSndCardBuf) {
    if (near_frame == nullptr) return -1;
    if (far_frame == nullptr) return -1;
    if (samplesCount == 0) return -1;
    AecmConfig config;
    config.cngMode = AecmTrue;
    config.echoMode = nMode;// 0, 1, 2, 3 (default), 4
    size_t samples = MIN(160, sampleRate / 100);
    if (samples == 0)
        return -1;
    const int maxSamples = 160;
    int16_t *near_input = near_frame;
    int16_t *far_input = far_frame;
    size_t nCount = (samplesCount / samples);
    void *aecmInst = WebRtcAecm_Create();
    if (aecmInst == NULL) return -1;
    int status = WebRtcAecm_Init(aecmInst, sampleRate);//8000 or 16000 Sample rate
    if (status != 0) {
        printf("WebRtcAecm_Init fail\n");
        WebRtcAecm_Free(aecmInst);
        return -1;
    }
    status = WebRtcAecm_set_config(aecmInst, config);
    if (status != 0) {
        printf("WebRtcAecm_set_config fail\n");
        WebRtcAecm_Free(aecmInst);
        return -1;
    }

    int16_t out_buffer[maxSamples];
    for (size_t i = 0; i < nCount; i++) {
        if (WebRtcAecm_BufferFarend(aecmInst, far_input, samples) != 0) {
            printf("WebRtcAecm_BufferFarend() failed.");
            WebRtcAecm_Free(aecmInst);
            return -1;
        }
        int nRet = WebRtcAecm_Process(aecmInst, near_input, NULL, out_buffer, samples, msInSndCardBuf);

        if (nRet != 0) {
            printf("failed in WebRtcAecm_Process\n");
            WebRtcAecm_Free(aecmInst);
            return -1;
        }
        memcpy(near_input, out_buffer, samples * sizeof(int16_t));
        near_input += samples;
        far_input += samples;
    }
    WebRtcAecm_Free(aecmInst);
    return 1;
}

void AECM(char *near_file, char *far_file, char *out_file) {
    //音频采样率 
    uint32_t sampleRate = 0;
    uint64_t inSampleCount = 0;
    uint32_t ref_sampleRate = 0;
    uint64_t ref_inSampleCount = 0;
    unsigned int channels = 0;
    int16_t *near_frame = wavRead_int16(near_file, &sampleRate, &inSampleCount, &channels);
    int16_t *far_frame = wavRead_int16(far_file, &ref_sampleRate, &ref_inSampleCount, &channels);
    if ((near_frame == nullptr || far_frame == nullptr)) {
        if (near_frame) free(near_frame);
        if (far_frame) free(far_frame);
        return;
    }
    //如果加载成功
    int16_t echoMode = 1;// 0, 1, 2, 3 (default), 4
    int16_t msInSndCardBuf = 40;
    double startTime = now();
    aecProcess(far_frame, near_frame, sampleRate, inSampleCount, echoMode, msInSndCardBuf);
    double elapsed_time = calcElapsed(startTime, now());
    printf("time interval: %d ms\n ", (int) (elapsed_time * 1000));
    wavWrite_int16(out_file, near_frame, sampleRate, inSampleCount, channels);
    free(near_frame);
    free(far_frame);
}

int main(int argc, char *argv[]) {
    printf("WebRTC Acoustic Echo Canceller for Mobile\n");
    printf("blog:http://cpuimage.cnblogs.com/\n");
    printf("usage : aecm far_file.wav near_file.wav\n");
    if (argc < 3)
        return -1;
    // echo file
    char *far_file = argv[1];
    // mixed file
    char *near_file = argv[2];
    char drive[3];
    char dir[256];
    char fname[256];
    char ext[256];
    char out_file[1024];
    splitpath(near_file, drive, dir, fname, ext);
    sprintf(out_file, "%s%s%s_out%s", drive, dir, fname, ext);
    AECM(near_file, far_file, out_file);
    printf("press any key to exit. \n");
    getchar();
    return 0;
}
