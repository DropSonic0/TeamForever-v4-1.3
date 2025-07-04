#include "RetroEngine.hpp"

#if RETRO_USE_MOD_LOADER || !RETRO_USE_ORIGINAL_CODE
char savePath[0x100];
#endif

char playerNames[PLAYER_COUNT][0x20];
byte playerCount = 0;

#if RETRO_USE_MOD_LOADER
std::vector<ModInfo> modList;
int activeMod = -1;

char modsPath[0x100];

bool redirectSave = false;

char modTypeNames[OBJECT_COUNT][0x40];
char modScriptPaths[OBJECT_COUNT][0x40];
byte modScriptFlags[OBJECT_COUNT];
byte modObjCount = 0;

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h> // Para getcwd si fuera necesario, aunque intentaremos evitarlo.
// #include <filesystem> // Comentado
#include <locale>
#include <stdio.h>
#include <string.h>


// Helper function to join paths (simplistic)
// Asegúrate de que 'base' tenga suficiente espacio.
static void joinPath(char* destination, const char* base, const char* name) {
    strcpy(destination, base);
    // Asegurar que haya un separador si es necesario
    if (destination[strlen(destination) - 1] != '/') {
        strcat(destination, "/");
    }
    strcat(destination, name);
}


int OpenModMenu()
{
    Engine.gameMode      = ENGINE_INITMODMENU;
    Engine.modMenuCalled = true;
    return 1;
}

// #if (RETRO_PLATFORM == RETRO_ANDROID)
// namespace fs = std::__fs::filesystem; // this is so we can avoid using c++17, which causes a ton of warnings w asio and looks ugly
// #else
// namespace fs = std::filesystem;
// #endif

// fs::path resolvePath(fs::path given)
// {
// 	    // This crashes and I don't know why
//     // Maybe to do with pathconf somehow?
// #if RETRO_PLATFORM != RETRO_SWITCH
//     if (given.is_relative())
//         given = fs::current_path() / given; // thanks for the weird syntax!
// #endif
//     for (auto &p : fs::directory_iterator{ given.parent_path() }) {
//         char pbuf[0x100];
//         char gbuf[0x100];
//         auto pf   = p.path().filename();
//         auto pstr = pf.string();
//         StringLowerCase(pbuf, pstr.c_str());
//         auto gf   = given.filename();
//         auto gstr = gf.string();
//         StringLowerCase(gbuf, gstr.c_str());
//         if (StrComp(pbuf, gbuf)) {
//             return p.path();
//         }
//     }
//     return given; // might work might not!
// }


// Reimplementación de resolvePath y helpers POSIX
// Esta es una simplificación y puede necesitar ajustes para mayúsculas/minúsculas y errores.
bool pathExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool isDirectory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

// NOTA: resolvePath es complejo de replicar 1:1 sin std::filesystem debido a la
// normalización de rutas y la búsqueda insensible a mayúsculas/minúsculas.
// Por ahora, vamos a simplificarlo y asumir que las rutas son correctas.
// Si se necesita la funcionalidad completa, requerirá más trabajo.
// Esta función ahora simplemente devuelve la ruta dada si existe.
// Opcionalmente, se podría prefijar con current_path si es relativa, pero getcwd puede ser problemático.
const char* resolvePath(const char* given_path) {
    // Para PS3, es mejor trabajar con rutas absolutas o relativas a un BASE_PATH conocido.
    // No intentaremos resolver como en el original por ahora.
    return given_path; 
}


