#ifndef MUSIC_H
#define MUSIC_H

#include "RetroEngine.hpp"

#define TRACK_COUNT (0x10)
#define MUSBUFFER_SIZE   (0x2000000)
#define STREAMFILE_COUNT (2)

#define MIX_BUFFER_SAMPLES (256)

#if RETRO_USING_SDL1 || RETRO_USING_SDL2

#define LockAudioDevice()   SDL_LockAudio()
#define UnlockAudioDevice() SDL_UnlockAudio()

#else
#define LockAudioDevice()   ;
#define UnlockAudioDevice() ;
#endif

struct TrackInfo {
    char fileName[0x40];
    bool trackLoop;
    uint loopPoint;
};

struct StreamInfo {
    OggVorbis_File vorbisFile;
    int vorbBitstream;
#if RETRO_USING_SDL1
    SDL_AudioSpec spec;
#endif
#if RETRO_USING_SDL2
    SDL_AudioStream *stream;
#endif
    Sint16 buffer[MIX_BUFFER_SAMPLES];
    bool trackLoop;
    uint loopPoint;
    bool loaded;
};

struct StreamFile {
    byte buffer[MUSBUFFER_SIZE];
    int fileSize;
    int filePos;
};

enum MusicStatuses {
    MUSIC_STOPPED = 0,
    MUSIC_PLAYING = 1,
    MUSIC_PAUSED  = 2,
    MUSIC_LOADING = 3,
    MUSIC_READY   = 4,
};

extern int trackID;
extern bool musicEnabled;
extern int musicStatus;
extern int musicStartPos;
extern int musicPosition;
extern int musicRatio;
extern TrackInfo musicTracks[TRACK_COUNT];

extern int currentStreamIndex;
extern int currentMusicTrack;
extern SDL_mutex *musicMutex;
extern StreamFile streamFile[STREAMFILE_COUNT];
extern StreamInfo streamInfo[STREAMFILE_COUNT];

void ProcessMusicStream(Sint32 *stream, size_t bytes_wanted);
void LoadMusic(void *userdata);
void SetMusicTrack(const char *filePath, byte trackID, bool loop, uint loopPoint);
void SwapMusicTrack(const char *filePath, byte trackID, uint loopPoint, uint ratio);
bool PlayMusic(int track, int musStartPos, bool async = false);
void StopMusic(bool setStatus);
bool PauseSound();
void ResumeSound();

#if !RETRO_USE_ORIGINAL_CODE
void freeMusInfo();
#endif

#endif // !MUSIC_H
