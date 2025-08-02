#include "RetroEngine.hpp"
#include "Music.hpp"
#include "SFX.hpp"
#include <cmath>

// --- Audio System ---
int masterVolume  = MAX_VOLUME;
int sfxVolume     = 40;
int bgmVolume     = 40;
bool audioEnabled = false;

// --- Music System ---
int trackID            = -1;
bool musicEnabled      = false;
int musicStatus        = MUSIC_STOPPED;
int musicStartPos      = 0;
int musicPosition      = 0;
int musicRatio         = 0;
TrackInfo musicTracks[TRACK_COUNT];
int currentStreamIndex = 0;
SDL_mutex *musicMutex  = NULL;
StreamFile streamFile[STREAMFILE_COUNT];
StreamInfo streamInfo[STREAMFILE_COUNT];
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
#define AUDIO_CHANNELS  (1)

#define ADJUST_VOLUME(s, v) (s = (s * v) / MAX_VOLUME)
#endif

int InitAudioPlayback()
{
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    StopAllSfx(); //"init"

#if !RETRO_USE_ORIGINAL_CODE
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    SDL_AudioSpec want;
    want.freq     = AUDIO_FREQUENCY;
    want.format   = AUDIO_FORMAT;
    want.samples  = AUDIO_SAMPLES;
    want.channels = AUDIO_CHANNELS;
    want.callback = ProcessAudioPlayback;

#if RETRO_USING_SDL2
    if ((audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &audioDeviceFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)) > 0) {
        audioEnabled = true;
        SDL_PauseAudioDevice(audioDevice, 0);
    }
    else {
        PrintLog("Unable to open audio device: %s", SDL_GetError());
        audioEnabled = false;
        return true; // no audio but game wont crash now
    }

    ogv_stream = SDL_NewAudioStream(AUDIO_F32SYS, 2, 48000, audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
    if (!ogv_stream) {
        PrintLog("Failed to create stream: %s", SDL_GetError());
        SDL_CloseAudioDevice(audioDevice);
        audioEnabled = false;
        return true;
    }
#elif RETRO_USING_SDL1
    if (SDL_OpenAudio(&want, &audioDeviceFormat) == 0) {
        audioEnabled = true;
        SDL_PauseAudio(0);
    }
    else {
        PrintLog("Unable to open audio device: %s", SDL_GetError());
        audioEnabled = false;
        return true; // no audio but game wont crash now
    }
#endif // !RETRO_USING_SDL1
#endif
#endif

    LoadGlobalSfx();

    musicMutex = SDL_CreateMutex();

    return true;
}

#if !RETRO_USE_ORIGINAL_CODE

