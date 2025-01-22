#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "test_common.h"
#include "timing.h"
#include "noise_suppression.h"

enum nsLevel {
    kLow,
    kModerate,
    kHigh,
    kVeryHigh
};

int nsProcess(int16_t *buffer, uint32_t sampleRate, uint64_t samplesCount, uint32_t channels, enum nsLevel level) {
    if (buffer == NULL) return -1;
    if (samplesCount == 0) return -1;
    size_t samples = MIN(160, sampleRate / 100);
    if (samples == 0) return -1;
    uint32_t num_bands = 1;
    int16_t *input = buffer;
    size_t frames = (samplesCount / (samples * channels));
    int16_t *frameBuffer = (int16_t *) malloc(sizeof(*frameBuffer) * channels * samples);
    NsHandle **NsHandles = (NsHandle **) malloc(channels * sizeof(NsHandle *));
    if (NsHandles == NULL || frameBuffer == NULL) {
        if (NsHandles)
            free(NsHandles);
        if (frameBuffer)
            free(frameBuffer);
        fprintf(stderr, "malloc error.\n");
        return -1;
    }
    for (int i = 0; i < channels; i++) {
        NsHandles[i] = WebRtcNs_Create();
        if (NsHandles[i] != NULL) {
            int status = WebRtcNs_Init(NsHandles[i], sampleRate);
            if (status != 0) {
                fprintf(stderr, "WebRtcNs_Init fail\n");
                WebRtcNs_Free(NsHandles[i]);
                NsHandles[i] = NULL;
            } else {
                status = WebRtcNs_set_policy(NsHandles[i], level);
                if (status != 0) {
                    fprintf(stderr, "WebRtcNs_set_policy fail\n");
                    WebRtcNs_Free(NsHandles[i]);
                    NsHandles[i] = NULL;
                }
            }
        }
        if (NsHandles[i] == NULL) {
            for (int x = 0; x < i; x++) {
                if (NsHandles[x]) {
                    WebRtcNs_Free(NsHandles[x]);
                }
            }
            free(NsHandles);
            free(frameBuffer);
            return -1;
        }
    }
    for (int i = 0; i < frames; i++) {
        for (int c = 0; c < channels; c++) {
            for (int k = 0; k < samples; k++)
                frameBuffer[k] = input[k * channels + c];

            int16_t *nsIn[1] = {frameBuffer};   //ns input[band][data]
            int16_t *nsOut[1] = {frameBuffer};  //ns output[band][data]
            WebRtcNs_Analyze(NsHandles[c], nsIn[0]);
            WebRtcNs_Process(NsHandles[c], (const int16_t *const *) nsIn, num_bands, nsOut);
            for (int k = 0; k < samples; k++)
                input[k * channels + c] = frameBuffer[k];
        }
        input += samples * channels;
    }

    for (int i = 0; i < channels; i++) {
        if (NsHandles[i]) {
            WebRtcNs_Free(NsHandles[i]);
        }
    }
    free(NsHandles);
    free(frameBuffer);
    return 1;
}

void noise_suppression(char *in_file, char *out_file) {
    //音频采样率 
    uint32_t sampleRate = 0;
    uint32_t channels = 0;
    //总音频采样数 
    uint64_t inSampleCount = 0;
    int16_t *inBuffer = wavRead_int16(in_file, &sampleRate, &inSampleCount, &channels);

    //如果加载成功
    if (inBuffer) {
        double startTime = now();
        nsProcess(inBuffer, sampleRate, inSampleCount, channels, kModerate);
        double time_interval = calcElapsed(startTime, now());
        printf("time interval: %d ms\n ", (int) (time_interval * 1000));


        wavWrite_int16(out_file, inBuffer, sampleRate, inSampleCount, channels);
        free(inBuffer);
    }
}

int main(int argc, char *argv[]) {
    printf("WebRtc Noise Suppression\n");
    printf("blog:http://cpuimage.cnblogs.com/\n");
    if (argc < 2)
        return -1;
    char *in_file = argv[1];
    char drive[3];
    char dir[256];
    char fname[256];
    char ext[256];
    char out_file[1024];
    splitpath(in_file, drive, dir, fname, ext);
    sprintf(out_file, "%s%s%s_out%s", drive, dir, fname, ext);
    noise_suppression(in_file, out_file);

    printf("press any key to exit. \n");
    getchar();
    return 0;
}
