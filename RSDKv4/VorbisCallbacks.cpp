#include "VorbisCallbacks.hpp"

size_t readVorbis(void *mem, size_t size, size_t nmemb, void *ptr)
{
    StreamFile *file = (StreamFile *)ptr;

    int n = size * nmemb;
    if (size * nmemb > file->fileSize - file->filePos)
        n = file->fileSize - file->filePos;

    if (n) {
        memcpy(mem, &file->buffer[file->filePos], n);
        file->filePos += n;
    }
    return n;
}
int seekVorbis(void *ptr, ogg_int64_t offset, int whence)
{
    StreamFile *file = (StreamFile *)ptr;

    switch (whence) {
        case SEEK_SET: whence = 0; break;
        case SEEK_CUR: whence = file->filePos; break;
        case SEEK_END: whence = file->fileSize; break;
        default: break;
    }
    file->filePos = whence + offset;
    return 0;
}
long tellVorbis(void *ptr)
{
    StreamFile *file = (StreamFile *)ptr;
    return file->filePos;
}
int closeVorbis(void *ptr) { return 1; }
