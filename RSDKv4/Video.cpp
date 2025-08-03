#include "RetroEngine.hpp"
#include <string>

int currentVideoFrame = 0;
int videoFrameCount = 0;
int videoWidth = 0;
int videoHeight = 0;
float videoAR = 0;

SDL_AudioStream *ogv_stream = NULL;

#if defined(PS3)
ogg_sync_state oggSyncState;
ogg_stream_state oggTheoraStream;
th_info theoraInfo;
th_comment theoraComment;
th_dec_ctx *theoraDecoder;
th_setup_info *theoraSetup;

ogg_stream_state oggVorbisStream;
vorbis_info vorbisInfo;
vorbis_dsp_state vorbisDSP;
vorbis_block vorbisBlock;
vorbis_comment vorbisComment;
#else
THEORAPLAY_Decoder *videoDecoder;
const THEORAPLAY_VideoFrame *videoVidData;
THEORAPLAY_Io callbacks;
#endif

byte videoSurface = 0;
FileIO *videoFile = NULL;
int videoFilePos = 0;
int videoPlaying = 0;
int vidFrameMS = 0;
int vidBaseTicks = 0;

bool videoSkipped = false;

#if !defined(PS3)
static long videoRead(THEORAPLAY_Io *io, void *buf, long buflen)
{
    FileIO *file    = (FileIO *)io->userdata;
    const size_t br = fRead(buf, 1, buflen * sizeof(byte), file);
    if (br == 0)
        return -1;
    return (int)br;
} // IoFopenRead

static void videoClose(THEORAPLAY_Io *io)
{
    FileIO *file = (FileIO *)io->userdata;
    fClose(file);
}
#endif

