#include "RetroEngine.hpp"
#include <string>

RSDKContainer rsdkContainer;

char fileName[0x100];
byte fileBuffer[0x2000];
int fileSize          = 0;
int vFileSize         = 0;
int readPos           = 0;
int readSize          = 0;
int bufferPosition    = 0;
int virtualFileOffset = 0;
bool useEncryption    = false;
byte packID           = 0;
byte eStringPosA;
byte eStringPosB;
byte eStringNo;
byte eNybbleSwap;
byte encryptionStringA[0x10];
byte encryptionStringB[0x10];

FileIO *cFileHandle = nullptr;

bool CheckRSDKFile(const char *filePath)
{
	PrintLog("CheckRSDKFile: Solicitado RSDK: %s", filePath);
    FileInfo info;

    char filePathBuffer[0x100];
#if RETRO_PLATFORM == RETRO_OSX
    sprintf(filePathBuffer, "%s/%s", gamePath, filePath);
#else
    sprintf(filePathBuffer, "%s", filePath);
#endif

	PrintLog("CheckRSDKFile: Intentando fOpen con ruta construida: %s", filePathBuffer);
    cFileHandle = fOpen(filePathBuffer, "rb");
    if (cFileHandle) {
        byte signature[6] = { 'R', 'S', 'D', 'K', 'v', 'B' };
        byte buf          = 0;
        for (int i = 0; i < 6; ++i) {
            fRead(&buf, 1, 1, cFileHandle);
            if (buf != signature[i])
                return false;
        }

        Engine.usingDataFile = true;
#if !RETRO_USE_ORIGINAL_CODE
        Engine.usingDataFile_Config = true;
#endif

        StrCopy(rsdkContainer.packNames[rsdkContainer.packCount], filePathBuffer);

        byte b[4];
        fRead(&b, 2, 1, cFileHandle);
        ushort fileCount = (b[1] << 8) | (b[0] << 0);
        for (int f = 0; f < fileCount; ++f) {
            for (int y = 0; y < 4; ++y) {
                fRead(b, 1, 4, cFileHandle);
                rsdkContainer.files[f].hash[y] = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | (b[3] << 0);
            }

            fRead(b, 4, 1, cFileHandle);
            rsdkContainer.files[f].offset = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | (b[0] << 0);
            fRead(b, 4, 1, cFileHandle);
            rsdkContainer.files[f].filesize = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | (b[0] << 0);

            rsdkContainer.files[f].encrypted = (rsdkContainer.files[f].filesize & 0x80000000);
            rsdkContainer.files[f].filesize &= 0x7FFFFFFF;

            rsdkContainer.files[f].packID = rsdkContainer.packCount;

            rsdkContainer.fileCount++;
        }

        fClose(cFileHandle);
        cFileHandle = NULL;
        if (LoadFile("Bytecode/GlobalCode.bin", &info)) {
            Engine.usingBytecode = true;
            CloseFile();
        }
        PrintLog("loaded datapack '%s'", filePathBuffer);

        rsdkContainer.packCount++;
        return true;
    }
    else {
        Engine.usingDataFile = false;
#if !RETRO_USE_ORIGINAL_CODE
        Engine.usingDataFile_Config = false;
#endif
        cFileHandle = NULL;

        if (LoadFile("Bytecode/GlobalCode.bin", &info)) {
            Engine.usingBytecode = true;
            CloseFile();
        }
        PrintLog("Couldn't load datapack '%s'", filePathBuffer);
        return false;
    }
}

#if !RETRO_USE_ORIGINAL_CODE
int CheckFileInfo(const char *filepath)
{
    char pathBuf[0x100];
    StrCopy(pathBuf, filepath);
    uint hash[4];
    int len = StrLength(pathBuf);
    GenerateMD5FromString(pathBuf, len, &hash[0], &hash[1], &hash[2], &hash[3]);

    for (int f = 0; f < rsdkContainer.fileCount; ++f) {
        RSDKFileInfo *file = &rsdkContainer.files[f];

        bool match = true;
        for (int h = 0; h < 4; ++h) {
            if (hash[h] != file->hash[h]) {
                match = false;
                break;
            }
        }
        if (!match)
            continue;

        return f;
    }
    return -1;
}

