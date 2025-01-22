#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "timing.h"
#include "vad.h"
#include "test_common.h"
#ifndef nullptr
#define nullptr 0
#endif

#ifndef MIN
#define  MIN(A, B)        ((A) < (B) ? (A) : (B))
#endif

#ifndef MAX
#define  MAX(A, B)        ((A) > (B) ? (A) : (B))
#endif

int vadProcess(int16_t *buffer, uint32_t sampleRate, size_t samplesCount, int16_t vad_mode, int per_ms_frames) {
    if (buffer == nullptr) return -1;
    if (samplesCount == 0) return -1;
    // kValidRates : 8000, 16000, 32000, 48000
    // 10, 20 or 30 ms frames
    per_ms_frames = MAX(MIN(30, per_ms_frames), 10);
    size_t samples = sampleRate * per_ms_frames / 1000;
    if (samples == 0) return -1;
    int16_t *input = buffer;
    size_t nCount = (samplesCount / samples);

    VadInst *vadInst = WebRtcVad_Create();
    if (vadInst == NULL) return -1;
    int status = WebRtcVad_Init(vadInst);
    if (status != 0) {
        printf("WebRtcVad_Init fail\n");
        WebRtcVad_Free(vadInst);
        return -1;
    }
    status = WebRtcVad_set_mode(vadInst, vad_mode);
    if (status != 0) {
        printf("WebRtcVad_set_mode fail\n");
        WebRtcVad_Free(vadInst);
        return -1;
    }
    printf("Activity ： \n");
    for (size_t i = 0; i < nCount; i++) {
        int nVadRet = WebRtcVad_Process(vadInst, sampleRate, input, samples);
        if (nVadRet == -1) {
            printf("failed in WebRtcVad_Process\n");
            WebRtcVad_Free(vadInst);
            return -1;
        } else {
            // output result
            printf(" %d \t", nVadRet);
        }
        input += samples;
    }
    printf("\n");
    WebRtcVad_Free(vadInst);
    return 1;
}

void vad(char *in_file) {
    uint32_t sampleRate = 0;
    uint64_t inSampleCount = 0;
    unsigned int channels = 0;
    int16_t *inBuffer = wavRead_int16(in_file, &sampleRate, &inSampleCount, &channels);
    //如果加载成功
    if (inBuffer != nullptr) {
        //    Aggressiveness mode (0, 1, 2, or 3)
        int16_t mode = 1;
        int per_ms = 30;
        double startTime = now();
        vadProcess(inBuffer, sampleRate, inSampleCount, mode, per_ms);
        double time_interval = calcElapsed(startTime, now());
        printf("time interval: %d ms\n ", (int) (time_interval * 1000));
        free(inBuffer);
    }
}

int main(int argc, char *argv[]) {
    printf("WebRTC Voice Activity Detector\n");
    printf("blog:http://cpuimage.cnblogs.com/\n");
    printf("usage : vad speech.wav\n");
    if (argc < 2)
        return -1;
    char *in_file = argv[1];
    vad(in_file);
    printf("press any key to exit. \n");
    getchar();
    return 0;
}


