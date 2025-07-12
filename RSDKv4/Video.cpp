#include "RetroEngine.hpp"
#include <string> 
#include <algorithm>

int currentVideoFrame = 0;
int videoFrameCount = 0;
int videoWidth = 0;
int videoHeight = 0;
float videoAR = 0;

THEORAPLAY_Decoder *videoDecoder = nullptr;
const THEORAPLAY_VideoFrame *videoVidData = nullptr;
THEORAPLAY_Io callbacks;

byte videoSurface = 0;
int videoFilePos = 0;
int videoPlaying = 0;
int vidFrameMS = 0;
int vidBaseTicks = 0;
bool videoSkipped = false;

static long videoRead(THEORAPLAY_Io *io, void *buf, long buflen)
{
    FileIO *file = (FileIO *)io->userdata;
    size_t br = fRead(buf, 1, buflen, file);
    if (br == 0) { // Si no se ley� nada
    Sint64 current_pos = SDL_RWtell(file);
    Sint64 total_size = SDL_RWsize(file);
    if (current_pos >= total_size) {
        return 0; // EOF para THEORAPLAY
    } else {
        return -1; // Error de lectura real
    }
}
return (long)br;
}

static void videoClose(THEORAPLAY_Io *io)
{
    FileIO *file = (FileIO *)io->userdata;
    if (file) {
        fClose(file);
        io->userdata = nullptr;
    }
}

// This entire old function will be replaced by the corrected one below,
// which was previously inserted. This is to remove the duplicated function.
// The actual new PlayVideoFile function with logging is already present further down.

