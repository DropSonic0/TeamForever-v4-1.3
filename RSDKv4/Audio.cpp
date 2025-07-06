#include "RetroEngine.hpp"
#include <cmath>

int globalSFXCount = 0;
int stageSFXCount  = 0;

int masterVolume  = MAX_VOLUME;
int trackID       = -1;
int sfxVolume     = 40;
int bgmVolume     = 40;
bool audioEnabled = false;

bool musicEnabled = 0;
int musicStatus   = MUSIC_STOPPED;
int musicStartPos = 0;
int musicPosition = 0;
int musicRatio    = 0;
TrackInfo musicTracks[TRACK_COUNT];
SFXInfo sfxList[SFX_COUNT];
char sfxNames[SFX_COUNT][0x40];

int currentStreamIndex = 0;
StreamFile streamFile[STREAMFILE_COUNT];
StreamInfo streamInfo[STREAMFILE_COUNT];
StreamFile *streamFilePtr = NULL;
StreamInfo *streamInfoPtr = NULL;

ChannelInfo sfxChannels[CHANNEL_COUNT];

int currentMusicTrack = -1;

#if RETRO_USING_SDL1 || RETRO_USING_SDL2

#if RETRO_USING_SDL2
SDL_AudioDeviceID audioDevice;
SDL_AudioStream *ogv_stream;
#endif
SDL_AudioSpec audioDeviceFormat;

#define AUDIO_FREQUENCY (44100)
#define AUDIO_FORMAT    (AUDIO_S16SYS) /**< Signed 16-bit samples */
#define AUDIO_SAMPLES   (0x800)
#define AUDIO_CHANNELS  (2)

#define ADJUST_VOLUME(s, v) (s = (s * v) / MAX_VOLUME)
#endif

int InitAudioPlayback()
{

    StopAllSfx();

#if !RETRO_USE_ORIGINAL_CODE
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    SDL_AudioSpec want;
    want.freq     = 22050;          // Lower frequency
    want.format   = AUDIO_S16SYS;   // Keep 16-bit signed
    want.samples  = 1024;           // Smaller SDL buffer fragments
    want.channels = 1;              // MONO output
    want.callback = ProcessAudioPlayback;


#if RETRO_USING_SDL2
    if ((audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &audioDeviceFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)) > 0) {
        audioEnabled = true;
        SDL_PauseAudioDevice(audioDevice, 0);
        PrintLog("InitAudioPlayback: SDL_OpenAudioDevice successful. Actual device format - Freq: %d, Format: 0x%X, Channels: %d, Samples: %d",
                 audioDeviceFormat.freq, audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.samples);
    }
    else { // SDL_OpenAudioDevice failed
        PrintLog("InitAudioPlayback: SDL_OpenAudioDevice failed! SDL_Error: %s", SDL_GetError());
        audioEnabled = false;
    }

    if (audioEnabled) { // Or test regardless: remove 'if (audioEnabled)' temporarily if needed
        ogv_stream = SDL_NewAudioStream(AUDIO_F32SYS, 2, 48000, audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
        if (!ogv_stream) {
        } else {
        }
    } else {
    }
#endif // RETRO_USING_SDL2
// ... (rest of SDL1/SDL2 block, then !RETRO_USE_ORIGINAL_CODE block) ...
#endif // SDL1 || SDL2
#endif // !RETRO_USE_ORIGINAL_CODE

    LoadGlobalSfx();

    return true;
}