void PlayVideoFile(char *filePath) {
    char pathBuffer[0x100];
    int len = StrLength(filePath);

    if (StrComp(filePath + ((size_t)len - 2), "us")) {
        filePath[len - 2] = 0;
    }

    StrCopy(pathBuffer, "videos/");
    StrAdd(pathBuffer, filePath);
    StrAdd(pathBuffer, ".ogv");

    bool addPath = true;
    // Fixes ".ani" ".Ani" bug and any other case differences
    char pathLower[0x100];
    memset(pathLower, 0, sizeof(char) * 0x100);
    for (int c = 0; c < strlen(pathBuffer); ++c) {
        pathLower[c] = tolower(pathBuffer[c]);
    }

#if RETRO_USE_MOD_LOADER
    for (int m = 0; m < modList.size(); ++m) {
        if (modList[m].active) {
            std::map<std::string, std::string>::const_iterator iter = modList[m].fileMap.find(pathLower);
            if (iter != modList[m].fileMap.cend()) {
                StrCopy(pathBuffer, iter->second.c_str());
                Engine.usingDataFile = false;
                addPath              = false;
                break;
            }
        }
    }
#endif

    char filepath[0x100];
    if (addPath) {
#if RETRO_PLATFORM == RETRO_UWP
        static char resourcePath[256] = { 0 };

        if (strlen(resourcePath) == 0) {
            auto folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto path   = to_string(folder.Path());

            std::copy(path.begin(), path.end(), resourcePath);
        }

        sprintf(filepath, "%s/%s", resourcePath, pathBuffer);
#elif RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_ANDROID
        sprintf(filepath, "%s/%s", gamePath, pathBuffer);
#else
        sprintf(filepath, "%s%s", BASE_PATH, pathBuffer);
#endif
    }
    else {
        sprintf(filepath, "%s", pathBuffer);
    }

    videoFile = fOpen(filepath, "rb");
    if (videoFile) {
        PrintLog("Loaded File '%s'!", filepath);

#if defined(PS3)
        ogg_sync_init(&oggSyncState);
        
        th_comment_init(&theoraComment);
        th_info_init(&theoraInfo);

        vorbis_info_init(&vorbisInfo);
        vorbis_comment_init(&vorbisComment);

        int theora_p = 0;
        int vorbis_p = 0;

        while (!theora_p || !vorbis_p) {
            char *buffer    = ogg_sync_buffer(&oggSyncState, 4096);
            long bytes      = fRead(buffer, 1, 4096, videoFile);
            ogg_sync_wrote(&oggSyncState, bytes);

            ogg_page oggPage;
            while (ogg_sync_pageout(&oggSyncState, &oggPage) > 0) {
                ogg_stream_state test;

                if (!ogg_page_bos(&oggPage)) {
                    if (theora_p) ogg_stream_pagein(&oggTheoraStream, &oggPage);
                    if (vorbis_p) ogg_stream_pagein(&oggVorbisStream, &oggPage);
                    goto header_end;
                }

                ogg_stream_init(&test, ogg_page_serialno(&oggPage));
                ogg_stream_pagein(&test, &oggPage);
                
                ogg_packet oggPacket;
                ogg_stream_packetout(&test, &oggPacket);

                if (!theora_p && th_decode_headerin(&theoraInfo, &theoraComment, &theoraSetup, &oggPacket) >= 0) {
                    memcpy(&oggTheoraStream, &test, sizeof(test));
                    theora_p = 1;
                }
                else if (!vorbis_p && vorbis_synthesis_headerin(&vorbisInfo, &vorbisComment, &oggPacket) >= 0) {
                    memcpy(&oggVorbisStream, &test, sizeof(test));
                    vorbis_p = 1;
                }
                else {
                    ogg_stream_clear(&test);
                }
            }
        }

    header_end:
        while (theora_p && theora_p < 3) {
            ogg_packet oggPacket;
            int ret = ogg_stream_packetout(&oggTheoraStream, &oggPacket);
            if (ret < 0) break;
            if (ret > 0) {
                if(th_decode_headerin(&theoraInfo, &theoraComment, &theoraSetup, &oggPacket)) break;
                theora_p++;
            }

            ogg_page oggPage;
            if(ogg_sync_pageout(&oggSyncState, &oggPage) > 0) {
                if (theora_p) ogg_stream_pagein(&oggTheoraStream, &oggPage);
                if (vorbis_p) ogg_stream_pagein(&oggVorbisStream, &oggPage);
            }
            else {
                char *buffer    = ogg_sync_buffer(&oggSyncState, 4096);
                long bytes      = fRead(buffer, 1, 4096, videoFile);
                ogg_sync_wrote(&oggSyncState, bytes);
            }
        }

        while (vorbis_p && vorbis_p < 3) {
            ogg_packet oggPacket;
            int ret = ogg_stream_packetout(&oggVorbisStream, &oggPacket);
            if (ret < 0) break;
            if (ret > 0) {
                if(vorbis_synthesis_headerin(&vorbisInfo, &vorbisComment, &oggPacket)) break;
                vorbis_p++;
            }

            ogg_page oggPage;
            if(ogg_sync_pageout(&oggSyncState, &oggPage) > 0) {
                if (theora_p) ogg_stream_pagein(&oggTheoraStream, &oggPage);
                if (vorbis_p) ogg_stream_pagein(&oggVorbisStream, &oggPage);
            }
            else {
                char *buffer    = ogg_sync_buffer(&oggSyncState, 4096);
                long bytes      = fRead(buffer, 1, 4096, videoFile);
                ogg_sync_wrote(&oggSyncState, bytes);
            }
        }

        theoraDecoder = th_decode_alloc(&theoraInfo, theoraSetup);
        th_setup_free(theoraSetup);

        vorbis_synthesis_init(&vorbisDSP, &vorbisInfo);
        vorbis_block_init(&vorbisDSP, &vorbisBlock);

        videoWidth  = theoraInfo.pic_width;
        videoHeight = theoraInfo.pic_height;
        videoAR = float(videoWidth) / float(videoHeight);

        SetupVideoBuffer(videoWidth, videoHeight);
        vidBaseTicks = SDL_GetTicks();
        vidFrameMS   = (theoraInfo.fps_denominator == 0.0) ? 0 : ((Uint32)(1000.0 * theoraInfo.fps_denominator / theoraInfo.fps_numerator));
#else
        callbacks.read     = videoRead;
        callbacks.close    = videoClose;
        callbacks.userdata = (void *)videoFile;

        // TODO
        // perhaps implement multi audio stream support? (e.g. sonic cd cutscenes)
#if RETRO_USING_SDL2 && !RETRO_USING_OPENGL
        videoDecoder = THEORAPLAY_startDecode(&callbacks, 4, THEORAPLAY_VIDFMT_IYUV);
#endif

        // TODO: does SDL1.2 support YUV?
#if RETRO_USING_SDL1 && !RETRO_USING_OPENGL
        //videoDecoder = THEORAPLAY_startDecode(&callbacks, /*FPS*/ 30, THEORAPLAY_VIDFMT_RGBA, GetGlobalVariableByName("Options.Soundtrack") ? 1 : 0);
        videoDecoder = THEORAPLAY_startDecodeFile(filepath, 1, THEORAPLAY_VIDFMT_IYUV);
#endif

#if RETRO_USING_OPENGL
        //videoDecoder = THEORAPLAY_startDecode(&callbacks, /*FPS*/ 30, THEORAPLAY_VIDFMT_RGBA, GetGlobalVariableByName("Options.Soundtrack") ? 1 : 0);
        videoDecoder = THEORAPLAY_startDecodeFile(filepath, 1, THEORAPLAY_VIDFMT_RGBA);
#endif

        if (!videoDecoder) {
            PrintLog("Video Decoder Error!");
            return;
        }
        while (!videoVidData) {
            if (!videoVidData)
                videoVidData = THEORAPLAY_getVideo(videoDecoder);
        }
        if (!videoVidData) {
            PrintLog("Video Error!");
            return;
        }

        videoWidth  = videoVidData->width;
        videoHeight = videoVidData->height;
        // commit video Aspect Ratio.
        videoAR = float(videoWidth) / float(videoHeight);

        SetupVideoBuffer(videoWidth, videoHeight);
        vidBaseTicks = SDL_GetTicks();
        vidFrameMS   = (videoVidData->fps == 0.0) ? 0 : ((Uint32)(1000.0 / videoVidData->fps));
#endif
        videoPlaying = 1; // playing ogv
        trackID      = TRACK_COUNT - 1;

        videoSkipped    = false;
        Engine.gameMode = ENGINE_VIDEOWAIT;
    }
    else {
        PrintLog("Couldn't find file '%s'!", filepath);
    }
}

