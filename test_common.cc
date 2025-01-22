#include "test_common.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

int16_t *wavRead_int16(char *filename, uint32_t *sampleRate, uint64_t *totalSampleCount, unsigned int* channels) {
    int16_t *buffer = drwav_open_file_and_read_pcm_frames_s16(filename, channels, sampleRate, totalSampleCount, NULL);
    if (buffer == NULL) {
        printf("读取wav文件失败.");
    }
    return buffer;
}

//写wav文件
void wavWrite_int16(char *filename, int16_t *buffer, uint32_t sampleRate, uint64_t totalSampleCount, unsigned int channels) {
    drwav wav;
    drwav_data_format format = {};
    format.container = drwav_container_riff;     // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
    format.format = DR_WAVE_FORMAT_PCM;          // <-- Any of the DR_WAVE_FORMAT_* codes.
    format.channels = channels;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 16;
    drwav_init_file_write(&wav, filename, &format, NULL);
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, totalSampleCount, buffer);
    drwav_uninit(&wav);
    if (framesWritten != totalSampleCount) {
        fprintf(stderr, "ERROR\n");
        exit(1);
    }
}

//分割路径函数
void splitpath(const char *path, char *drv, char *dir, char *name, char *ext) {
    const char *end;
    const char *p;
    const char *s;
    if (path[0] && path[1] == ':') {
        if (drv) {
            *drv++ = *path++;
            *drv++ = *path++;
            *drv = '\0';
        }
    } else if (drv)
        *drv = '\0';
    for (end = path; *end && *end != ':';)
        end++;
    for (p = end; p > path && *--p != '\\' && *p != '/';)
        if (*p == '.') {
            end = p;
            break;
        }
    if (ext)
        for (s = end; (*ext = *s++);)
            ext++;
    for (p = end; p > path;)
        if (*--p == '\\' || *p == '/') {
            p++;
            break;
        }
    if (name) {
        for (s = p; s < end;)
            *name++ = *s++;
        *name = '\0';
    }
    if (dir) {
        for (s = path; s < p;)
            *dir++ = *s++;
        *dir = '\0';
    }
}