void PlayVideoFile(char *filePath) {
    PrintLog("PlayVideoFile: Initial filePath = '%s'", filePath); // Log initial filePath

    char pathBuffer[0x100];
    int len = StrLength(filePath);
    if (len > 2 && StrComp(filePath + (len - 2), "us")) {
        filePath[len - 2] = 0; // Remove "us" suffix if present
    }
    StrCopy(pathBuffer, "videos/");
    StrAdd(pathBuffer, filePath);
    StrAdd(pathBuffer, ".ogv");
    PrintLog("PlayVideoFile: pathBuffer after initial mods and adding .ogv = '%s'", pathBuffer);

    bool addBasePath = true; // Renamed from addPath for clarity
    char pathLower[0x100];
    memset(pathLower, 0, sizeof(pathLower));
    for (size_t c = 0; c < strlen(pathBuffer); ++c) {
        pathLower[c] = tolower(pathBuffer[c]);
    }
    PrintLog("PlayVideoFile: pathLower for mod lookup = '%s'", pathLower);

#if RETRO_USE_MOD_LOADER
    PrintLog("PlayVideoFile: RETRO_USE_MOD_LOADER is enabled.");
    for (size_t m = 0; m < modList.size(); ++m) {
        if (modList[m].active) {
            PrintLog("PlayVideoFile: Checking active mod: '%s' (Folder: '%s')", modList[m].name.c_str(), modList[m].folder.c_str());
            std::map<std::string, std::string>::const_iterator iter = modList[m].fileMap.find(pathLower);
            if (iter != modList[m].fileMap.cend()) {
                // If found in mod, the path should be relative to the mod's folder,
                // or an absolute path if the mod system prepares it that way.
                // Assuming iter->second is a path that fOpen can use directly or needs BASE_PATH.
                // For now, let's assume mod paths are relative to game's data directory structure if not absolute.
                // This part might need adjustment based on how mod paths are stored.
                // If iter->second is already an absolute path for the system, addBasePath should be false.
                // If iter->second is relative to the game's root (like "mods/MyMod/videos/video.ogv"),
                // then BASE_PATH should be prepended.

                StrCopy(pathBuffer, iter->second.c_str()); // Use the path from the mod
                PrintLog("PlayVideoFile: Found video in mod '%s'. Using mod path: '%s'", modList[m].name.c_str(), pathBuffer);
                // Determine if BASE_PATH is still needed. If iter->second is like "C:/Games/RSDK/mods/MyMod/video.ogv",
                // then addBasePath should be false. If it's "mods/MyMod/video.ogv", it depends on what BASE_PATH is.
                // For safety, let's assume mods provide paths that might need BASE_PATH unless they are clearly absolute.
                // This logic might need refinement based on typical mod structures.
                // For now, if a mod supplies a path, we'll assume it's complete or relative to something fOpen understands.
                // Let's assume mod paths are relative to BASE_PATH for now, so addBasePath remains true,
                // but pathBuffer is now the mod's specific path.
                // If mods *always* provide a full path, then addBasePath = false would be set here.
                // Given the original logic, if a mod path is found, `addPath` became `false`, implying the mod path was self-contained.
                addBasePath = false; // Mod path is likely complete or relative to a known mod root handled by fOpen
                break;
            }
        }
    }
#else
    PrintLog("PlayVideoFile: RETRO_USE_MOD_LOADER is disabled.");
#endif

    char finalFilepath[0x200];
    if (addBasePath) {
        // This is the typical case for non-modded files or mods that provide relative paths
        sprintf(finalFilepath, "%s%s", BASE_PATH, pathBuffer);
        PrintLog("PlayVideoFile: addBasePath is true. BASE_PATH = '%s'. finalFilepath = '%s'", BASE_PATH ? BASE_PATH : "NULL", finalFilepath);
    } else {
        // This case is for when a mod provides a full, directly usable path
        sprintf(finalFilepath, "%s", pathBuffer);
        PrintLog("PlayVideoFile: addBasePath is false (likely using a mod-provided path). finalFilepath = '%s'", finalFilepath);
    }

    FileIO *file = fOpen(finalFilepath, "rb");
    if (file) {
        PrintLog("Video: Successfully opened '%s'. Attempting to play.", finalFilepath);
        callbacks.read = videoRead; callbacks.close = videoClose; callbacks.userdata = (void *)file;
        videoDecoder = THEORAPLAY_startDecode(&callbacks, 30000, THEORAPLAY_VIDFMT_IYUV);

        if (!videoDecoder) {
            PrintLog("Video Decoder Error: Failed to start decoding '%s'", finalFilepath);
            fClose(file); return;
        }
        PrintLog("Video: Decoder started for '%s'. Waiting for video data...", finalFilepath);
        while (!videoVidData && THEORAPLAY_isDecoding(videoDecoder)) {
            videoVidData = THEORAPLAY_getVideo(videoDecoder);
            // Potentially add a small delay or a counter here if this loop spins too fast without yielding
        }
        if (!videoVidData) {
            PrintLog("Video Error: No video data found in '%s' after attempting to decode. Decoder status: %s", 
                     finalFilepath, THEORAPLAY_isDecoding(videoDecoder) ? "still decoding" : "stopped/error");
            THEORAPLAY_stopDecode(videoDecoder); videoDecoder = nullptr;
            return;
        }
        PrintLog("Video: Video data retrieved for '%s'. Width: %d, Height: %d, FPS: %f", 
                 finalFilepath, videoVidData->width, videoVidData->height, videoVidData->fps);
        videoWidth  = videoVidData->width; videoHeight = videoVidData->height;
        videoAR = (videoHeight != 0) ? ((float)videoWidth / (float)videoHeight) : (16.0f/9.0f);
        SetupVideoBuffer(videoWidth, videoHeight);
        vidBaseTicks = SDL_GetTicks();
        vidFrameMS   = (videoVidData->fps == 0.0) ? 0 : ((Uint32)(1000.0 / videoVidData->fps));
        videoPlaying = 1; 
        videoSkipped = false; Engine.gameMode = ENGINE_VIDEOWAIT;
        PrintLog("Video: Playing '%s' (%dx%d @ %f fps, frame_ms: %d)", finalFilepath, videoWidth, videoHeight, videoVidData->fps, vidFrameMS);
    } else {
        PrintLog("Video Error: COULD NOT OPEN FILE '%s'. fOpen returned NULL.", finalFilepath);
        // Consider adding platform-specific error logging here if fOpen or a similar function provides error codes.
        // For example, on POSIX systems, you might check errno.
    }
}

void UpdateVideoFrame() 
{
    if (videoPlaying == 2) { /* ... Tu l�gica original para formato RVF ... */ }
}