void UpdateVideoFrame()
{
    if (videoPlaying == 2) {
        if (currentVideoFrame < videoFrameCount) {
            GFXSurface *surface = &gfxSurface[videoSurface];
            byte fileBuffer     = 0;
            ushort fileBuffer2  = 0;
            FileRead(&fileBuffer, 1);
            videoFilePos += fileBuffer;
            FileRead(&fileBuffer, 1);
            videoFilePos += fileBuffer << 8;
            FileRead(&fileBuffer, 1);
            videoFilePos += fileBuffer << 16;
            FileRead(&fileBuffer, 1);
            videoFilePos += fileBuffer << 24;

            byte clr[3];
            for (int i = 0; i < 0x80; ++i) {
                FileRead(&clr, 3);
                activePalette32[i].r = clr[0];
                activePalette32[i].g = clr[1];
                activePalette32[i].b = clr[2];
                activePalette[i]     = ((ushort)(clr[0] >> 3) << 11) | 32 * (clr[1] >> 2) | (clr[2] >> 3);
            }

            FileRead(&fileBuffer, 1);
            while (fileBuffer != ',') FileRead(&fileBuffer, 1); // gif image start identifier

            FileRead(&fileBuffer2, 2); // IMAGE LEFT
            FileRead(&fileBuffer2, 2); // IMAGE TOP
            FileRead(&fileBuffer2, 2); // IMAGE WIDTH
            FileRead(&fileBuffer2, 2); // IMAGE HEIGHT
            FileRead(&fileBuffer, 1);  // PaletteType
            bool interlaced = (fileBuffer & 0x40) >> 6;
            if (fileBuffer >> 7 == 1) {
                int c = 0x80;
                do {
                    ++c;
                    FileRead(&fileBuffer, 1);
                    FileRead(&fileBuffer, 1);
                    FileRead(&fileBuffer, 1);
                } while (c != 0x100);
            }
            ReadGifPictureData(surface->width, surface->height, interlaced, graphicData, surface->dataPosition);

            SetFilePosition(videoFilePos);
            ++currentVideoFrame;
        }
        else {
            videoPlaying = 0;
            CloseFile();
        }
    }
}