inline bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}
#endif

bool LoadFile(const char *filePath, FileInfo *fileInfo)
{
	PrintLog("LoadFile: Solicitud para cargar: %s", filePath);
    MEM_ZEROP(fileInfo);

    PrintLog("LoadFile: Original filePath request: %s", filePath); // Log original request

    if (cFileHandle)
        fClose(cFileHandle);

    char filePathBuf[0x100];
    StrCopy(filePathBuf, filePath);
    bool forceFolder = false;
#if RETRO_USE_MOD_LOADER
    // Fixes ".ani" ".Ani" bug and any other case differences
    char pathLower[0x100];
    memset(pathLower, 0, sizeof(char) * 0x100);
    for (int c = 0; c < strlen(filePathBuf); ++c) { // filePathBuf still holds the original or slightly modified (e.g. script path)
        pathLower[c] = tolower(filePathBuf[c]);
    }
    PrintLog("LoadFile: Normalized requested path (for map lookup): %s, activeMod: %d", pathLower, activeMod);

    bool addPath = true;
    int startModIndex = activeMod != -1 ? activeMod : 0;
    int endModIndex = activeMod != -1 ? activeMod + 1 : modList.size();

    for (int m = startModIndex; m < endModIndex; ++m) {
        if (modList[m].active) {
            PrintLog("LoadFile: Checking active mod: %s (Path: %s)", modList[m].name.c_str(), modList[m].path.c_str());
            std::map<std::string, std::string>::const_iterator iter = modList[m].fileMap.find(pathLower);
            if (iter != modList[m].fileMap.cend()) {
                PrintLog("LoadFile: Found in mod '%s' fileMap. Key: '%s', Value: '%s'", modList[m].name.c_str(), iter->first.c_str(), iter->second.c_str());
                StrCopy(filePathBuf, iter->second.c_str()); // Use the full path from the mod's fileMap
                forceFolder = true; // Indicates we are loading directly from a folder (the mod's folder)
                addPath     = false;    // Prevent BASE_PATH from being prepended later by the OSX/Android block
                PrintLog("LoadFile: Overriding filePathBuf with mod file: %s. forceFolder=true, addPath=false", filePathBuf);
                break; 
            } else {
                PrintLog("LoadFile: Not found in mod '%s' fileMap for key: %s", modList[m].name.c_str(), pathLower);
            }
        }
    }

    if (forceUseScripts && !forceFolder) { // This logic might need review if script paths are already absolute from fileMap
        if (std::string(filePathBuf).rfind("Data/Scripts/", 0) == 0 && ends_with(std::string(filePathBuf), "txt")) {
            PrintLog("LoadFile: forceUseScripts active and script detected: %s", filePathBuf);
            forceFolder   = true;
            Engine.usingDataFile = false; // Typically scripts are not in RSDK
            addPath              = true; // This seems counterintuitive if we want to load from a specific "scripts/" folder.
                                         // If scripts are in the mod, fileMap should handle it.
                                         // If they are relative to BASE_PATH/scripts/, this addPath=true might be for the OSX/Android block.
            std::string fStr     = std::string(filePathBuf);
            fStr.erase(fStr.begin(), fStr.begin() + 5); // remove "Data/"
            StrCopy(filePathBuf, fStr.c_str());
            PrintLog("LoadFile: forceUseScripts modified filePathBuf to: %s. addPath is now true.", filePathBuf);
        }
    }
#endif

#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_ANDROID || RETRO_PLATFORM == RETRO_PS3
// For PS3, always try to construct full path if addPath is true, similar to OSX/Android.
// This handles cases where the file is not in a mod and needs BASE_PATH.
#if RETRO_USE_MOD_LOADER
    if (addPath) {
        PrintLog("LoadFile: addPath is true, prepending gamePath (BASE_PATH): %s to filePathBuf: %s", gamePath, filePathBuf);
#else
    if (true) { // Non-modloader version always prepends
        PrintLog("LoadFile: (No ModLoader) Prepending gamePath (BASE_PATH): %s to filePathBuf: %s", gamePath, filePathBuf);
#endif
        char pathBuf[0x100];
        sprintf(pathBuf, "%s%s", gamePath, filePathBuf); // Ensure gamePath has trailing slash or filePathBuf has leading if needed. Given BASE_PATH, this should be fine.
        sprintf(filePathBuf, "%s", pathBuf);
        PrintLog("LoadFile: Final filePathBuf after potential gamePath prepend: %s", filePathBuf);
    } else {
        PrintLog("LoadFile: addPath is false, filePathBuf (%s) is considered absolute or already handled by mod.", filePathBuf);
    }
#endif

    cFileHandle = NULL;
#if !RETRO_USE_ORIGINAL_CODE
    StringLowerCase(fileInfo->fileName, filePath); // filePath here is the original requested path, used for hashing
    StrCopy(fileName, fileInfo->fileName); // fileName is also based on original requested path

    // filePathBuf now contains the potentially mod-redirected full path, or BASE_PATH + original filePath
    PrintLog("LoadFile: Attempting to load from DataPack. Original filePath for hashing: %s. Effective path for fOpen (if not in pack): %s", filePath, filePathBuf);

    int fileIndex = CheckFileInfo(fileName); // CheckFileInfo uses the original, non-mod-redirected name for hash lookup
    if (fileIndex != -1 && !forceFolder) { // If found in RSDK's manifest AND not forced to load from folder (mod)
        RSDKFileInfo *file = &rsdkContainer.files[fileIndex];
        packID      = file->packID;
        cFileHandle = fOpen(rsdkContainer.packNames[file->packID], "rb"); // Open the RSDK pack file
		PrintLog("LoadFile (desde DataPack): Matched file in RSDK pack. PackID: %d (%s), File: %s (original request: %s)", file->packID, rsdkContainer.packNames[file->packID], fileName, filePath);
        if (cFileHandle) {
            fSeek(cFileHandle, 0, SEEK_END);
            fileSize = (int)fTell(cFileHandle); // Total size of the RSDK pack file

            vFileSize         = file->filesize;
            virtualFileOffset = file->offset;
            readPos           = file->offset;
            readSize          = 0;
            bufferPosition    = 0;
            fSeek(cFileHandle, virtualFileOffset, SEEK_SET);

            useEncryption = file->encrypted;
            memset(fileInfo->encryptionStringA, 0, 0x10 * sizeof(byte));
            memset(fileInfo->encryptionStringB, 0, 0x10 * sizeof(byte));
            if (useEncryption) {
                GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
                eStringNo   = (vFileSize & 0x1FC) >> 2;
                eStringPosA = 0;
                eStringPosB = 8;
                eNybbleSwap = 0;
                memcpy(fileInfo->encryptionStringA, encryptionStringA, 0x10 * sizeof(byte));
                memcpy(fileInfo->encryptionStringB, encryptionStringB, 0x10 * sizeof(byte));
            }

            fileInfo->readPos           = readPos;
            fileInfo->fileSize          = fileSize;
            fileInfo->vfileSize         = vFileSize;
            fileInfo->virtualFileOffset = virtualFileOffset;
            fileInfo->eStringNo         = eStringNo;
            fileInfo->eStringPosB       = eStringPosB;
            fileInfo->eStringPosA       = eStringPosA;
            fileInfo->eNybbleSwap       = eNybbleSwap;
            fileInfo->bufferPosition    = bufferPosition;
            fileInfo->useEncryption     = useEncryption;
            fileInfo->packID            = packID;
            fileInfo->usingDataPack     = true;
            // PrintLog("Loaded Data File '%s'", filePath); // Original filePath
            PrintLog("LoadFile: Successfully opened and set up from RSDK pack for: %s (original request: %s)", fileName, filePath);

            Engine.usingDataFile = true; // This implies that subsequent reads will use the RSDK pack logic

            return true;
        } else {
            PrintLog("LoadFile (desde DataPack): Found in RSDK manifest but fOpen failed for pack: %s", rsdkContainer.packNames[file->packID]);
        }
#else // RETRO_USE_ORIGINAL_CODE path (less logging, similar logic)
    if (Engine.usingDataFile) { // This implies an RSDK file has been loaded
        StringLowerCase(fileInfo->fileName, filePath); // Original requested path
        StrCopy(fileName, fileInfo->fileName); // Original requested path
        uint hash[4];
        int len = StrLength(fileInfo->fileName);
        GenerateMD5FromString(fileInfo->fileName, len, &hash[0], &hash[1], &hash[2], &hash[3]);

        for (int f = 0; f < rsdkContainer.fileCount; ++f) {
            RSDKFileInfo *file = &rsdkContainer.files[f];

            bool match = true;
            for (int h = 0; h < 4; ++h) {
                if (hash[h] != file->hash[h]) {
                    match = false;
                    break;
                }
            }
            if (!match)
                continue;

            // If we are here, a match was found in the RSDK file's manifest
            packID      = file->packID;
            cFileHandle = fOpen(rsdkContainer.packNames[file->packID], "rb");
            if (cFileHandle) {
                fSeek(cFileHandle, 0, SEEK_END);
                fileSize = (int)fTell(cFileHandle); // Total size of RSDK pack

                vFileSize         = file->filesize; // Size of the specific file entry in RSDK
                virtualFileOffset = file->offset;   // Offset of the specific file entry in RSDK
                readPos           = file->offset;
                readSize          = 0;
                bufferPosition    = 0;
                fSeek(cFileHandle, virtualFileOffset, SEEK_SET);

                useEncryption = file->encrypted;
                memset(fileInfo->encryptionStringA, 0, 0x10 * sizeof(byte));
                memset(fileInfo->encryptionStringB, 0, 0x10 * sizeof(byte));
                if (useEncryption) {
                    GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
                    eStringNo   = (vFileSize & 0x1FC) >> 2;
                    eStringPosA = 0;
                    eStringPosB = 8;
                    eNybbleSwap = 0;
                    memcpy(fileInfo->encryptionStringA, encryptionStringA, 0x10 * sizeof(byte));
                    memcpy(fileInfo->encryptionStringB, encryptionStringB, 0x10 * sizeof(byte));
                }

                fileInfo->readPos           = readPos;
                fileInfo->fileSize          = fileSize; // RSDK pack total size
                fileInfo->vfileSize         = vFileSize; // Specific file size
                fileInfo->virtualFileOffset = virtualFileOffset;
                fileInfo->eStringNo         = eStringNo;
                fileInfo->eStringPosB       = eStringPosB;
                fileInfo->eStringPosA       = eStringPosA;
                fileInfo->eNybbleSwap       = eNybbleSwap;
                fileInfo->bufferPosition    = bufferPosition;
                fileInfo->useEncryption     = useEncryption;
                fileInfo->packID            = packID;
                fileInfo->usingDataPack     = true; // Indicates using RSDK pack logic
                PrintLog("Loaded Data File (Original Code Path) '%s'", filePath);

                return true;
            }
            else { // fOpen for RSDK pack failed
                PrintLog("LoadFile (Original Code Path): RSDK file match, but fOpen failed for pack %s", rsdkContainer.packNames[file->packID]);
                break; 
            }
        }
#endif
        // If we reach here, the file was not found in any RSDK pack's manifest, or opening the pack failed
        PrintLog("LoadFile: File '%s' not found in any RSDK pack manifest or RSDK pack failed to open. (forceFolder was %s)", filePath, forceFolder ? "true" : "false");
        // Now, this will fall through to the direct file loading part using filePathBuf if RETRO_USE_ORIGINAL_CODE is false
        // or just fail if RETRO_USE_ORIGINAL_CODE is true and it didn't find it in the RSDK
    }
    // This 'else' corresponds to `if (fileIndex != -1 && !forceFolder)` for !RETRO_USE_ORIGINAL_CODE
    // OR `if (Engine.usingDataFile)` for RETRO_USE_ORIGINAL_CODE.
    // It means either:
    // 1. (!RETRO_USE_ORIGINAL_CODE) The file was NOT found in RSDK manifest OR `forceFolder` was true (mod load)
    // 2. (RETRO_USE_ORIGINAL_CODE) `Engine.usingDataFile` was false (no RSDK loaded, try direct file access)
    // In both these scenarios, we try to load the file directly using filePathBuf.
    // For !RETRO_USE_ORIGINAL_CODE, filePathBuf could be the mod path or BASE_PATH + original.
    // For RETRO_USE_ORIGINAL_CODE, filePathBuf would be BASE_PATH + original.
    else { // Attempt to load as a direct file (mod file or non-RSDK game file)
        PrintLog("LoadFile: Attempting to load as direct file using path: %s", filePathBuf);
        StrCopy(fileInfo->fileName, filePathBuf); // Use the (potentially mod-redirected) filePathBuf
        StrCopy(fileName, fileInfo->fileName);    // Keep fileName in sync with what we are trying to open

        cFileHandle = fOpen(fileInfo->fileName, "rb");
        if (!cFileHandle) {
            PrintLog("LoadFile: Direct fOpen failed for: '%s' (Original request was '%s')", fileInfo->fileName, filePath);
            return false;
        }
        PrintLog("LoadFile: Successfully opened direct file: %s", fileInfo->fileName);
        virtualFileOffset = 0; // No offset for direct files
        fSeek(cFileHandle, 0, SEEK_END);
        fileInfo->fileSize = (int)fTell(cFileHandle);
        fileSize = fileInfo->vfileSize = fileInfo->fileSize;
        fSeek(cFileHandle, 0, SEEK_SET);
        readPos           = 0;
        fileInfo->readPos = readPos;
        packID = fileInfo->packID = -1;
        fileInfo->usingDataPack   = false;
        bufferPosition            = 0;
        readSize                  = 0;
        useEncryption             = false;

#if !RETRO_USE_ORIGINAL_CODE
        Engine.usingDataFile = false; // When loading a direct file, we are not using the RSDK pack logic
#endif

        // PrintLog("Loaded File '%s'", filePath); // filePath is original, fileInfo->fileName has the actual path opened
        PrintLog("LoadFile: Successfully loaded direct file: %s (Original request: %s)", fileInfo->fileName, filePath);
        return true;
    }
}

void GenerateELoadKeys(uint key1, uint key2)
{
    char buffer[0x20];
    uint hash[0x4];

    // StringA
    ConvertIntegerToString(buffer, key1);
    int len = StrLength(buffer);
    GenerateMD5FromString(buffer, len, &hash[0], &hash[1], &hash[2], &hash[3]);

#if !RETRO_USE_ORIGINAL_CODE
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) encryptionStringA[i * 4 + j] = (hash[i] >> (8 * (j ^ 3))) & 0xFF;
#else
    for (int y = 0; y < 0x10; y += 4) {
        encryptionStringA[y + 3] = hash[y + 0];
        encryptionStringA[y + 2] = hash[y + 1];
        encryptionStringA[y + 1] = hash[y + 2];
        encryptionStringA[y + 0] = hash[y + 3];
    }
#endif

    // StringB
    ConvertIntegerToString(buffer, key2);
    len = StrLength(buffer);
    GenerateMD5FromString(buffer, len, &hash[0], &hash[1], &hash[2], &hash[3]);

#if !RETRO_USE_ORIGINAL_CODE
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) encryptionStringB[i * 4 + j] = (hash[i] >> (8 * (j ^ 3))) & 0xFF;
#else
    for (int y = 0; y < 0x10; y += 4) {
        encryptionStringB[y + 3] = hash[y + 0];
        encryptionStringB[y + 2] = hash[y + 1];
        encryptionStringB[y + 1] = hash[y + 2];
        encryptionStringB[y + 0] = hash[y + 3];
    }
#endif
}