int ProcessVideo() {
    if (videoPlaying == 1 && videoDecoder) { 
        if (inputDevice[INPUT_BUTTONA].press || inputDevice[INPUT_START].press || touches > 0) {
             if (!videoSkipped) fadeMode = 0; 
             videoSkipped = true;
        }
        if (videoSkipped && fadeMode < 0xFF) fadeMode += 8;

        if (!THEORAPLAY_isDecoding(videoDecoder) || (videoSkipped && fadeMode >= 0xFF)) {
            StopVideoPlayback(); return 1; 
        }

        const Uint32 now_ticks = SDL_GetTicks();
        const Uint32 elapsed_ms = now_ticks - vidBaseTicks;

        if (!videoVidData) videoVidData = THEORAPLAY_getVideo(videoDecoder);
        
        if (!videoVidData) { 
            if (!THEORAPLAY_isDecoding(videoDecoder)) { StopVideoPlayback(); return 1; }
            return 2; 
        }

        if (videoVidData->playms <= elapsed_ms) {
            if (vidFrameMS > 0 && (elapsed_ms - videoVidData->playms) >= vidFrameMS) {
                const THEORAPLAY_VideoFrame *last = videoVidData; videoVidData = NULL; THEORAPLAY_freeVideo(last);
                while ((videoVidData = THEORAPLAY_getVideo(videoDecoder)) != NULL) {
                    if ((elapsed_ms - videoVidData->playms) < vidFrameMS) break; 
                    last = videoVidData; videoVidData = NULL; THEORAPLAY_freeVideo(last);
                }
            }
            if (videoVidData) {
#if RETRO_USING_SDL2 && !RETRO_USING_OPENGL && RETRO_SOFTWARE_RENDER
                if (Engine.videoTexture) {
                    int half_w = videoVidData->width / 2, h = videoVidData->height /2;
                    const Uint8 *y = (const Uint8 *)videoVidData->pixels;
                    const Uint8 *u = y + (videoVidData->width * videoVidData->height);
                    const Uint8 *v = u + (half_w * h);
                    SDL_UpdateYUVTexture(Engine.videoTexture, NULL, y, videoVidData->width, u, half_w, v, half_w);
                } else { PrintLog("ProcessVideo Error: Engine.videoTexture is NULL."); }
#endif
                THEORAPLAY_freeVideo(videoVidData); videoVidData = NULL;
            }
        }
        return 2; 
    } else if (videoPlaying == 2) { 
        UpdateVideoFrame(); 
        return videoPlaying == 0 ? 1 : 2; 
    }
    return 0; 
}

void StopVideoPlayback()
{
    if (videoPlaying == 1 && videoDecoder) { 
        if (videoVidData) { THEORAPLAY_freeVideo(videoVidData); videoVidData = NULL; }
        THEORAPLAY_stopDecode(videoDecoder); videoDecoder = NULL;
    }
    CloseVideoBuffer(); 
    videoPlaying = 0; 
    fadeMode = 0; 
    videoSkipped = false;
    PrintLog("Video: Playback stopped.");
}

void SetupVideoBuffer(int width, int height) 
{
#if RETRO_USING_SDL2 && !RETRO_USING_OPENGL && RETRO_SOFTWARE_RENDER
    if (Engine.videoTexture) { SDL_DestroyTexture(Engine.videoTexture); Engine.videoTexture = nullptr; }
    if (Engine.renderer) {
        Engine.videoTexture = SDL_CreateTexture(Engine.renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!Engine.videoTexture) PrintLog("Failed to create video texture! SDL Error: %s", SDL_GetError());
        else PrintLog("Video texture created %dx%d", width, height);
    } else { PrintLog("SetupVideoBuffer Error: Engine.renderer is NULL."); }
#endif
}

void CloseVideoBuffer() 
{
#if RETRO_USING_SDL2 && !RETRO_USING_OPENGL && RETRO_SOFTWARE_RENDER
    if (Engine.videoTexture) {
        SDL_DestroyTexture(Engine.videoTexture);
        Engine.videoTexture = nullptr;
        PrintLog("Video texture destroyed.");
    }
#endif
}