void InitMods()
{
    modList.clear();
    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");

    // modsPath DEBE estar inicializado a BASE_PATH ANTES de esta función.
    // Por ejemplo, en RefreshEngine o al inicio del programa.
    // Si modsPath no es global o persistente, se debe asegurar su valor aquí.
    // Ejemplo de inicialización única (si modsPath fuera estático o global y solo necesitara llenarse una vez):
    if (modsPath[0] == '\0') { // Solo si está vacío
        strcpy(modsPath, BASE_PATH); 
        if (modsPath[strlen(modsPath) - 1] == '/') {
            modsPath[strlen(modsPath) - 1] = '\0'; 
        }
    }

    char modRootPath[0x200]; // Ruta a la carpeta .../USRDIR/mods
    sprintf(modRootPath, "%s/mods", modsPath); // modsPath aquí es la raíz del USRDIR

    if (pathExists(modRootPath) && isDirectory(modRootPath)) {
        char mod_config_filepath[0x200];
        joinPath(mod_config_filepath, modRootPath, "modconfig.ini");
        
        FileIO *configFile = fOpen(mod_config_filepath, "r");
        if (configFile) {
            fClose(configFile);
            IniParser modConfig(mod_config_filepath, false);

            for (size_t m = 0; m < modConfig.items.size(); ++m) {
                bool active = false;
                ModInfo info;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                // Pass modRootPath (char[]) and modConfig.items[m].key (char*) directly
                if (LoadMod(&info, modRootPath, modConfig.items[m].key, active)) 
                    modList.push_back(info);
            }
        }

        DIR *dir = opendir(modRootPath);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue; // Ignorar . y .. y ocultos

                char fullEntryPath[0x200];
                joinPath(fullEntryPath, modRootPath, entry->d_name);

                if (isDirectory(fullEntryPath)) {
                    ModInfo info;
                    // entry->d_name is already char*
                    
                    bool alreadyLoaded = false;
                    for (size_t m = 0; m < modList.size(); ++m) {
                        if (modList[m].folder == entry->d_name) { // ModInfo::folder is std::string, comparison is fine
                            alreadyLoaded = true;
                            break;
                        }
                    }

                    if (!alreadyLoaded) {
                        // Pass modRootPath (char[]) and entry->d_name (char*) directly
                        if (LoadMod(&info, modRootPath, entry->d_name, false))
                            modList.insert(modList.begin(), info);
                    }
                }
            }
            closedir(dir);
        } else {
            PrintLog("Mods Folder Scanning Error: Could not open directory %s", modRootPath);
        }
    }

    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");
    for (int m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause |= modList[m].disableFocusPause;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
        if (modList[m].forceSonic1)
            Engine.forceSonic1 = true;
    }

    ReadSaveRAMData();
    ReadUserdata();
}

// Changed modsPath and folder to const char*
bool LoadMod(ModInfo *info, const char* baseModsPath, const char* folderName, bool active)
{
    if (!info)
        return false;

    info->fileMap.clear();
    info->name    = ""; // These will be std::string in ModInfo
    info->desc    = "";
    info->author  = "";
    info->version = "";
    info->folder  = folderName; // Store the folder name
    info->active  = false;

    char modDir[0x200];
    joinPath(modDir, baseModsPath, folderName); // Construct .../mods/ModFolder

    char modIniPath[0x300];
    joinPath(modIniPath, modDir, "mod.ini");

    FileIO *f = fOpen(modIniPath, "r");
    if (f) {
        fClose(f);
        IniParser modSettings(modIniPath, false);

        info->name    = "Unnamed Mod";
        info->desc    = "";
        info->author  = "Unknown Author";
        info->version = "1.0.0";
        // info->folder is already set

        char infoBuf[0x100];
        // Name
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Name", infoBuf);
        if (infoBuf[0] != '\0') // Check if string is not empty
            info->name = infoBuf;
        // Desc
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Description", infoBuf);
        if (infoBuf[0] != '\0')
            info->desc = infoBuf;
        // Author
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Author", infoBuf);
        if (infoBuf[0] != '\0')
            info->author = infoBuf;
        // Version
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Version", infoBuf);
        if (infoBuf[0] != '\0')
            info->version = infoBuf;

        info->active = active;

        ScanModFolder(info); // ScanModFolder expects info->folder to be set

        info->useScripts = false;
        modSettings.GetBool("", "TxtScripts", &info->useScripts);
        if (info->useScripts && info->active)
            forceUseScripts = true;

        info->skipStartMenu = false;
        modSettings.GetBool("", "SkipStartMenu", &info->skipStartMenu);
        if (info->skipStartMenu && info->active)
            skipStartMenu = true;

        info->disableFocusPause = 0; // Ensure it's an int
        modSettings.GetInteger("", "DisableFocusPause", &info->disableFocusPause);
        if (info->disableFocusPause && info->active)
            disableFocusPause |= info->disableFocusPause;

        info->redirectSave = false;
        modSettings.GetBool("", "RedirectSaveRAM", &info->redirectSave);
        if (info->redirectSave) {
            char pathStr[0x100];
            sprintf(pathStr, "mods/%s/", folderName); // Use folderName (const char*)
            info->savePath = pathStr; // ModInfo::savePath is std::string
        }

        info->forceSonic1 = false;
        modSettings.GetBool("", "ForceSonic1", &info->forceSonic1);
        if (info->forceSonic1 && info->active)
            Engine.forceSonic1 = true;

        return true;
    }
    // If mod.ini doesn't exist, we can still treat it as a mod folder, just without metadata.
    // Or decide to return false if mod.ini is mandatory. For now, let's allow it.
    info->folder = folderName;
    info->active = active; // It might be active from modconfig.ini
    ScanModFolder(info); // Still scan its contents
    // Default values for metadata were already set or will remain empty if mod.ini is not found.
    // Set a default name if none was loaded
    if (info->name.empty() || info->name == "Unnamed Mod") {
        char tempName[0x100];
        sprintf(tempName, "%s (No mod.ini)", folderName);
        info->name = tempName;
    }
    return true; // Return true even if mod.ini is not found, as long as the folder exists.
                 // The calling function in InitMods checks if it's already loaded.
}