const uint ENC_KEY_2 = 0x24924925;
const uint ENC_KEY_1 = 0xAAAAAAAB;
int mulUnsignedHigh(uint arg1, int arg2) { return (int)(((unsigned long long)arg1 * (unsigned long long)arg2) >> 32); }

void FileRead(void *dest, int size)
{
    byte *data = (byte *)dest;
    memset(data, 0, size);

    if (readPos <= fileSize) {
        if (useEncryption) {
            while (size > 0) {
                if (bufferPosition == readSize)
                    FillFileBuffer();

                *data = encryptionStringB[eStringPosB] ^ eStringNo ^ fileBuffer[bufferPosition++];
                if (eNybbleSwap)
                    *data = ((*data << 4) + (*data >> 4)) & 0xFF;
                *data ^= encryptionStringA[eStringPosA];

                ++eStringPosA;
                ++eStringPosB;
                if (eStringPosA <= 0x0F) {
                    if (eStringPosB > 0x0C) {
                        eStringPosB = 0;
                        eNybbleSwap ^= 0x01;
                    }
                }
                else if (eStringPosB <= 0x08) {
                    eStringPosA = 0;
                    eNybbleSwap ^= 0x01;
                }
                else {
                    eStringNo += 2;
                    eStringNo &= 0x7F;

                    if (eNybbleSwap != 0) {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 0;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosA = eStringNo - temp1 / 4 * 7;
                        eStringPosB = eStringNo - temp2 * 4 + 2;
                    }
                    else {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 1;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosB = eStringNo - temp1 / 4 * 7;
                        eStringPosA = eStringNo - temp2 * 4 + 3;
                    }
                }

                ++data;
                --size;
            }
        }
        else {
            while (size > 0) {
                if (bufferPosition == readSize)
                    FillFileBuffer();

                *data++ = fileBuffer[bufferPosition++];
                size--;
            }
        }
    }
}

