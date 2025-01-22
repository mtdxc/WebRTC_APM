#include <stdint.h>

#ifndef MIN
#define  MIN(A, B)        ((A) < (B) ? (A) : (B))
#endif

#ifndef MAX
#define  MAX(A, B)        ((A) > (B) ? (A) : (B))
#endif

#ifdef __cplusplus
extern "C" {
#endif
int16_t *wavRead_int16(char *filename, uint32_t *sampleRate, uint64_t *totalSampleCount, unsigned int* channels);
void wavWrite_int16(char *filename, int16_t *buffer, uint32_t sampleRate, uint64_t totalSampleCount, unsigned int channels);

void splitpath(const char *path, char *drv, char *dir, char *name, char *ext);
#ifdef __cplusplus
}
#endif