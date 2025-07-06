#ifndef DEBUG_H
#define DEBUG_H

// Ensure logging is not disabled for PS3 debug builds
#if RETRO_PLATFORM == RETRO_PS3
    #ifdef RETRO_DISABLE_LOG
        #undef RETRO_DISABLE_LOG
    #endif
#endif

#if RETRO_PLATFORM == RETRO_ANDROID
#include <android/log.h>
#endif

extern bool endLine;
inline void PrintLog(const char *msg, ...)
{
#ifndef RETRO_DISABLE_LOG
    if (engineDebugMode) {
        char buffer[0x100];

        // make the full string
        va_list args;
        va_start(args, msg);
        vsprintf(buffer, msg, args);
        if (endLine) {
            printf("%s\n", buffer);
        }
        else {
            printf("%s", buffer);
        }

        char pathBuffer[0x100];
#if RETRO_PLATFORM == RETRO_UWP
        if (!usingCWD)
            sprintf(pathBuffer, "%s/log.txt", getResourcesPath());
        else
            sprintf(pathBuffer, "log.txt");
#elif RETRO_PLATFORM == RETRO_ANDROID
        sprintf(pathBuffer, "%s/log.txt", gamePath);
        __android_log_print(ANDROID_LOG_INFO, "RSDKv4", "%s", buffer);
#elif RETRO_PLATFORM == RETRO_PS3
        sprintf(pathBuffer, BASE_PATH "log.txt");
        printf("PS3 LOG DEBUG: PrintLog(char*) called. Message: %s\n", buffer);
        printf("PS3 LOG DEBUG: PrintLog(char*) called. Message: %s\n", buffer);
        // printf("PS3 LOG DEBUG: Attempting to open log file: %s\n", pathBuffer); // File I/O disabled for PS3
#else
        sprintf(pathBuffer, "log.txt"); // Default path for other platforms
#endif
// File I/O for log.txt on PS3 is now disabled to save memory.
// The #if 0 block can be removed if this is permanent, or kept for easy re-enabling.
#if RETRO_PLATFORM == RETRO_PS3
    // File logging explicitly disabled for PS3 in PrintLog
    // printf("PS3 LOG DEBUG: File logging is disabled.\n");
#else // For other platforms, keep the file logging
        FileIO *file = fOpen(pathBuffer, "a");
        if (file) {
            size_t len = StrLength(buffer);
            if (len > 0) { 
                fWrite(buffer, 1, len, file);
                if (endLine) { // Add newline to file if endLine is true
                    fWrite("\n", 1, 1, file);
                }
            }
            fClose(file);
        } else {
            // Optional: some generic error for other platforms if file opening fails
            // printf("LOG ERROR: FAILED to open log file: %s\n", pathBuffer);
        }
#endif
    }
#endif
}

inline void PrintLog(const ushort *msg)
{
#ifndef RETRO_DISABLE_LOG
    if (engineDebugMode) {
#if RETRO_PLATFORM == RETRO_PS3
        printf("PS3 LOG DEBUG: PrintLog(ushort*) called.\n");
#endif
        int mPos = 0;
        while (msg[mPos]) {
            printf("%lc", (ushort)msg[mPos]);
            mPos++;
        }
        if (endLine) {
            printf("\n");
        }

        char pathBuffer[0x100];
#if RETRO_PLATFORM == RETRO_UWP
        if (!usingCWD)
            sprintf(pathBuffer, "%s/log.txt", getResourcesPath());
        else
            sprintf(pathBuffer, "log.txt");
#elif RETRO_PLATFORM == RETRO_ANDROID
        sprintf(pathBuffer, "%s/log.txt", gamePath);
        __android_log_print(ANDROID_LOG_INFO, "RSDKv4", "%ls", (wchar_t *)msg);
#elif RETRO_PLATFORM == RETRO_PS3
        sprintf(pathBuffer, BASE_PATH "log.txt"); // Path still constructed for console message
        // printf("PS3 LOG DEBUG: Attempting to open log file (ushort): %s\n", pathBuffer); // File I/O disabled for PS3
#else
        sprintf(pathBuffer, "log.txt"); // Default path
#endif

// File I/O for log.txt on PS3 is now disabled to save memory.
#if RETRO_PLATFORM == RETRO_PS3
    // File logging explicitly disabled for PS3 in PrintLog
    // printf("PS3 LOG DEBUG: File logging for ushort is disabled.\n");
#else // For other platforms, keep the file logging
        FileIO *file = fOpen(pathBuffer, "a");
        if (file) {
            mPos = 0;
            bool wroteSomething = false;
            char convBuffer[3]; 
            while (msg[mPos]) {
                if (msg[mPos] < 256) { 
                    convBuffer[0] = (char)msg[mPos];
                    convBuffer[1] = '\0';
                    fWrite(convBuffer, 1, 1, file);
                    wroteSomething = true;
                }
                mPos++;
            }

            if (wroteSomething && endLine) {
                fWrite("\n", 1, 1, file);
            }
            fClose(file);
        } else {
            // printf("LOG ERROR: FAILED to open log file (ushort): %s\n", pathBuffer);
        }
#endif
    }
#endif
}

enum DevMenuMenus {
    DEVMENU_MAIN,
    DEVMENU_PLAYERSEL,
    DEVMENU_STAGELISTSEL,
    DEVMENU_STAGESEL,
    DEVMENU_SCRIPTERROR,
#if !RETRO_USE_ORIGINAL_CODE
    DEVMENU_MODMENU
#endif
};

void InitDevMenu();
void InitErrorMessage();
void ProcessStageSelect();

// Not in original, but the code was, and its cleaner this way
void SetTextMenu(int mode);

#endif //! DEBUG_H
