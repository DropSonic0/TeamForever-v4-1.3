#ifndef DEBUG_H
#define DEBUG_H

// Ensure logging is not disabled for PS3 debug builds
#ifdef RETRO_DISABLE_LOG
#undef RETRO_DISABLE_LOG
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
            // sprintf(buffer, "%s\n", buffer); // Redundant, buffer already contains the string
        }
        else {
            printf("%s", buffer);
            // sprintf(buffer, "%s", buffer); // Redundant
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
#else // PS3 falls into this case
        sprintf(pathBuffer, BASE_PATH "log.txt");
#endif
        // PS3 Log Debug: Check path and fOpen result
        printf("PS3 LOG DEBUG: PrintLog(char*) called. Message: %s\n", buffer);
        printf("PS3 LOG DEBUG: Attempting to open log file: %s\n", pathBuffer);
        FileIO *file = fOpen(pathBuffer, "a");
        if (file) {
            printf("PS3 LOG DEBUG: Log file opened successfully. Writing...\n");
            size_t len = StrLength(buffer);
            if (len > 0) { // Only write if there's something to write
                fWrite(buffer, 1, len, file);
            }
            fClose(file);
            printf("PS3 LOG DEBUG: Log file closed.\n");
        } else {
            printf("PS3 LOG DEBUG: FAILED to open log file: %s\n", pathBuffer);
        }
    }
#endif
}

inline void PrintLog(const ushort *msg)
{
#ifndef RETRO_DISABLE_LOG
    if (engineDebugMode) {
        // PS3 Log Debug: ushort version
        printf("PS3 LOG DEBUG: PrintLog(ushort) called.\n");
        
        // First, print to console if possible (original behavior)
        int mPos = 0;
        while (msg[mPos]) {
            printf("%lc", (ushort)msg[mPos]);
            mPos++;
        }
        if (endLine) {
            printf("\n");
        }

        // Now, attempt to write to log file
        char pathBuffer[0x100];
#if RETRO_PLATFORM == RETRO_UWP
        if (!usingCWD)
            sprintf(pathBuffer, "%s/log.txt", getResourcesPath());
        else
            sprintf(pathBuffer, "log.txt");
#elif RETRO_PLATFORM == RETRO_ANDROID
        sprintf(pathBuffer, "%s/log.txt", gamePath);
        __android_log_print(ANDROID_LOG_INFO, "RSDKv4", "%ls", (wchar_t *)msg);
#else // PS3 falls into this case
        sprintf(pathBuffer, BASE_PATH "log.txt");
#endif
        printf("PS3 LOG DEBUG: Attempting to open log file (ushort): %s\n", pathBuffer);
        FileIO *file = fOpen(pathBuffer, "a");
        if (file) {
            printf("PS3 LOG DEBUG: Log file opened successfully (ushort). Writing...\n");
            mPos = 0;
            bool wroteSomething = false;
            while (msg[mPos]) {
                fWrite(&msg[mPos], 2, 1, file); // Assuming ushort is 2 bytes
                mPos++;
                wroteSomething = true;
            }

            if (wroteSomething && endLine) { // Add newline if content was written and endLine is true
                ushort el = '\n';
                fWrite(&el, 2, 1, file);
            }
            fClose(file);
            printf("PS3 LOG DEBUG: Log file closed (ushort).\n");
        } else {
            printf("PS3 LOG DEBUG: FAILED to open log file (ushort): %s\n", pathBuffer);
        }
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