int ProcessVideo() {
    if (videoPlaying == 1) {
        CheckKeyPress(&inputPress);

        if (videoSkipped && fadeMode < 0xFF) {
            fadeMode += 8;
        }

        // taxman forgot the anyPress i guess
        if (inputDevice[INPUT_BUTTONA].press || inputDevice[INPUT_BUTTONB].press || inputDevice[INPUT_BUTTONC].press || inputDevice[INPUT_BUTTONX].press || inputDevice[INPUT_BUTTONY].press || inputDevice[INPUT_BUTTONZ].press || inputDevice[INPUT_BUTTONL].press || inputDevice[INPUT_BUTTONR].press || inputDevice[INPUT_START].press || inputDevice[INPUT_SELECT].press || touches > 0) {
            if (!videoSkipped)
                fadeMode = 0;

            videoSkipped = true;
        }

#if defined(PS3)
        // Main decoding loop
        while (true) {
            // Try to decode and queue some audio
            float **pcm;
            int frames = vorbis_synthesis_pcmout(&vorbisDSP, &pcm);
            if (frames > 0) {
                // Simple mono mixdown for now
                float *buffer = (float*)malloc(frames * sizeof(float));
                for (int i = 0; i < frames; i++) {
                    buffer[i] = (pcm[0][i] + pcm[1][i]) * 0.5f;
                }
                SDL_AudioStreamPut(ogv_stream, buffer, frames * sizeof(float));
                free(buffer);
                vorbis_synthesis_read(&vorbisDSP, frames);
            }

            // Try to decode a video frame
            ogg_packet oggPacket;
            if (ogg_stream_packetout(&oggTheoraStream, &oggPacket) > 0) {
                if (th_decode_packetin(theoraDecoder, &oggPacket, NULL) == 0) {
                    th_ycbcr_buffer ycbcr;
                    th_decode_ycbcr_out(theoraDecoder, ycbcr);
                    void *pixels;
                    int pitch;
                    SDL_LockTexture(Engine.videoBuffer, NULL, &pixels, &pitch);
                    
                    int y_w = ycbcr[0].width;
                    int y_h = ycbcr[0].height;
                    int uv_w = ycbcr[1].width;
                    int uv_h = ycbcr[1].height;

                    Uint8 *p = (Uint8*)pixels;
                    for(int i=0; i<y_h; i++) {
                        memcpy(p, ycbcr[0].data + i * ycbcr[0].stride, y_w);
                        p += pitch;
                    }
                    for(int i=0; i<uv_h; i++) {
                        memcpy(p, ycbcr[1].data + i * ycbcr[1].stride, uv_w);
                        p += pitch / 2;
                    }
                    for(int i=0; i<uv_h; i++) {
                        memcpy(p, ycbcr[2].data + i * ycbcr[2].stride, uv_w);
                        p += pitch / 2;
                    }
                    SDL_UnlockTexture(Engine.videoBuffer);
                    break; // Decoded a video frame, exit loop for this ProcessVideo call
                }
            }

            // If we are here, we need more data for one or both streams
            char *buffer = ogg_sync_buffer(&oggSyncState, 4096);
            long bytes = fRead(buffer, 1, 4096, videoFile);
            if (bytes <= 0) {
                // End of file
                StopVideoPlayback();
                ResumeSound();
                return 1;
            }
            ogg_sync_wrote(&oggSyncState, bytes);

            ogg_page oggPage;
            while (ogg_sync_pageout(&oggSyncState, &oggPage) > 0) {
                if (ogg_stream_pagein(&oggTheoraStream, &oggPage) != 0) {
                    // Page doesn't belong to theora, try vorbis
                    ogg_stream_pagein(&oggVorbisStream, &oggPage);
                }
            }

            // Try to get an audio packet
            if (ogg_stream_packetout(&oggVorbisStream, &oggPacket) > 0) {
                if (vorbis_synthesis(&vorbisBlock, &oggPacket) == 0) {
                    vorbis_synthesis_blockin(&vorbisDSP, &vorbisBlock);
                }
            }
        }
        return 2;
#else
        // ok so
        // theoraplay is just never returning false for some reason???
        // i hacked around this i guess, check line 237
        if (/*!THEORAPLAY_isDecoding(videoDecoder) || */(videoSkipped && fadeMode >= 0xFF)) {
            StopVideoPlayback();
            ResumeSound();
            return 1; // video finished
        }

        // Don't pause or it'll go wild
        if (videoPlaying == 1) {
            const Uint32 now = (SDL_GetTicks() - vidBaseTicks);

            if (!videoVidData) {
                videoVidData = THEORAPLAY_getVideo(videoDecoder);
                // we done lmao
                if (!videoVidData) {
                    StopVideoPlayback();
                    ResumeSound();
                    return 1;
                }
            }

            // Play video frames when it's time.
            if (videoVidData && (videoVidData->playms <= now)) {
                if (vidFrameMS && ((now - videoVidData->playms) >= vidFrameMS)) {
                    // Skip frames to catch up, but keep track of the last one+
                    //  in case we catch up to a series of dupe frames, which
                    //  means we'd have to draw that final frame and then wait for
                    //  more.

                    const THEORAPLAY_VideoFrame *last = videoVidData;
                    while ((videoVidData = THEORAPLAY_getVideo(videoDecoder)) != NULL) {
                        THEORAPLAY_freeVideo(last);
                        last = videoVidData;
                        if ((now - videoVidData->playms) < vidFrameMS)
                            break;
                    }

                    if (!videoVidData)
                        videoVidData = last;
                }

                // do nothing; we're far behind and out of options.
                if (!videoVidData) {
                    // video lagging uh oh
                }

#if RETRO_USING_OPENGL
                glBindTexture(GL_TEXTURE_2D, videoBuffer);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoVidData->width, videoVidData->height, GL_RGBA, GL_UNSIGNED_BYTE, videoVidData->pixels);
                glBindTexture(GL_TEXTURE_2D, 0);
#elif RETRO_USING_SDL2
                int half_w     = videoVidData->width / 2;
                const Uint8 *y = (const Uint8 *)videoVidData->pixels;
                const Uint8 *u = y + (videoVidData->width * videoVidData->height);
                const Uint8 *v = u + (half_w * (videoVidData->height / 2));

                SDL_UpdateYUVTexture(Engine.videoBuffer, NULL, y, videoVidData->width, u, half_w, v, half_w);
#elif RETRO_USING_SDL1
                memcpy(Engine.videoBuffer->pixels, videoVidData->pixels, videoVidData->width * videoVidData->height * sizeof(uint));
#endif
                THEORAPLAY_freeVideo(videoVidData);
                videoVidData = NULL;
            }

            return 2; // its playing as expected
        }
#endif
    }

    return 0; // its not even initialised
}