void ProcessAudioPlayback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata; // Unused

    if (!audioEnabled)
        return;

    Sint16 *output_buffer = (Sint16 *)stream;

    size_t samples_remaining = (size_t)len / sizeof(Sint16);
    while (samples_remaining != 0) {
        Sint32 mix_buffer[MIX_BUFFER_SAMPLES];
        memset(mix_buffer, 0, sizeof(mix_buffer));

        const size_t samples_to_do = (samples_remaining < MIX_BUFFER_SAMPLES) ? samples_remaining : MIX_BUFFER_SAMPLES;

        // Mix music
        ProcessMusicStream(mix_buffer, samples_to_do * sizeof(Sint16));

#if RETRO_USING_SDL2
        if (videoPlaying == 1) {
            const size_t bytes_to_do = samples_to_do * sizeof(Sint16);

            const THEORAPLAY_AudioPacket *packet;

            while ((packet = THEORAPLAY_getAudio(videoDecoder)) != NULL) {
                SDL_AudioStreamPut(ogv_stream, packet->samples, packet->frames * sizeof(float) * 2);
                THEORAPLAY_freeAudio(packet);
            }

            Sint16 buffer[MIX_BUFFER_SAMPLES];

            if (SDL_AudioStreamAvailable(ogv_stream) < bytes_to_do) {
                SDL_AudioStreamFlush(ogv_stream);
            }

            int get = SDL_AudioStreamGet(ogv_stream, buffer, (int)bytes_to_do);

            if (get != -1) {
                ProcessAudioMixing(mix_buffer, buffer, get / sizeof(Sint16), bgmVolume, 0);
            }
        } else {
            SDL_AudioStreamClear(ogv_stream);
        }
#endif

#if RETRO_USING_SDL1
        // Process music being played by a video
        // TODO: SDL1.2 lacks SDL_AudioStream so until someone finds good way to replicate that, I'm gonna leave this commented out
        /*if (videoPlaying) {
            // Fetch THEORAPLAY audio packets
            const size_t bytes_to_do = samples_to_do * sizeof(Sint16);
            size_t bytes_done        = 0;

            byte *vid_buffer             = (byte *)malloc(bytes_to_do);
            memset(vid_buffer, 0, bytes_to_do);

            const THEORAPLAY_AudioPacket *packet;

            while ((packet = THEORAPLAY_getAudio(videoDecoder)) != NULL) {
                int data_size = packet->frames * sizeof(float) * 2;
                if (bytes_done < bytes_to_do) {
                    memcpy(vid_buffer + bytes_done, packet->samples, data_size >= bytes_to_do ? bytes_to_do : data_size); // 2 for stereo
                    bytes_done += data_size >= bytes_to_do ? bytes_to_do : data_size;
                }
                THEORAPLAY_freeAudio(packet);
            }

            Sint16 convBuffer[MIX_BUFFER_SAMPLES];

            // If we need more samples, assume we've reached the end of the file,
            // and flush the audio stream so we can get more. If we were wrong, and
            // there's still more file left, then there will be a gap in the audio. Sorry.
            if (bytes_done < bytes_to_do) {
                memset(vid_buffer, 0, bytes_to_do);
            }

            if (bytes_done > 0) {
                SDL_AudioCVT convert;
                MEM_ZERO(convert);
                int cvtResult =
                    SDL_BuildAudioCVT(&convert, AUDIO_S16SYS, 2, 48000, audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
                if (cvtResult == 0) {
                    if (convert.len_mult > 0) {
                        convert.buf = (byte *)malloc(bytes_done * convert.len_mult);
                        convert.len = bytes_done;
                        memcpy(convert.buf, vid_buffer, bytes_done);
                        SDL_ConvertAudio(&convert);
                    }
                }

                if (cvtResult == 0)
                    ProcessAudioMixing(mix_buffer, (const Sint16 *)convert.buf, bytes_done / sizeof(Sint16), MAX_VOLUME, 0);

                if (convert.len > 0 && convert.buf)
                    free(convert.buf);
            }
        }*/
#endif

        // Mix SFX
        for (byte i = 0; i < CHANNEL_COUNT; ++i) {
            ChannelInfo *sfx = &sfxChannels[i];
            if (sfx == NULL)
                continue;

            if (sfx->sfxID < 0)
                continue;

            if (sfx->samplePtr) {
                Sint16 buffer[MIX_BUFFER_SAMPLES];

                size_t samples_done = 0;
                while (samples_done != samples_to_do) {
                    size_t sampleLen = (sfx->sampleLength < samples_to_do - samples_done) ? sfx->sampleLength : samples_to_do - samples_done;
                    memcpy(&buffer[samples_done], sfx->samplePtr, sampleLen * sizeof(Sint16));

                    samples_done += sampleLen;
                    sfx->samplePtr += sampleLen;
                    sfx->sampleLength -= sampleLen;

                    if (sfx->sampleLength == 0) {
                        if (sfx->loopSFX) {
                            sfx->samplePtr    = sfxList[sfx->sfxID].buffer;
                            sfx->sampleLength = sfxList[sfx->sfxID].length;
                        }
                        else {
                            MEM_ZEROP(sfx);
                            sfx->sfxID = -1;
                            break;
                        }
                    }
                }

#if RETRO_USING_SDL1 || RETRO_USING_SDL2
                ProcessAudioMixing(mix_buffer, buffer, (int)samples_done, sfxVolume, sfx->pan);
#endif
            }
        }

        // Clamp mixed samples back to 16-bit and write them to the output buffer
        for (size_t i = 0; i < sizeof(mix_buffer) / sizeof(*mix_buffer); ++i) {
            const Sint16 max_audioval = ((1 << (16 - 1)) - 1);
            const Sint16 min_audioval = -(1 << (16 - 1));

            const Sint32 sample = mix_buffer[i];

            if (sample > max_audioval)
                *output_buffer++ = max_audioval;
            else if (sample < min_audioval)
                *output_buffer++ = min_audioval;
            else
                *output_buffer++ = sample;
        }

        samples_remaining -= samples_to_do;
    }
}

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