void LoadGlobalSfx()
{
    FileInfo info;
    FileInfo infoStore; // Used to store/restore GameConfig.bin read state
    char strBuffer[0x100]; // Buffer for reading strings/data to be skipped
    byte lengthByte = 0; // Used to read lengths of data segments
    int fileBuffer_i = 0; // Changed name from fileBuffer2 to avoid potential scope issues

    globalSFXCount = 0;

    if (LoadFile("Data/Game/GameConfig.bin", &info)) {
        infoStore = info; // Store the initial state of GameConfig.bin

        // Skip Game Name
        FileRead(&lengthByte, 1);
        FileSkip(lengthByte);

        // Skip Game Description
        FileRead(&lengthByte, 1);
        FileSkip(lengthByte);

        // Skip Palettes (0x60 colors, 3 bytes each)
        FileSkip(0x60 * 3);

        // Skip Object Names & Script Paths
        byte objectCount = 0;
        FileRead(&objectCount, 1);
        for (byte o_loop = 0; o_loop < objectCount; ++o_loop) { // Object Names
            FileRead(&lengthByte, 1); FileSkip(lengthByte);
        }
        for (byte s_loop = 0; s_loop < objectCount; ++s_loop) { // Script Paths
            FileRead(&lengthByte, 1); FileSkip(lengthByte);
        }

        // Skip Variables
        byte varCount = 0;
        FileRead(&varCount, 1);
        for (byte v_loop = 0; v_loop < varCount; ++v_loop) {
            FileRead(&lengthByte, 1); FileSkip(lengthByte); // Name
            FileSkip(4); // Value (int - 4 bytes)
        }

        // Read SFX
        FileRead(&lengthByte, 1); // This is the count of SFX entries
        globalSFXCount = lengthByte;

        // First loop: Read all SFX names
        for (byte s = 0; s < globalSFXCount; ++s) {
            FileRead(&lengthByte, 1);           // Length of SFX Name
            FileRead(&strBuffer, lengthByte);   // SFX Name
            strBuffer[lengthByte] = 0;
            SetSfxName(strBuffer, s); // Original call
        }

        // Second loop: Read all SFX paths and load them
        for (byte s = 0; s < globalSFXCount; ++s) {
            FileRead(&lengthByte, 1);           // Length of SFX Path
            FileRead(&strBuffer, lengthByte);   // SFX Path
            strBuffer[lengthByte] = 0;
            
            GetFileInfo(&infoStore); 
            CloseFile();             

            LoadSfx(strBuffer, s);   

            SetFileInfo(&infoStore); 
        }

        CloseFile(); 

#if RETRO_USE_MOD_LOADER
        Engine.LoadXMLSoundFX();
#endif
    } else {
    }

    for (int i = 0; i < CHANNEL_COUNT; ++i) sfxChannels[i].sfxID = -1;
}

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

#if !RETRO_USE_ORIGINAL_CODE
void ProcessMusicStream(Sint32 *stream, size_t bytes_wanted)
{
    if (!streamFilePtr || !streamInfoPtr)
        return;
    if (!streamFilePtr->fileSize)
        return;
    switch (musicStatus) {
        case MUSIC_READY:
        case MUSIC_PLAYING: {
#if RETRO_USING_SDL2
            while (musicStatus == MUSIC_PLAYING && streamInfoPtr->stream && SDL_AudioStreamAvailable(streamInfoPtr->stream) < bytes_wanted) {
                // We need more samples: get some
                long bytes_read = ov_read(&streamInfoPtr->vorbisFile, (char *)streamInfoPtr->buffer, sizeof(streamInfoPtr->buffer), 0, 2, 1,
                                          &streamInfoPtr->vorbBitstream);

                if (bytes_read == 0) {
                    // We've reached the end of the file
                    if (streamInfoPtr->trackLoop) {
                        ov_pcm_seek(&streamInfoPtr->vorbisFile, streamInfoPtr->loopPoint);
                        continue;
                    }
                    else {
                        musicStatus = MUSIC_STOPPED;
						trackID = -1;
                        break;
                    }
                }

                if (musicStatus != MUSIC_PLAYING
                    || (streamInfoPtr->stream && SDL_AudioStreamPut(streamInfoPtr->stream, streamInfoPtr->buffer, (int)bytes_read) == -1))
                    return;
            }

            // Now that we know there are enough samples, read them and mix them
            int bytes_done = SDL_AudioStreamGet(streamInfoPtr->stream, streamInfoPtr->buffer, (int)bytes_wanted);
            if (bytes_done == -1) {
                return;
            }
            if (bytes_done != 0)
                ProcessAudioMixing(stream, streamInfoPtr->buffer, bytes_done / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME, 0);
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

                // Now that we know there are enough samples, read them and mix them
                // int bytes_done = SDL_AudioStreamGet(oggFilePtr->stream, oggFilePtr->buffer, bytes_wanted);
                // if (bytes_done == -1) {
                //    return;
                //}

                if (cvtResult == 0)
                    ProcessAudioMixing(stream, (const Sint16 *)convert.buf, bytes_gotten / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME,
                                       0);

                if (convert.len > 0 && convert.buf)
                    free(convert.buf);
            }
            if (bytes_wanted > 0)
                free(buffer);
#endif

            musicPosition = ov_pcm_tell(&streamInfoPtr->vorbisFile);
            break;
        }
        case MUSIC_STOPPED:
        case MUSIC_PAUSED:
        case MUSIC_LOADING:
            // dont play
            break;
    }
}

