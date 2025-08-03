#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <SDL2/SDL.h>

#if defined(PS3)
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#else
#include "theoraplay.h"
#endif

extern SDL_AudioStream *ogv_stream;
extern int currentVideoFrame;
extern int videoFrameCount;
extern int videoWidth;
extern int videoHeight;
extern float videoAR;

#if defined(PS3)
extern ogg_sync_state oggSyncState;
extern ogg_stream_state oggTheoraStream;
extern th_info theoraInfo;
extern th_comment theoraComment;
extern th_dec_ctx *theoraDecoder;
extern th_setup_info *theoraSetup;

extern ogg_stream_state oggVorbisStream;
extern vorbis_info vorbisInfo;
extern vorbis_dsp_state vorbisDSP;
extern vorbis_block vorbisBlock;
extern vorbis_comment vorbisComment;
#else
extern THEORAPLAY_Decoder *videoDecoder;
extern const THEORAPLAY_VideoFrame *videoVidData;
extern const THEORAPLAY_AudioPacket *videoAudioData;
extern THEORAPLAY_Io callbacks;
#endif

extern byte videoSurface;
extern FileIO *videoFile;
extern int videoFilePos;
extern int videoPlaying; // 0 = not playing, 1 = playing ogv, 2 = playing rsv
extern int vidFrameMS;
extern int vidBaseTicks;
extern float videoAR;

void PlayVideoFile(char *filePath);
void UpdateVideoFrame();
int ProcessVideo();
void StopVideoPlayback();

void SetupVideoBuffer(int width, int height);
void CloseVideoBuffer();

#endif