#ifndef PS3_PPU_PRX_LOADER // If not defined by compiler, define it here for this file
#define PS3_PPU_PRX_LOADER
#endif

#include "RetroEngine.hpp"

#if !RETRO_USE_ORIGINAL_CODE

#if RETRO_PLATFORM == RETRO_WIN
#include "Windows.h"
#endif

void parseArguments(int argc, char *argv[])
{
    for (int a = 0; a < argc; ++a) {
        const char *find = "";

        find = strstr(argv[a], "stage=");
        if (find) {
            int b = 0;
            int c = 6;
            while (find[c] && find[c] != ';') Engine.startSceneFolder[b++] = find[c++];
            Engine.startSceneFolder[b] = 0;
        }

        find = strstr(argv[a], "scene=");
        if (find) {
            int b = 0;
            int c = 6;
            while (find[c] && find[c] != ';') Engine.startSceneID[b++] = find[c++];
            Engine.startSceneID[b] = 0;
        }

        find = strstr(argv[a], "console=true");
        if (find) {
            engineDebugMode       = true;
            Engine.devMenu        = true;
            Engine.consoleEnabled = true;
#if RETRO_PLATFORM == RETRO_WIN
            AllocConsole();
            freopen_s((FILE **)stdin, "CONIN$", "w", stdin);
            freopen_s((FILE **)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE **)stderr, "CONOUT$", "w", stderr);
#endif
        }

        find = strstr(argv[a], "usingCWD=true");
        if (find) {
            usingCWD = true;
        }
    }
}
#endif

#ifdef NXLINK
#include <switch.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <unistd.h>

static int s_nxlinkSock = -1;

static void initNxLink()
{
    if (R_FAILED(socketInitializeDefault()))
        return;

    s_nxlinkSock = nxlinkStdio();
    if (s_nxlinkSock >= 0)
        printf("printf output now goes to nxlink server\n");
    else
        socketExit();
}
#endif

int main(int argc, char *argv[])
{
    #ifdef PS3_PPU_PRX_LOADER
    printf("PS3_DEBUG: main() function entered.\n");
    #endif
    // The old "PS3 EXECUTION TEST: main() started." should be removed or replaced by the line above.

    // Remove or comment out the other "PS3 EXECUTION TEST" printfs in main if you want, 
    // or leave them if they are helpful for your general PS3 testing.
    // For our current focused debugging, the "PS3_DEBUG" prefixed ones are primary.
    printf("PS3 EXECUTION TEST: Calling SDL_Init(SDL_INIT_EVERYTHING) from main().\n");
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        printf("FATAL ERROR: SDL_Init(SDL_INIT_EVERYTHING) failed in main()! SDL Error: %s\n", SDL_GetError());
        return 1; // Exit early if this fails
    }
    printf("PS3 EXECUTION TEST: SDL_Init(SDL_INIT_EVERYTHING) in main() returned.\n");
#ifdef NXLINK
    initNxLink();
#endif

#if !RETRO_USE_ORIGINAL_CODE
    parseArguments(argc, argv);
#endif

    SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
    Engine.Init();
    Engine.Run();

#if !RETRO_USE_ORIGINAL_CODE
    if (Engine.consoleEnabled) {
#if RETRO_PLATFORM == RETRO_WIN
        FreeConsole();
#endif //! RETRO_PLATFORM == RETRO_WIN
    }
#endif //! !RETRO_USE_ORIGINAL_CODE

#ifdef NXLINK
    socketExit();
#endif //! NXLINK

    return 0;
}

#if RETRO_PLATFORM == RETRO_UWP
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) { return SDL_WinRTRunApp(main, NULL); }
#endif