void ProcessAudioPlayback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata; 

    static bool playback_format_printed_once = false; // CORRECTED VARIABLE NAME
    if (!playback_format_printed_once) {
        if (audioEnabled && audioDevice > 0) { 
        } else {
        }
        playback_format_printed_once = true; // CORRECTED VARIABLE NAME
    }

    if (!audioEnabled) {
        memset(stream, 0, len); 
        return;
    }

    Sint16 *output_buffer = (Sint16 *)stream;
    size_t samples_remaining = (size_t)len / sizeof(Sint16);

    while (samples_remaining != 0) { // WHILE LOOP START
        Sint32 mix_buffer[MIX_BUFFER_SAMPLES]; 
        memset(mix_buffer, 0, sizeof(mix_buffer));

        const size_t samples_to_do = (samples_remaining < MIX_BUFFER_SAMPLES) ? samples_remaining : MIX_BUFFER_SAMPLES;

        ProcessMusicStream(mix_buffer, samples_to_do * sizeof(Sint16));
        
#if RETRO_USING_SDL2 
        if (videoPlaying == 1) {
            if (ogv_stream) {
                const size_t bytes_to_do_video = samples_to_do * sizeof(Sint16);
                const THEORAPLAY_AudioPacket *packet_video;

                while ((packet_video = THEORAPLAY_getAudio(videoDecoder)) != NULL) {
                    if (SDL_AudioStreamPut(ogv_stream, packet_video->samples, packet_video->frames * sizeof(float) * 2) == -1) {
                    }
                    THEORAPLAY_freeAudio(packet_video);
                }

                Sint16 video_sfx_buffer[MIX_BUFFER_SAMPLES]; 
                int get_video = SDL_AudioStreamGet(ogv_stream, video_sfx_buffer, (int)bytes_to_do_video);
                if (get_video == -1) {
                } else if (get_video != 0) {
                    ProcessAudioMixing(mix_buffer, video_sfx_buffer, get_video / sizeof(Sint16), bgmVolume, 0);
                }
            } else {
            }
        } else if (ogv_stream && videoPlaying == 0) { 
            SDL_AudioStreamClear(ogv_stream);
        }
#endif

        for (byte i = 0; i < CHANNEL_COUNT; ++i) { // SFX FOR LOOP START
            ChannelInfo *sfx = &sfxChannels[i];

            if (sfx->sfxID < 0) { 
                continue;
            }
            if (sfx->sfxID >= SFX_COUNT) { 
                sfx->sfxID = -1; 
                continue;
            }
            if (!sfxList[sfx->sfxID].loaded || sfxList[sfx->sfxID].buffer == NULL) {
                sfx->sfxID = -1; 
                continue;
            }
            
            if (sfx->samplePtr) { 
                Sint16 channel_sfx_buffer[MIX_BUFFER_SAMPLES]; 
                memset(channel_sfx_buffer, 0, sizeof(channel_sfx_buffer));

                size_t samples_done_this_channel = 0;
                size_t current_sfx_samples_to_mix = samples_to_do; 

                while (samples_done_this_channel < current_sfx_samples_to_mix) { // INNER SFX WHILE START
                    if (sfx->sampleLength == 0) { 
                        if (sfx->loopSFX) {
                            sfx->samplePtr    = sfxList[sfx->sfxID].buffer;
                            sfx->sampleLength = sfxList[sfx->sfxID].length;
                            if (sfxList[sfx->sfxID].buffer == NULL || sfxList[sfx->sfxID].length == 0) { 
                                sfx->sfxID = -1; break;
                            }
                        } else {
                            MEM_ZEROP(sfx); 
                            sfx->sfxID = -1;
                            break; 
                        }
                    }
                    
                    size_t sampleLen_to_copy = (sfx->sampleLength < (current_sfx_samples_to_mix - samples_done_this_channel)) 
                                             ? sfx->sampleLength 
                                             : (current_sfx_samples_to_mix - samples_done_this_channel);

                    if (sfx->samplePtr) { 
                         memcpy(&channel_sfx_buffer[samples_done_this_channel], sfx->samplePtr, sampleLen_to_copy * sizeof(Sint16));
                    } else { 
                        sfx->sfxID = -1; break;
                    }

                    samples_done_this_channel += sampleLen_to_copy;
                    sfx->samplePtr += sampleLen_to_copy;
                    sfx->sampleLength -= sampleLen_to_copy;

                    if (sfx->sfxID == -1) break; 
                } // INNER SFX WHILE END

                if (sfx->sfxID != -1 && samples_done_this_channel > 0) { 
                    ProcessAudioMixing(mix_buffer, channel_sfx_buffer, (int)samples_done_this_channel, sfxVolume, sfx->pan);
                }
            }
        } // SFX FOR LOOP END

        for (size_t i_mix = 0; i_mix < samples_to_do; ++i_mix) { 
            Sint32 sample = mix_buffer[i_mix]; 
            const Sint16 max_audioval = ((1 << (16 - 1)) - 1);
            const Sint16 min_audioval = -(1 << (16 - 1));

            if (sample > max_audioval)
                *output_buffer++ = max_audioval;
            else if (sample < min_audioval)
                *output_buffer++ = min_audioval;
            else
                *output_buffer++ = (Sint16)sample;
        }
        samples_remaining -= samples_to_do;
    } // WHILE LOOP END
} // ProcessAudioPlayback FUNCTION END

