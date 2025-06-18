#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h> // Para FILE*

// Variables Globales para Logging (definidas en Debug.cpp)
extern FILE* gameLogFile;
extern bool engineDebugMode;
extern bool endLine;
void InitDebug();
void ReleaseDebug();

// Funciones de Logging y Debug
void InitDebug();
void ReleaseDebug();
void PrintLog(const char *msg, ...);
void PrintLog(const ushort *msg); // Si aún la necesitas

// Tus otras declaraciones de Debug
enum DevMenuMenus {
    DEVMENU_MAIN, DEVMENU_PLAYERSEL, DEVMENU_STAGELISTSEL,
    DEVMENU_STAGESEL, DEVMENU_SCRIPTERROR,
#if !RETRO_USE_ORIGINAL_CODE
    DEVMENU_MODMENU
#endif
};
void InitDevMenu();
void InitErrorMessage();
void ProcessStageSelect();
void SetTextMenu(int mode);

#endif // !DEBUG_H