void StopVideoPlayback()
{
    if (videoPlaying == 1) {
        // `videoPlaying` and `videoDecoder` are read by
        // the audio thread, so lock it to prevent a race
        // condition that results in invalid memory accesses.
        SDL_LockAudio();

        if (videoSkipped && fadeMode >= 0xFF)
            fadeMode = 0;

#if defined(PS3)
        ogg_stream_clear(&oggTheoraStream);
        th_decode_free(theoraDecoder);
        th_comment_clear(&theoraComment);
        th_info_clear(&theoraInfo);

        ogg_stream_clear(&oggVorbisStream);
        vorbis_block_clear(&vorbisBlock);
        vorbis_dsp_clear(&vorbisDSP);
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);

        ogg_sync_clear(&oggSyncState);
#else
        if (videoVidData) {
            THEORAPLAY_freeVideo(videoVidData);
            videoVidData = NULL;
        }
        if (videoDecoder) {
            THEORAPLAY_stopDecode(videoDecoder);
            videoDecoder = NULL;
        }
#endif
        CloseVideoBuffer();
        videoPlaying = 0;

        SDL_UnlockAudio();
    }
}

void SetupVideoBuffer(int width, int height)
{
#if RETRO_USING_OPENGL
    if (videoBuffer > 0) {
        glDeleteTextures(1, &videoBuffer);
        videoBuffer = 0;
    }
    glGenTextures(1, &videoBuffer);
    glBindTexture(GL_TEXTURE_2D, videoBuffer);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoVidData->width, videoVidData->height, GL_RGBA, GL_UNSIGNED_BYTE, videoVidData->pixels);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
	
	if (!videoBuffer || !&videoBuffer || !videoVidData)
        PrintLog("Failed to create video buffer!");
#elif RETRO_USING_SDL1
    Engine.videoBuffer = SDL_CreateRGBSurface(0, width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    if (!Engine.videoBuffer)
        PrintLog("Failed to create video buffer!");
#elif RETRO_USING_SDL2
#if defined(PS3)
    Engine.videoBuffer = SDL_CreateTexture(Engine.renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
#else
    Engine.videoBuffer = SDL_CreateTexture(Engine.renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_TARGET, width, height);
#endif

    if (!Engine.videoBuffer)
        PrintLog("Failed to create video buffer!");
#endif
}

void CloseVideoBuffer()
{
    if (videoPlaying == 1) {
#if RETRO_USING_OPENGL
        if (videoBuffer > 0) {
            glDeleteTextures(1, &videoBuffer);
            videoBuffer = 0;
        }
#elif RETRO_USING_SDL1
        SDL_FreeSurface(Engine.videoBuffer);
        Engine.videoBuffer = nullptr;
#elif RETRO_USING_SDL2
        SDL_DestroyTexture(Engine.videoBuffer);
        Engine.videoBuffer = nullptr;
#endif
    }
}