#if RETRO_USING_SDL1 || RETRO_USING_SDL2
void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan)
{
    if (volume == 0)
        return;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;

    float panL = 0.0;
    float panR = 0.0;
    int i      = 0;

    if (pan < 0) {
        panR = 1.0f - abs(pan / 100.0f);
        panL = 1.0f;
    }
    else if (pan > 0) {
        panL = 1.0f - abs(pan / 100.0f);
        panR = 1.0f;
    }

    while (len--) {
        Sint32 sample = *src++;
        ADJUST_VOLUME(sample, volume);

        if (pan != 0) {
            if ((i % 2) != 0) {
                sample *= panR;
            }
            else {
                sample *= panL;
            }
        }

        *dst++ += sample;

        i++;
    }
}
#endif
#endif

void LoadMusic(void *userdata)
{
    int oldStreamID = currentStreamIndex;
    currentStreamIndex++;
    currentStreamIndex %= STREAMFILE_COUNT;

    LockAudioDevice();

    if (streamFile[currentStreamIndex].fileSize > 0)
        StopMusic(false);

    FileInfo info;
    if (LoadFile(musicTracks[currentMusicTrack].fileName, &info)) {
        StreamInfo *strmInfo = &streamInfo[currentStreamIndex];

        StreamFile *musFile = &streamFile[currentStreamIndex];
        musFile->filePos    = 0;
        musFile->fileSize   = info.vfileSize;
        if (info.vfileSize > MUSBUFFER_SIZE)
            musFile->fileSize = MUSBUFFER_SIZE;

        FileRead(streamFile[currentStreamIndex].buffer, musFile->fileSize);
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
                uint oldPos = (uint)ov_pcm_tell(&streamInfo[oldStreamID].vorbisFile);

                float newPos  = oldPos * ((float)musicRatio * 0.0001); // 8,000 == 0.8, 10,000 == 1.0 (ratio / 10,000)
                musicStartPos = fmod(newPos, samples);

                ov_pcm_seek(&strmInfo->vorbisFile, musicStartPos);
            }
            musicStartPos = 0;

            musicStatus         = MUSIC_PLAYING;
            masterVolume        = MAX_VOLUME;
            trackID             = currentMusicTrack;
            strmInfo->trackLoop = musicTracks[currentMusicTrack].trackLoop;
            strmInfo->loopPoint = musicTracks[currentMusicTrack].loopPoint;
            strmInfo->loaded    = true;
            streamFilePtr       = &streamFile[currentStreamIndex];
            streamInfoPtr       = &streamInfo[currentStreamIndex];
            currentMusicTrack   = -1;
            musicPosition       = 0;
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
    UnlockAudioDevice();
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
        PlayMusic(trackID, 1);
    }
}