// POSIX-compliant recursive directory scanning
void ScanModSubdirectory(ModInfo *info, const char* modTrueRootPath, const char* scanTypeFolder, const char* currentSubdirRel) {
    char currentFullPath[0x300]; // Increased buffer size
    joinPath(currentFullPath, modTrueRootPath, scanTypeFolder);
    if (currentSubdirRel[0] != '\0') {
        joinPath(currentFullPath, currentFullPath, currentSubdirRel);
    }

    DIR *dir = opendir(currentFullPath);
    if (!dir) {
        // It's okay if a subfolder like "Data" doesn't exist.
        // PrintLog("ScanModSubdirectory: Could not open directory %s", currentFullPath);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Skip . and ..

        char entryFullAbsPath[0x400]; // Increased buffer size
        joinPath(entryFullAbsPath, currentFullPath, entry->d_name);

        struct stat st;
        if (stat(entryFullAbsPath, &st) == 0) {
            char entryRelPath[0x300]; // Path relative to scanTypeFolder (e.g., "Sprites/Sonic.png")
            if (currentSubdirRel[0] == '\0') {
                strcpy(entryRelPath, entry->d_name);
            } else {
                joinPath(entryRelPath, currentSubdirRel, entry->d_name);
            }

            if (S_ISDIR(st.st_mode)) {
                ScanModSubdirectory(info, modTrueRootPath, scanTypeFolder, entryRelPath);
            } else if (S_ISREG(st.st_mode)) {
                char mapKey[0x400]; // Key for the fileMap (e.g., "data/sprites/sonic.png")
                
                // Construct the key: scanTypeFolder + / + entryRelPath
                // Ensure scanTypeFolder is lowercased for the key part
                char scanTypeFolderLower[0x100];
                StringLowerCase(scanTypeFolderLower, scanTypeFolder);
                joinPath(mapKey, scanTypeFolderLower, entryRelPath);

                // Normalize path separators in mapKey to '/' and lowercase the whole key
                for (char *p = mapKey; *p; ++p) {
                    if (*p == '\\') *p = '/';
                    *p = tolower(*p);
                }
                info->fileMap[mapKey] = entryFullAbsPath;
                // PrintLog("Mapped: %s -> %s", mapKey, entryFullAbsPath);
            }
        } else {
            PrintLog("ScanModSubdirectory: stat failed for %s", entryFullAbsPath);
        }
    }
    closedir(dir);
}


void ScanModFolder(ModInfo *info)
{
    if (!info) return;

    info->fileMap.clear();

    // Construct the absolute path to the root of the specific mod's folder
    // modsPath should be the USRDIR (e.g. /dev_hdd0/game/S1F00S2A0/USRDIR)
    // info->folder is the mod's folder name (e.g., "MyMod")
    char modTrueRootPath[0x200];
    joinPath(modTrueRootPath, modsPath, "mods"); // Should be .../USRDIR/mods
    joinPath(modTrueRootPath, modTrueRootPath, info->folder.c_str()); // Should be .../USRDIR/mods/MyMod

    // Scan Data, Scripts, and Bytecode subdirectories
    ScanModSubdirectory(info, modTrueRootPath, "Data", "");
    ScanModSubdirectory(info, modTrueRootPath, "Scripts", "");
    ScanModSubdirectory(info, modTrueRootPath, "Bytecode", "");
}

