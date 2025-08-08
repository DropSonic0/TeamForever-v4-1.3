#include "RetroEngine.hpp"
#include "VorbisCallbacks.hpp"


void ProcessMusicStream(Sint32 *stream, size_t bytes_wanted)
{
    SDL_LockMutex(musicMutex);
    StreamInfo *info = &streamInfo[currentStreamIndex];

    if (!info->loaded) {
        SDL_UnlockMutex(musicMutex);
        return;
    }

    switch (musicStatus) {
        case MUSIC_READY:
        case MUSIC_PLAYING: {
#if RETRO_USING_SDL2
            if (!info->stream) {
                SDL_UnlockMutex(musicMutex);
                return;
            }
            while (musicStatus == MUSIC_PLAYING && info->stream && SDL_AudioStreamAvailable(info->stream) < bytes_wanted) {
                // We need more samples: get some
                long bytes_read = ov_read(&info->vorbisFile, (char *)info->buffer, sizeof(info->buffer), 0, 2, 1,
                                          &info->vorbBitstream);

                if (bytes_read == 0) {
                    // We've reached the end of the file
                    if (info->trackLoop) {
                        ov_pcm_seek(&info->vorbisFile, info->loopPoint);
                        continue;
                    }
                    else {
                        musicStatus = MUSIC_STOPPED;
						trackID = -1;
                        break;
                    }
                }

                if (musicStatus != MUSIC_PLAYING
                    || (info->stream && SDL_AudioStreamPut(info->stream, info->buffer, (int)bytes_read) == -1)) {
                    SDL_UnlockMutex(musicMutex);
                    return;
                }
            }

            // Now that we know there are enough samples, read them and mix them
            int bytes_done = SDL_AudioStreamGet(info->stream, info->buffer, (int)bytes_wanted);
            if (bytes_done == -1) {
                SDL_UnlockMutex(musicMutex);
                return;
            }
            if (bytes_done != 0)
                ProcessAudioMixing(stream, info->buffer, bytes_done / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME, 0);
#endif

#if RETRO_USING_SDL1
            size_t bytes_gotten = 0;
            byte *buffer        = (byte *)malloc(bytes_wanted);
            memset(buffer, 0, bytes_wanted);
            while (bytes_gotten < bytes_wanted) {
                // We need more samples: get some
                long bytes_read =
                    ov_read(&oggFilePtr->vorbisFile, (char *)oggFilePtr->buffer,
                            sizeof(oggFilePtr->buffer) > (bytes_wanted - bytes_gotten) ? (bytes_wanted - bytes_gotten) : sizeof(oggFilePtr->buffer),
                            0, 2, 1, &oggFilePtr->vorbBitstream);

                if (bytes_read == 0) {
                    // We've reached the end of the file
                    if (oggFilePtr->trackLoop) {
                        ov_pcm_seek(&oggFilePtr->vorbisFile, oggFilePtr->loopPoint);
                        continue;
                    }
                    else {
                        musicStatus = MUSIC_STOPPED;
						trackID = -1;
                        break;
                    }
                }

                if (bytes_read > 0) {
                    memcpy(buffer + bytes_gotten, oggFilePtr->buffer, bytes_read);
                    bytes_gotten += bytes_read;
                }
                else {
                    PrintLog("Music read error: vorbis error: %d", bytes_read);
                }
            }

            if (bytes_gotten > 0) {
                SDL_AudioCVT convert;
                MEM_ZERO(convert);
                int cvtResult = SDL_BuildAudioCVT(&convert, oggFilePtr->spec.format, oggFilePtr->spec.channels, oggFilePtr->spec.freq,
                                                  audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
                if (cvtResult == 0) {
                    if (convert.len_mult > 0) {
                        convert.buf = (byte *)malloc(bytes_gotten * convert.len_mult);
                        convert.len = bytes_gotten;
                        memcpy(convert.buf, buffer, bytes_gotten);
                        SDL_ConvertAudio(&convert);
                    }
                }

                if (cvtResult == 0)
                    ProcessAudioMixing(stream, (const Sint16 *)convert.buf, bytes_gotten / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME,
                                       0);

                if (convert.len > 0 && convert.buf)
                    free(convert.buf);
            }
            if (bytes_wanted > 0)
                free(buffer);
#endif

            musicPosition = ov_pcm_tell(&info->vorbisFile);
            break;
        }
        case MUSIC_STOPPED:
        case MUSIC_PAUSED:
        case MUSIC_LOADING:
            // dont play
            break;
    }
    SDL_UnlockMutex(musicMutex);
}

#if !RETRO_USE_ORIGINAL_CODE
void freeMusInfo()
{
    SDL_LockMutex(musicMutex);
    int streamID = currentStreamIndex;

#if RETRO_USING_SDL2
    if (streamInfo[streamID].loaded && streamInfo[streamID].stream) {
        SDL_FreeAudioStream(streamInfo[streamID].stream);
        streamInfo[streamID].stream = NULL;
    }
#endif

    if (streamInfo[streamID].loaded)
        ov_clear(&streamInfo[streamID].vorbisFile);

#if RETRO_USING_SDL2
    streamInfo[streamID].stream = nullptr;
#endif
    streamInfo[streamID].loaded = false;

    SDL_UnlockMutex(musicMutex);
}
#endif

void LoadMusic(void *userdata)
{
    // This function runs in a separate thread, so it needs to be careful with shared data.
    // It prepares the "back buffer" and then swaps it with the active one.

    SDL_LockMutex(musicMutex);
    int activeStream = currentStreamIndex;
    int backBuffer   = 1 - activeStream;
    SDL_UnlockMutex(musicMutex);

    // Clean up the back buffer before using it
    if (streamInfo[backBuffer].loaded) {
#if RETRO_USING_SDL2
        if (streamInfo[backBuffer].stream) {
            SDL_FreeAudioStream(streamInfo[backBuffer].stream);
            streamInfo[backBuffer].stream = NULL;
        }
#endif
        ov_clear(&streamInfo[backBuffer].vorbisFile);
        streamInfo[backBuffer].loaded = false;
    }

    FileInfo info;
    if (LoadFile(musicTracks[currentMusicTrack].fileName, &info)) {
        StreamInfo *strmInfo = &streamInfo[backBuffer];

        StreamFile *musFile = &streamFile[backBuffer];
        musFile->filePos    = 0;
        musFile->fileSize   = info.vfileSize;
        if (info.vfileSize > MUSBUFFER_SIZE)
            musFile->fileSize = MUSBUFFER_SIZE;

        FileRead(musFile->buffer, musFile->fileSize);
        CloseFile();

        unsigned long long samples = 0;
        ov_callbacks callbacks;

        callbacks.read_func  = readVorbis;
        callbacks.seek_func  = seekVorbis;
        callbacks.tell_func  = tellVorbis;
        callbacks.close_func = closeVorbis;

        int error = ov_open_callbacks(musFile, &strmInfo->vorbisFile, NULL, 0, callbacks);
        if (error == 0) {
            strmInfo->vorbBitstream = -1;
            strmInfo->vorbisFile.vi = ov_info(&strmInfo->vorbisFile, -1);

            samples = (unsigned long long)ov_pcm_total(&strmInfo->vorbisFile, -1);

#if RETRO_USING_SDL2
            strmInfo->stream = SDL_NewAudioStream(AUDIO_S16, strmInfo->vorbisFile.vi->channels, (int)strmInfo->vorbisFile.vi->rate,
                                                  audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
            if (!strmInfo->stream)
                PrintLog("Failed to create stream: %s", SDL_GetError());
#endif

#if RETRO_USING_SDL1
            playbackInfo->spec.format   = AUDIO_S16;
            playbackInfo->spec.channels = playbackInfo->vorbisFile.vi->channels;
            playbackInfo->spec.freq     = (int)playbackInfo->vorbisFile.vi->rate;
#endif

            if (musicStartPos) {
                if (streamInfo[activeStream].loaded) {
                    uint oldPos   = (uint)ov_pcm_tell(&streamInfo[activeStream].vorbisFile);
                    float newPos  = oldPos * ((float)musicRatio * 0.0001); // 8,000 == 0.8, 10,000 == 1.0 (ratio / 10,000)
                    ov_pcm_seek(&strmInfo->vorbisFile, fmod(newPos, samples));
                }
                else {
                    ov_pcm_seek(&strmInfo->vorbisFile, musicStartPos);
                }
            }
            musicStartPos = 0;

            SDL_LockMutex(musicMutex);
            musicStatus         = MUSIC_PLAYING;
            masterVolume        = MAX_VOLUME;
            trackID             = currentMusicTrack;
            strmInfo->trackLoop = musicTracks[currentMusicTrack].trackLoop;
            strmInfo->loopPoint = musicTracks[currentMusicTrack].loopPoint;
            strmInfo->loaded    = true;
            currentStreamIndex = backBuffer;
            currentMusicTrack   = -1;
            musicPosition       = 0;
            SDL_UnlockMutex(musicMutex);
        }
        else {
            musicStatus = MUSIC_STOPPED;
			trackID = -1;
            PrintLog("Failed to load vorbis! error: %d", error);
            switch (error) {
                default: PrintLog("Vorbis open error: Unknown (%d)", error); break;
                case OV_EREAD: PrintLog("Vorbis open error: A read from media returned an error"); break;
                case OV_ENOTVORBIS: PrintLog("Vorbis open error: Bitstream does not contain any Vorbis data"); break;
                case OV_EVERSION: PrintLog("Vorbis open error: Vorbis version mismatch"); break;
                case OV_EBADHEADER: PrintLog("Vorbis open error: Invalid Vorbis bitstream header"); break;
                case OV_EFAULT: PrintLog("Vorbis open error: Internal logic fault; indicates a bug or heap / stack corruption"); break;
            }
        }
    }
    else {
        musicStatus = MUSIC_STOPPED;
		trackID = -1;
    }
}

void SetMusicTrack(const char *filePath, byte trackID, bool loop, uint loopPoint)
{
    LockAudioDevice();
    TrackInfo *track = &musicTracks[trackID];
    StrCopy(track->fileName, "Data/Music/");
    StrAdd(track->fileName, filePath);
    track->trackLoop = loop;
    track->loopPoint = loopPoint;
    UnlockAudioDevice();
}

void SwapMusicTrack(const char *filePath, byte trackID, uint loopPoint, uint ratio)
{
    if (StrLength(filePath) <= 0) {
        StopMusic(true);
    }
    else {
        LockAudioDevice();
        TrackInfo *track = &musicTracks[trackID];
        StrCopy(track->fileName, "Data/Music/");
        StrAdd(track->fileName, filePath);
        track->trackLoop = true;
        track->loopPoint = loopPoint;
        musicRatio       = ratio;
        UnlockAudioDevice();
        PlayMusic(trackID, 1, true);
    }
}

bool PlayMusic(int track, int musStartPos, bool async)
{
#if RETRO_PLATFORM == RETRO_PS3
    if (musicStatus == MUSIC_PLAYING || musicStatus == MUSIC_PAUSED) {
        async = true;
    }
#endif

    if (!audioEnabled)
        return false;

    if (musicTracks[track].fileName[0]) {
        if (musicStatus != MUSIC_LOADING) {
            LockAudioDevice();
            if (track < 0 || track >= TRACK_COUNT) {
                StopMusic(true);
                currentMusicTrack = -1;
                return false;
            }
            musicStartPos     = musStartPos;
            currentMusicTrack = track;
            musicStatus       = MUSIC_LOADING;
            if (async) {
                SDL_Thread *thread = SDL_CreateThread((SDL_ThreadFunction)LoadMusic, "LoadMusic", NULL);
                if (thread) {
                    SDL_DetachThread(thread); // Detach the thread
                } else {
                    // Fallback to synchronous loading if thread creation failed
                    // Optionally, add RSDK::PrintLog(PRINT_ERROR, "Failed to create SDL_Thread for LoadMusic, loading synchronously.");
                    LoadMusic(NULL);
                }
            } else {
                LoadMusic(NULL);
            }
            return true;
        }
        else {
            PrintLog("WARNING music tried to play while music was loading!");
        }
    }

    return false;
}

void StopMusic(bool setStatus)
{
    if (setStatus) {
        musicStatus = MUSIC_STOPPED;
		trackID = -1;
	}
    musicPosition = 0;

#if !RETRO_USE_ORIGINAL_CODE
    LockAudioDevice();
    freeMusInfo();
    UnlockAudioDevice();
#endif
}

bool PauseSound()
{
    if (musicStatus == MUSIC_PLAYING) {
        musicStatus = MUSIC_PAUSED;
        return true;
    }
    return false;
}

void ResumeSound()
{
    if (musicStatus == MUSIC_PAUSED)
        musicStatus = MUSIC_PLAYING;
}