void FileSkip(int count)
{
    if (readPos <= fileSize) {
        if (useEncryption) {
            while (count > 0) {
                if (bufferPosition == readSize)
                    FillFileBuffer();
                bufferPosition++;

                ++eStringPosA;
                ++eStringPosB;
                if (eStringPosA <= 0x0F) {
                    if (eStringPosB > 0x0C) {
                        eStringPosB = 0;
                        eNybbleSwap ^= 0x01;
                    }
                }
                else if (eStringPosB <= 0x08) {
                    eStringPosA = 0;
                    eNybbleSwap ^= 0x01;
                }
                else {
                    eStringNo += 2;
                    eStringNo &= 0x7F;

                    if (eNybbleSwap != 0) {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 0;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosA = eStringNo - temp1 / 4 * 7;
                        eStringPosB = eStringNo - temp2 * 4 + 2;
                    }
                    else {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 1;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosB = eStringNo - temp1 / 4 * 7;
                        eStringPosA = eStringNo - temp2 * 4 + 3;
                    }
                }

                --count;
            }
        }
        else {
            while (count > 0) {
                if (bufferPosition == readSize)
                    FillFileBuffer();
                bufferPosition++;
                count--;
            }
        }
    }
}

void GetFileInfo(FileInfo *fileInfo)
{
    StrCopy(fileInfo->fileName, fileName);
    fileInfo->bufferPosition    = bufferPosition;
    fileInfo->readPos           = readPos - readSize;
    fileInfo->fileSize          = fileSize;
    fileInfo->vfileSize         = vFileSize;
    fileInfo->virtualFileOffset = virtualFileOffset;
    fileInfo->eStringPosA       = eStringPosA;
    fileInfo->eStringPosB       = eStringPosB;
    fileInfo->eStringNo         = eStringNo;
    fileInfo->eNybbleSwap       = eNybbleSwap;
    fileInfo->useEncryption     = useEncryption;
    fileInfo->packID            = packID;
    fileInfo->usingDataPack     = Engine.usingDataFile;
    memcpy(encryptionStringA, fileInfo->encryptionStringA, 0x10 * sizeof(byte));
    memcpy(encryptionStringB, fileInfo->encryptionStringB, 0x10 * sizeof(byte));
}