void SaveMods()
{
    char modConfigDir[0x200];
    joinPath(modConfigDir, modsPath, "mods"); // .../USRDIR/mods

    // On PS3, we assume the mods directory exists if we're trying to save.
    // mkdir might not be available or desired here.
    // if (!pathExists(modConfigDir)) {
    //    #if defined(_WIN32)
    //        _mkdir(modConfigDir);
    //    #else
    //        mkdir(modConfigDir, 0777); // POSIX
    //    #endif
    // }

    if (pathExists(modConfigDir) && isDirectory(modConfigDir)) {
        char mod_config_filepath[0x300];
        joinPath(mod_config_filepath, modConfigDir, "modconfig.ini");
        
        IniParser modConfig; // Create new or overwrite existing

        for (size_t m = 0; m < modList.size(); ++m) {
            ModInfo *info = &modList[m];
            modConfig.SetBool("mods", info->folder.c_str(), info->active);
        }
        modConfig.Write(mod_config_filepath, false); // Write it out
    } else {
        PrintLog("SaveMods: Mods directory %s does not exist or is not a directory. Cannot save modconfig.ini.", modConfigDir);
    }
}

void RefreshEngine()
{
    // Reload entire engine
    Engine.LoadGameConfig("Data/Game/GameConfig.bin");
#if RETRO_USING_SDL2
    if (Engine.window) {
        char gameTitle[0x40];
        sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile_Config ? "" : "");
        SDL_SetWindowTitle(Engine.window, gameTitle);
    }
#elif RETRO_USING_SDL1
    char gameTitle[0x40];
    sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile_Config ? "" : "");
    SDL_WM_SetCaption(gameTitle, NULL);
