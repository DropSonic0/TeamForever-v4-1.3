#ifndef VIDEO_HPP
#define VIDEO_HPP

#include "theoraplay.h"

extern int currentVideoFrame;
extern int videoFrameCount;
extern int videoWidth;
extern int videoHeight;
extern float videoAR;

extern THEORAPLAY_Decoder *videoDecoder;
extern const THEORAPLAY_VideoFrame *videoVidData;
extern const THEORAPLAY_AudioPacket *videoAudioData;
extern THEORAPLAY_Io callbacks;

extern byte videoSurface;
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