void SetFileInfo(FileInfo *fileInfo)
{
#if !RETRO_USE_ORIGINAL_CODE
    if (fileInfo->usingDataPack) {
#else
    if (Engine.usingDataFile) {
#endif
        cFileHandle = fOpen(rsdkContainer.packNames[fileInfo->packID], "rb");
        if (cFileHandle) {
            virtualFileOffset = fileInfo->virtualFileOffset;
            vFileSize         = fileInfo->vfileSize;
            fSeek(cFileHandle, 0, SEEK_END);
            fileSize = (int)fTell(cFileHandle);
            readPos  = fileInfo->readPos;
            fSeek(cFileHandle, readPos, SEEK_SET);
            FillFileBuffer();
            bufferPosition       = fileInfo->bufferPosition;
            eStringPosA          = fileInfo->eStringPosA;
            eStringPosB          = fileInfo->eStringPosB;
            eStringNo            = fileInfo->eStringNo;
            eNybbleSwap          = fileInfo->eNybbleSwap;
            useEncryption        = fileInfo->useEncryption;
            packID               = fileInfo->packID;
            Engine.usingDataFile = fileInfo->usingDataPack;

            if (useEncryption) {
                GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
            }
        }
    }
    else {
        StrCopy(fileName, fileInfo->fileName);
        cFileHandle       = fOpen(fileInfo->fileName, "rb");
        virtualFileOffset = 0;
        fileSize          = fileInfo->fileSize;
        readPos           = fileInfo->readPos;
        fSeek(cFileHandle, readPos, SEEK_SET);
        FillFileBuffer();
        bufferPosition       = fileInfo->bufferPosition;
        eStringPosA          = 0;
        eStringPosB          = 0;
        eStringNo            = 0;
        eNybbleSwap          = 0;
        useEncryption        = fileInfo->useEncryption;
        packID               = fileInfo->packID;
        Engine.usingDataFile = fileInfo->usingDataPack;
    }
}

size_t GetFilePosition()
{
    if (Engine.usingDataFile)
        return bufferPosition + readPos - readSize - virtualFileOffset;
    else
        return bufferPosition + readPos - readSize;
}

void SetFilePosition(int newPos)
{
    if (useEncryption) {
        readPos     = virtualFileOffset + newPos;
        eStringNo   = (vFileSize & 0x1FC) >> 2;
        eStringPosA = 0;
        eStringPosB = 8;
        eNybbleSwap = false;
        while (newPos) {
            ++eStringPosA;
            ++eStringPosB;
            if (eStringPosA <= 0x0F) {
                if (eStringPosB > 0x0C) {
                    eStringPosB = 0;
                    eNybbleSwap ^= 0x01;
                }
            }
            else if (eStringPosB <= 0x08) {
                eStringPosA = 0;
                eNybbleSwap ^= 0x01;
            }
            else {
                eStringNo += 2;
                eStringNo &= 0x7F;

                if (eNybbleSwap != 0) {
                    int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    eNybbleSwap = 0;

                    int temp1 = key2 + (eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    eStringPosA = eStringNo - temp1 / 4 * 7;
                    eStringPosB = eStringNo - temp2 * 4 + 2;
                }
                else {
                    int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    eNybbleSwap = 1;

                    int temp1 = key2 + (eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    eStringPosB = eStringNo - temp1 / 4 * 7;
                    eStringPosA = eStringNo - temp2 * 4 + 3;
                }
            }
            --newPos;
        }
    }
    else {
        if (Engine.usingDataFile)
            readPos = virtualFileOffset + newPos;
        else
            readPos = newPos;
    }
    fSeek(cFileHandle, readPos, SEEK_SET);
    FillFileBuffer();
}

bool ReachedEndOfFile()
{
    if (Engine.usingDataFile)
        return bufferPosition + readPos - readSize - virtualFileOffset >= vFileSize;
    else
        return bufferPosition + readPos - readSize >= fileSize;
}