bool PlayMusic(int track, int musStartPos)
{
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
            LoadMusic(NULL);
            UnlockAudioDevice();
            return true;
        }
        else {
            PrintLog("WARNING music tried to play while music was loading!");
        }
    }
    else {
        StopMusic(true);
    }

    return false;
}

void SetSfxName(const char *sfxName, int sfxID)
{
    int sfxNameID   = 0;
    int soundNameID = 0;
    while (sfxName[sfxNameID]) {
        if (sfxName[sfxNameID] != ' ')
            sfxNames[sfxID][soundNameID++] = sfxName[sfxNameID];
        ++sfxNameID;
    }
    sfxNames[sfxID][soundNameID] = 0;
    PrintLog("Set SFX (%d) name to: %s", sfxID, sfxName);
}

void LoadSfx(char *filePath, byte sfxID)
{
    if (!audioEnabled) {
        return;
    }

    FileInfo info;
    char fullPath[0x80];

    StrCopy(fullPath, "Data/SoundFX/");
    StrAdd(fullPath, filePath); // filePath is from GameConfig.bin, e.g., "Global/Jump.wav" or "Global/Laser.ogg"

    PrintLog("LoadSfx: Received filePath='%s', constructed initial fullPath='%s'", filePath, fullPath);

    if (LoadFile(fullPath, &info)) {
        PrintLog("LoadSfx: LoadFile successful for '%s'", fullPath);
        byte type = 0;
        if (StrLength(fullPath) > 3) {
            type = fullPath[StrLength(fullPath) - 3]; // Get first char of extension, e.g., 'w' or 'o'
        }
        PrintLog("LoadSfx: Detected type char: '%c' for %s", type ? type : '?', fullPath);

         if (type == 'w' || type == 'W') { // WAV file
            PrintLog("LoadSfx: Attempting to load as WAV: %s", fullPath);
            byte *sfx_buffer_wav = new byte[info.vfileSize];
            FileRead(sfx_buffer_wav, info.vfileSize);
            PrintLog("LoadSfx (WAV): FileRead %d bytes into sfx_buffer_wav for %s", info.vfileSize, fullPath);

            SDL_RWops *src = SDL_RWFromMem(sfx_buffer_wav, info.vfileSize);
            if (src == NULL) {
                PrintLog("LoadSfx (WAV): SDL_RWFromMem failed for %s", fullPath);
                delete[] sfx_buffer_wav;
                sfxList[sfxID].loaded = false; // Ensure loaded is false
            } else {
                PrintLog("LoadSfx (WAV): SDL_RWFromMem successful for %s", fullPath);
                SDL_AudioSpec wav_spec;
                Uint32 wav_length;
                Uint8 *wav_buffer;
                
                SDL_AudioSpec *wav_loaded_spec = SDL_LoadWAV_RW(src, 1, &wav_spec, &wav_buffer, &wav_length); // Changed to auto-close src

                if (wav_loaded_spec == NULL) {
                    PrintLog("LoadSfx (WAV): SDL_LoadWAV_RW failed for %s. SDL_Error: %s", fullPath, SDL_GetError());
                    delete[] sfx_buffer_wav; 
                    // src is closed by SDL_LoadWAV_RW if second param is 1
                } else {
                    PrintLog("LoadSfx (WAV): SDL_LoadWAV_RW successful for %s. Format:0x%X, Freq:%d, Channels:%d, Length: %u", fullPath, wav_spec.format, wav_spec.freq, wav_spec.channels, wav_length);
                    SDL_AudioCVT convert;
                    int build_cvt_result = SDL_BuildAudioCVT(&convert, wav_spec.format, wav_spec.channels, wav_spec.freq, 
                                                              audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
                    PrintLog("LoadSfx (WAV): SDL_BuildAudioCVT result: %d for %s. Need to convert: %s", fullPath, build_cvt_result, (build_cvt_result == 1) ? "yes" : "no/error");

                    if (build_cvt_result >= 0) { // 0 means no conversion needed, 1 means conversion needed, -1 means error
                        convert.len = wav_length;
                        convert.buf = (Uint8 *)malloc(wav_length * convert.len_mult); // len_mult might be 1 if no conversion
                        
                        if (!convert.buf) {
                            PrintLog("LoadSfx (WAV): Failed to malloc convert.buf for %s", fullPath);
                            sfxList[sfxID].loaded = false;
                            sfxList[sfxID].buffer = NULL; 
                        } else {
                            memcpy(convert.buf, wav_buffer, wav_length);
                            if (build_cvt_result == 1) { // Only call SDL_ConvertAudio if needed
                                SDL_ConvertAudio(&convert);
                                PrintLog("LoadSfx (WAV): SDL_ConvertAudio done for %s. New length: %d", fullPath, convert.len_cvt);
                            } else {
                                convert.len_cvt = convert.len; // If no conversion, len_cvt is same as len
                                PrintLog("LoadSfx (WAV): No audio conversion needed for %s.", fullPath);
                            }

                            LockAudioDevice();
                            StrCopy(sfxList[sfxID].name, filePath);
                            sfxList[sfxID].buffer = (Sint16 *)convert.buf; 
                            sfxList[sfxID].length = convert.len_cvt / sizeof(Sint16);
                            sfxList[sfxID].loaded = true;
                            UnlockAudioDevice();
                            PrintLog("LoadSfx (WAV): Successfully loaded and prepared %s. SFX length in samples: %d", fullPath, sfxList[sfxID].length);
                        }
                    } else { 
                        PrintLog("LoadSfx (WAV): SDL_BuildAudioCVT failed with code %d for %s", build_cvt_result, fullPath);
                        sfxList[sfxID].loaded = false;
                        if (convert.buf) { free(convert.buf); convert.buf = NULL; } // Should not happen if build_cvt_result < 0
                    }
                    SDL_FreeWAV(wav_buffer); 
                }
                delete[] sfx_buffer_wav; 
                // SDL_RWclose(src) is handled by SDL_LoadWAV_RW(src, 1, ...)
            }
        }
        else if (type == 'o' || type == 'O') { // OGG file
            PrintLog("LoadSfx: Attempting to load as OGG: %s", fullPath);
            OggVorbis_File vf;
            ov_callbacks callbacks = OV_CALLBACKS_DEFAULT; 
            vorbis_info *vinfo;
            Uint8 *audioBuf_ogg = NULL; 
            Uint32 audioLen_ogg = 0;  
            SDL_AudioSpec spec_ogg;   
            long samples_ogg;

            currentStreamIndex++; 
            currentStreamIndex %= STREAMFILE_COUNT;
            StreamFile *sfxFile = &streamFile[currentStreamIndex]; 
            
            sfxFile->filePos    = 0;
            sfxFile->fileSize   = info.vfileSize;
            if (info.vfileSize > MUSBUFFER_SIZE) { 
                sfxFile->fileSize = MUSBUFFER_SIZE;
            }

            FileRead(sfxFile->buffer, sfxFile->fileSize); 

            callbacks.read_func  = readVorbis; 
            callbacks.seek_func  = seekVorbis;
            callbacks.tell_func  = tellVorbis;
            callbacks.close_func = closeVorbis; 

            int ov_error = ov_open_callbacks(sfxFile, &vf, NULL, 0, callbacks);
            PrintLog("LoadSfx (OGG): ov_open_callbacks result: %d for %s", ov_error, fullPath);
            if (ov_error != 0) {
                PrintLog("LoadSfx (OGG): Failed to open vorbis callbacks for %s. Error: %d", fullPath, ov_error);
                sfxList[sfxID].loaded = false;
            } else {
                vinfo = ov_info(&vf, -1);
                PrintLog("LoadSfx (OGG): Vorbis info: %ld Hz, %d channels, %ld samples for %s", vinfo->rate, vinfo->channels, (long)ov_pcm_total(&vf, -1), fullPath);
                
                memset(&spec_ogg, 0, sizeof(SDL_AudioSpec));
                spec_ogg.format   = AUDIO_S16SYS; 
                spec_ogg.channels = vinfo->channels;
                spec_ogg.freq     = (int)vinfo->rate;
                PrintLog("LoadSfx (OGG): Original OGG Freq=%d. Device Freq=%d.", spec_ogg.freq, audioDeviceFormat.freq);
                spec_ogg.freq     = audioDeviceFormat.freq; // Force match device frequency
                PrintLog("LoadSfx (OGG): Forcing OGG Freq to %d for conversion.", spec_ogg.freq);

                samples_ogg = (long)ov_pcm_total(&vf, -1);
                // Recalculate audioLen_ogg based on potentially altered spec_ogg.freq IF ov_read uses it (it doesn't directly, uses samples)
                // However, the number of PCM samples from ov_pcm_total is fixed. The duration changes if rate is forced.
                // For buffer allocation, using ov_pcm_total is correct for decoded samples.
                audioLen_ogg = (Uint32)(samples_ogg * vinfo->channels * (SDL_AUDIO_BITSIZE(AUDIO_S16SYS) / 8)); // Use vinfo->channels and AUDIO_S16SYS
                PrintLog("LoadSfx (OGG): Calculated PCM audio length (using original OGG channels & S16 format): %u bytes for %s", audioLen_ogg, fullPath);


                audioBuf_ogg = (Uint8 *)malloc(audioLen_ogg);
                if (!audioBuf_ogg) {
                    PrintLog("LoadSfx (OGG): Failed to malloc audioBuf_ogg (%u bytes) for %s", audioLen_ogg, fullPath);
                    ov_clear(&vf);
                    sfxList[sfxID].loaded = false;
                } else {
                    Uint8 *buf_ptr_ogg = audioBuf_ogg;
                    long toRead_ogg = audioLen_ogg;
                    long read_total_ogg = 0;
                    int bitstream_dummy;

                    PrintLog("LoadSfx (OGG): Starting OGG decode loop for %s", fullPath);
                    while(read_total_ogg < audioLen_ogg) {
                        long ret = ov_read(&vf, (char*)buf_ptr_ogg, toRead_ogg > 4096 ? 4096 : toRead_ogg, 0, 2, 1, &bitstream_dummy);
                        if (ret == 0) { // End of file
                            PrintLog("LoadSfx (OGG): ov_read returned 0 (EOF) after %ld bytes for %s", read_total_ogg, fullPath);
                            break; 
                        }
                        if (ret < 0) { // Error in stream
                            PrintLog("LoadSfx (OGG): ov_read error %ld after %ld bytes for %s", ret, read_total_ogg, fullPath);
                            free(audioBuf_ogg);
                            audioBuf_ogg = NULL;
                            break;
                        }
                        read_total_ogg += ret;
                        buf_ptr_ogg += ret;
                        toRead_ogg -= ret;
                    }
                    PrintLog("LoadSfx (OGG): OGG decode loop finished. Total bytes decoded: %ld for %s", read_total_ogg, fullPath);

                    if (audioBuf_ogg) { // if decode was successful (or partially successful)
                        SDL_AudioCVT convert_ogg;
                        PrintLog("LoadSfx (OGG): Preparing for SDL_BuildAudioCVT. Src Spec: Format=0x%X, Freq=%d, Chan=%d. Dst Spec: Format=0x%X, Freq=%d, Chan=%d",
                                 spec_ogg.format, spec_ogg.freq, spec_ogg.channels,
                                 audioDeviceFormat.format, audioDeviceFormat.freq, audioDeviceFormat.channels);
                        int build_cvt_result_ogg = SDL_BuildAudioCVT(&convert_ogg, spec_ogg.format, spec_ogg.channels, spec_ogg.freq, 
                                                                      audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
                        PrintLog("LoadSfx (OGG): SDL_BuildAudioCVT result: %d for %s. Need to convert: %s", fullPath, build_cvt_result_ogg, (build_cvt_result_ogg == 1) ? "yes" : (build_cvt_result_ogg == 0 ? "no" : "error"));
                        
                        if (build_cvt_result_ogg >= 0) {
                            convert_ogg.len = read_total_ogg; // Use actual decoded length
                            PrintLog("LoadSfx (OGG): Attempting to malloc convert_ogg.buf (size: %d * %d = %d bytes) for %s", read_total_ogg, convert_ogg.len_mult, read_total_ogg * convert_ogg.len_mult, fullPath);
                            convert_ogg.buf = (Uint8*)malloc(read_total_ogg * convert_ogg.len_mult);
                            
                            if (!convert_ogg.buf) {
                                PrintLog("LoadSfx (OGG): Failed to malloc convert_ogg.buf for %s", fullPath);
                                sfxList[sfxID].loaded = false;
                            } else {
                                PrintLog("LoadSfx (OGG): Malloc successful for convert_ogg.buf. Copying %ld bytes.", read_total_ogg);
                                memcpy(convert_ogg.buf, audioBuf_ogg, read_total_ogg);
                                if (build_cvt_result_ogg == 1) { // Only convert if necessary
                                    PrintLog("LoadSfx (OGG): Calling SDL_ConvertAudio for %s", fullPath);
                                    SDL_ConvertAudio(&convert_ogg);
                                    PrintLog("LoadSfx (OGG): SDL_ConvertAudio done for %s. New length: %d", fullPath, convert_ogg.len_cvt);
                                } else {
                                    convert_ogg.len_cvt = convert_ogg.len;
                                     PrintLog("LoadSfx (OGG): No audio conversion needed for %s.", fullPath);
                                }

                                LockAudioDevice();
                                StrCopy(sfxList[sfxID].name, filePath);
                                sfxList[sfxID].buffer = (Sint16 *)convert_ogg.buf; 
                                sfxList[sfxID].length = convert_ogg.len_cvt / sizeof(Sint16);
                                sfxList[sfxID].loaded = true;
                                UnlockAudioDevice();
                                PrintLog("LoadSfx (OGG): Successfully loaded and prepared %s. SFX length in samples: %d", fullPath, sfxList[sfxID].length);
                            }
                        } else {
                            PrintLog("LoadSfx (OGG): SDL_BuildAudioCVT failed with code %d for %s", build_cvt_result_ogg, fullPath);
                            sfxList[sfxID].loaded = false;
                            // if (convert_ogg.buf) { free(convert_ogg.buf); convert_ogg.buf = NULL; } // convert_ogg.buf would not be allocated if build_cvt_result_ogg is < 0
                        }
                        free(audioBuf_ogg); 
                    } else {
                         sfxList[sfxID].loaded = false; // If audioBuf_ogg became NULL due to decode error
                         PrintLog("LoadSfx (OGG): audioBuf_ogg was NULL after decode loop for %s", fullPath);
                    }
                }
                ov_clear(&vf); 
            }
        } else {
            PrintLog("LoadSfx: Unknown or unsupported SFX type for %s (type char: '%c')", fullPath, type ? type : '?');
            sfxList[sfxID].loaded = false;
        }
        CloseFile(); 
    } else {
    }
}
void PlaySfx(int sfx, bool loop)
{
    LockAudioDevice();
    int sfxChannelID = -1;
    for (int c = 0; c < CHANNEL_COUNT; ++c) {
        if (sfxChannels[c].sfxID == sfx || sfxChannels[c].sfxID == -1) {
            sfxChannelID = c;
            break;
        }
    }

    ChannelInfo *sfxInfo  = &sfxChannels[sfxChannelID];
    sfxInfo->sfxID        = sfx;
    sfxInfo->samplePtr    = sfxList[sfx].buffer;
    sfxInfo->sampleLength = sfxList[sfx].length;
    sfxInfo->loopSFX      = loop;
    sfxInfo->pan          = 0;
    UnlockAudioDevice();
}
void SetSfxAttributes(int sfx, int loopCount, sbyte pan)
{
    LockAudioDevice();
    int sfxChannel = -1;
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx) {
            sfxChannel = i;
            break;
        }
    }
    if (sfxChannel == -1) {
        UnlockAudioDevice();
        return; // wasn't found
    }

    ChannelInfo *sfxInfo = &sfxChannels[sfxChannel];
    sfxInfo->loopSFX     = loopCount == -1 ? sfxInfo->loopSFX : loopCount;
    sfxInfo->pan         = pan;
    sfxInfo->sfxID       = sfx;
    UnlockAudioDevice();
}
