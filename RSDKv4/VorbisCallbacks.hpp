#ifndef VORBISCALLBACKS_H
#define VORBISCALLBACKS_H

#include "RetroEngine.hpp"

size_t readVorbis(void *mem, size_t size, size_t nmemb, void *ptr);
int seekVorbis(void *ptr, ogg_int64_t offset, int whence);
long tellVorbis(void *ptr);
int closeVorbis(void *ptr);

#endif // !VORBISCALLBACKS_H