#endif

    ClearMeshData();
    ClearTextures(true);

    nativeEntityCountBackup = 0;
    memset(backupEntityList, 0, sizeof(backupEntityList));
    memset(objectEntityBackup, 0, sizeof(objectEntityBackup));

    nativeEntityCountBackupS = 0;
    memset(backupEntityListS, 0, sizeof(backupEntityListS));
    memset(objectEntityBackupS, 0, sizeof(objectEntityBackupS));

    for (int i = 0; i < FONTLIST_COUNT; ++i) {
        fontList[i].count = 2;
    }

    ReleaseStageSfx();
    ReleaseGlobalSfx();
    LoadGlobalSfx();
    InitLocalizedStrings();

    for (nativeEntityPos = 0; nativeEntityPos < nativeEntityCount; ++nativeEntityPos) {
        NativeEntity *entity = &objectEntityBank[activeEntityList[nativeEntityPos]];
        entity->eventCreate(entity);
    }

    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");
    for (int m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause |= modList[m].disableFocusPause;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
        if (modList[m].forceSonic1)
            Engine.forceSonic1 = true;
    }

    Engine.gameType = GAME_SONIC2;
    if (strstr(Engine.gameWindowText, "Sonic 1") || Engine.forceSonic1) {
        Engine.gameType = GAME_SONIC1;
    }

    achievementCount = 0;
    if (Engine.gameType == GAME_SONIC1) {
        AddAchievement("Ramp Ring Acrobatics",
                       "Without touching the ground,\rcollect all the rings in a\rtrapezoid formation in Green\rHill Zone Act 1");
        AddAchievement("Blast Processing", "Clear Green Hill Zone Act 1\rin under 30 seconds");
        AddAchievement("Secret of Marble Zone", "Travel though a secret\rroom in Marbale Zone Act 3");
        AddAchievement("Block Buster", "Break 16 blocks in a row\rwithout stopping");
        AddAchievement("Ring King", "Collect 200 Rings");
        AddAchievement("Secret of Labyrinth Zone", "Activate and ride the\rhidden platform in\rLabyrinth Zone Act 1");
        AddAchievement("Flawless Pursuit", "Clear the boss in Labyrinth\rZone without getting hurt");
        AddAchievement("Bombs Away", "Defeat the boss in Starlight Zone\rusing only the see-saw bombs");
        AddAchievement("Hidden Transporter", "Collect 50 Rings and take the hidden transporter path\rin Scrap Brain Act 2");
        AddAchievement("Chaos Connoisseur", "Collect all the chaos\remeralds");
        AddAchievement("One For the Road", "As a parting gift, land a\rfinal hit on Dr. Eggman's\rescaping Egg Mobile");
        AddAchievement("Beat The Clock", "Clear the Time Attack\rmode in less than 45\rminutes");
    }
    else if (Engine.gameType == GAME_SONIC2) {
        AddAchievement("Quick Run", "Complete Emerald Hill\rZone Act 1 in under 35\rseconds");
        AddAchievement("100% Chemical Free", "Complete Chemical Plant\rwithout going underwater");
        AddAchievement("Early Bird Special", "Collect all the Chaos\rEmeralds before Chemical\rPlant");
        AddAchievement("Superstar", "Complete any Act as\rSuper Sonic");
        AddAchievement("Hit it Big", "Get a jackpot on the Casino Night slot machines");
        AddAchievement("Bop Non-stop", "Defeat any boss in 8\rconsecutive hits without\rtouching he ground");
        AddAchievement("Perfectionist", "Get a Perfect Bonus by\rcollecting every Ring in an\rAct");
        AddAchievement("A Secret Revealed", "Find and complete\rHidden Palace Zone");
        AddAchievement("Head 2 Head", "Win a 2P Versus race\ragainst a friend");
        AddAchievement("Metropolis Master", "Complete Any Metropolis\rZone Act without getting\rhurt");
        AddAchievement("Scrambled Egg", "Defeat Dr. Eggman's Boss\rAttack mode in under 7\rminutes");
        AddAchievement("Beat the Clock", "Complete the Time Attack\rmode in less than 45\rminutes");
    }

    SaveMods();

    ReadSaveRAMData();
    ReadUserdata();
}

void GetModCount() { scriptEng.checkResult = (int)modList.size(); }
void GetModName(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].name.c_str());
}

void GetModDescription(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].desc.c_str());
}

void GetModAuthor(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].author.c_str());
}

void GetModVersion(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].version.c_str());
}

void GetModActive(uint *id, int *unused)
{
    scriptEng.checkResult = false;
    if (*id >= modList.size())
        return;
    scriptEng.checkResult = modList[*id].active;
}

void SetModActive(uint *id, int *active)
{
    if (*id >= modList.size())
        return;

    modList[*id].active = *active;
}

void MoveMod(uint *id, int *up)
{
    if (!id)
        return;

    int preOption = *id;
    int option    = preOption + (*up ? -1 : 1);
    if (option < 0 || preOption < 0)
        return;

    if (option >= (int)modList.size() || preOption >= (int)modList.size())
        return;

    ModInfo swap       = modList[preOption];
    modList[preOption] = modList[option];
    modList[option]    = swap;
}

#endif

#if RETRO_USE_MOD_LOADER || !RETRO_USE_ORIGINAL_CODE
int GetSceneID(byte listID, const char *sceneName)
{
    if (listID >= 3)
        return -1;

    char scnName[0x40];
    int scnPos = 0;
    int pos    = 0;
    while (sceneName[scnPos]) {
        if (sceneName[scnPos] != ' ')
            scnName[pos++] = sceneName[scnPos];
        ++scnPos;
    }
    scnName[pos] = 0;

    for (int s = 0; s < stageListCount[listID]; ++s) {
        char nameBuffer[0x40];

        scnPos = 0;
        pos    = 0;
        while (stageList[listID][s].name[scnPos]) {
            if (stageList[listID][s].name[scnPos] != ' ')
                nameBuffer[pos++] = stageList[listID][s].name[scnPos];
            ++scnPos;
        }
        nameBuffer[pos] = 0;

        if (StrComp(scnName, nameBuffer)) {
            return s;
        }
    }
    return -1;
}
#endif
