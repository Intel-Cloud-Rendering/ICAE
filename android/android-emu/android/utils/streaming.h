
#ifndef STREAMING_H
#define STREAMING_H

#include "android/utils/compiler.h"

ANDROID_BEGIN_HEADER

struct RRndrStream;

struct RRndrStream *RRndr_create_audio(int channels, int freq, int fmt, const char *path);
void RRndr_delete(struct RRndrStream *stream);
int RRndr_write(struct RRndrStream *stream, void *buf, int size);

ANDROID_END_HEADER

#endif /* STREAMING_H */

