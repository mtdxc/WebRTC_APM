#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "agc.h"
#include "test_common.h"
#include "timing.h"

int agcProcess(int16_t *buffer, uint32_t sampleRate, size_t samplesCount, int16_t agcMode) {
    if (!buffer) return -1;
    if (samplesCount == 0) return -1;
    WebRtcAgcConfig agcConfig;
    agcConfig.compressionGaindB = 9; // default 9 dB
    agcConfig.limiterEnable = 1; // default kAgcTrue (on)
    agcConfig.targetLevelDbfs = 3; // default 3 (-3 dBOv)
    int minLevel = 0;
    int maxLevel = 255;
    size_t samples = MIN(160, sampleRate / 100);
    if (samples == 0) return -1;

    int16_t *input = buffer;
    size_t nTotal = (samplesCount / samples);
    void *agcInst = WebRtcAgc_Create();
    if (agcInst == NULL) return -1;
    int status = WebRtcAgc_Init(agcInst, minLevel, maxLevel, agcMode, sampleRate);
    if (status != 0) {
        printf("WebRtcAgc_Init fail\n");
        WebRtcAgc_Free(agcInst);
        return -1;
    }
    status = WebRtcAgc_set_config(agcInst, agcConfig);
    if (status != 0) {
        printf("WebRtcAgc_set_config fail\n");
        WebRtcAgc_Free(agcInst);
        return -1;
    }
    size_t num_bands = 1;
    int inMicLevel, outMicLevel = -1;
    int16_t out_buffer[320];
    int16_t *out16 = out_buffer;
    uint8_t saturationWarning = 1;                 //是否有溢出发生，增益放大以后的最大值超过了65536
    int16_t echo = 0;                                 //增益放大是否考虑回声影响
    for (int i = 0; i < nTotal; i++) {
        inMicLevel = 0;
        int nAgcRet = WebRtcAgc_Process(agcInst, (const int16_t *const *) &input, num_bands, samples,
                                        (int16_t *const *) &out16, inMicLevel, &outMicLevel, echo,
                                        &saturationWarning);

        if (nAgcRet != 0) {
            printf("failed in WebRtcAgc_Process\n");
            WebRtcAgc_Free(agcInst);
            return -1;
        }
        memcpy(input, out_buffer, samples * sizeof(int16_t));
        input += samples;
    }

    const size_t remainedSamples = samplesCount - nTotal * samples;
    if (remainedSamples > 0) {
        if (nTotal > 0) {
            input = input - samples + remainedSamples;
        }

        inMicLevel = 0;
        int nAgcRet = WebRtcAgc_Process(agcInst, (const int16_t *const *) &input, num_bands, samples,
                                        (int16_t *const *) &out16, inMicLevel, &outMicLevel, echo,
                                        &saturationWarning);

        if (nAgcRet != 0) {
            printf("failed in WebRtcAgc_Process during filtering the last chunk\n");
            WebRtcAgc_Free(agcInst);
            return -1;
        }
        memcpy(&input[samples-remainedSamples], &out_buffer[samples-remainedSamples], remainedSamples * sizeof(int16_t));
        input += samples;
    }

    WebRtcAgc_Free(agcInst);
    return 1;
}

void auto_gain(char *in_file, char *out_file) {
    uint32_t sampleRate = 0;
    uint64_t inSampleCount = 0;
    unsigned int channels = 0;
    int16_t *inBuffer = wavRead_int16(in_file, &sampleRate, &inSampleCount, &channels);
    //如果加载成功
    if (inBuffer) {
        //  kAgcModeAdaptiveAnalog  模拟音量调节
        //  kAgcModeAdaptiveDigital 自适应增益
        //  kAgcModeFixedDigital 固定增益
        double startTime = now();

        agcProcess(inBuffer, sampleRate, inSampleCount, kAgcModeAdaptiveDigital);

        double elapsed_time = calcElapsed(startTime, now());

        printf("time: %d ms\n ", (int) (elapsed_time * 1000));
        wavWrite_int16(out_file, inBuffer, sampleRate, inSampleCount, channels);
        free(inBuffer);
    }
 }

int main(int argc, char *argv[]) {
    printf("WebRTC Automatic Gain Control\n");
    printf("博客:http://cpuimage.cnblogs.com/\n");
    printf("音频自动增益\n");
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
    auto_gain(in_file, out_file);

    printf("按任意键退出程序 \n");
    getchar();
    return 0;
}
