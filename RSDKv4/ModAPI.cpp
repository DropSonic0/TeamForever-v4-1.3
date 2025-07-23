#include "RetroEngine.hpp"

#if RETRO_USE_MOD_LOADER || !RETRO_USE_ORIGINAL_CODE
char savePath[0x100];
#endif

char playerNames[PLAYER_COUNT][0x20];
byte playerCount = 0;

#if RETRO_USE_MOD_LOADER
#include <sys/stat.h>
#include <dirent.h>

std::vector<ModInfo> modList;
int activeMod = -1;

char modsPath[0x100];

bool redirectSave = false;

char modTypeNames[OBJECT_COUNT][0x40];
char modScriptPaths[OBJECT_COUNT][0x40];
byte modScriptFlags[OBJECT_COUNT];
byte modObjCount = 0;

int OpenModMenu()
{
    Engine.gameMode      = ENGINE_INITMODMENU;
    Engine.modMenuCalled = true;
    return 1;
}

// Helper function to check if a file/directory exists
bool ModPathExists(const char *path)
{
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

// Helper function to check if a path is a directory
bool IsDirectory(const char *path)
{
    struct stat buffer;
    if (stat(path, &buffer) != 0)
        return false;
    return S_ISDIR(buffer.st_mode);
}

void InitMods()
{
    modList.clear();

    char modBuf[0x100];
    sprintf(modBuf, "%s/mods", modsPath);

    if (ModPathExists(modBuf) && IsDirectory(modBuf)) {
        std::string mod_config = std::string(modBuf) + "/modconfig.ini";
        FileIO *configFile     = fOpen(mod_config.c_str(), "r");
        if (configFile) {
            fClose(configFile);
            IniParser modConfig(mod_config.c_str(), false);

            for (const auto& item : modConfig.items) {
                bool active = false;
                ModInfo info;
                modConfig.GetBool("mods", item.key, &active);
                if (LoadMod(&info, modBuf, item.key, active))
                    modList.push_back(info);
            }
        }

        DIR *dir = opendir(modBuf);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;

                bool isDir = entry->d_type == DT_DIR;
                if (entry->d_type == DT_UNKNOWN) {
                    std::string modDirPath = std::string(modBuf) + "/" + entry->d_name;
                    isDir = IsDirectory(modDirPath.c_str());
                }

                if (isDir) {
                    ModInfo info;
                    std::string folder = entry->d_name;

                    bool found = false;
                    for (const auto& mod : modList) {
                        if (mod.folder == folder) {
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        if (LoadMod(&info, modBuf, folder.c_str(), false))
                            modList.insert(modList.begin(), info);
                    }
                }
            }
            closedir(dir);
        }
    }

    RefreshEngine();
}

bool LoadMod(ModInfo *info, const char *modsPath, const char *folder, bool active)
{
    if (!info)
        return false;

    info->fileMap.clear();
    info->name    = "Unnamed Mod";
    info->desc    = "";
    info->author  = "Unknown Author";
    info->version = "1.0.0";
    info->folder  = folder;
    info->active  = active;

    std::string modDir = std::string(modsPath) + "/" + folder;

    FileIO *f = fOpen((modDir + "/mod.ini").c_str(), "r");
    if (f) {
        fClose(f);
        IniParser modSettings((modDir + "/mod.ini").c_str(), false);

        char infoBuf[0x100];
        modSettings.GetString("", "Name", infoBuf);
        if (StrLength(infoBuf))
            info->name = infoBuf;

        modSettings.GetString("", "Description", infoBuf);
        if (StrLength(infoBuf))
            info->desc = infoBuf;

        modSettings.GetString("", "Author", infoBuf);
        if (StrLength(infoBuf))
            info->author = infoBuf;

        modSettings.GetString("", "Version", infoBuf);
        if (StrLength(infoBuf))
            info->version = infoBuf;

        ScanModFolder(info);

        modSettings.GetBool("", "TxtScripts", &info->useScripts);
        modSettings.GetBool("", "SkipStartMenu", &info->skipStartMenu);
        modSettings.GetInteger("", "DisableFocusPause", &info->disableFocusPause);
        modSettings.GetBool("", "RedirectSaveRAM", &info->redirectSave);
        if (info->redirectSave) {
            char path[0x100];
            sprintf(path, "mods/%s/", folder);
            info->savePath = path;
        }
        modSettings.GetBool("", "ForceSonic1", &info->forceSonic1);

        return true;
    }
    return false;
}

void ScanModFolderSub(ModInfo *info, const char *modDir, const char *folder)
{
    std::string fullPath = std::string(modDir) + "/" + folder;
    if (ModPathExists(fullPath.c_str()) && IsDirectory(fullPath.c_str())) {
        DIR *dir = opendir(fullPath.c_str());
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;

                std::string path = std::string(folder) + "/" + entry->d_name;
                
                bool isDir = entry->d_type == DT_DIR;
                if (entry->d_type == DT_UNKNOWN) {
                    std::string entryFullPath = fullPath + "/" + entry->d_name;
                    isDir = IsDirectory(entryFullPath.c_str());
                }

                if (isDir) {
                    ScanModFolderSub(info, modDir, path.c_str());
                }
                else {
                    std::string modPath = fullPath + "/" + entry->d_name;
                    char pathLower[0x100] = {0};
                    for (size_t c = 0; c < path.size(); ++c) {
                        pathLower[c] = tolower(path.c_str()[c]);
                    }
                    info->fileMap[pathLower] = modPath;
                }
            }
            closedir(dir);
        }
    }
}

void ScanModFolder(ModInfo *info)
{
    if (!info)
        return;

    char modBuf[0x100];
    sprintf(modBuf, "%s/mods", modsPath);

    const std::string modDir = std::string(modBuf) + "/" + info->folder;

    info->fileMap.clear();

    ScanModFolderSub(info, modDir.c_str(), "Data");
    ScanModFolderSub(info, modDir.c_str(), "Scripts");
    ScanModFolderSub(info, modDir.c_str(), "Bytecode");
}

void SaveMods()
{
    char modBuf[0x100];
    sprintf(modBuf, "%s/mods", modsPath);

    if (ModPathExists(modBuf) && IsDirectory(modBuf)) {
        std::string mod_config = std::string(modBuf) + "/modconfig.ini";
        IniParser modConfig;

        for (const auto& info : modList) {
            modConfig.SetBool("mods", info.folder.c_str(), info.active);
        }
        modConfig.Write(mod_config.c_str(), false);
    }
}

void RefreshEngine()
{
    // Reload entire engine
    Engine.LoadGameConfig("Data/Game/GameConfig.bin");
#if RETRO_USING_SDL2
    if (Engine.window) {
        char gameTitle[0x40];
        sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile_Config ? "" : " - DECOMPLUS");
        SDL_SetWindowTitle(Engine.window, gameTitle);
    }
#elif RETRO_USING_SDL1
    char gameTitle[0x40];
    sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile_Config ? "" : " - DECOMPLUS");
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
    for (const auto& mod : modList) {
        if (!mod.active)
            continue;
        forceUseScripts |= mod.useScripts;
        skipStartMenu |= mod.skipStartMenu;
        disableFocusPause |= mod.disableFocusPause;
        if (mod.redirectSave) {
            sprintf(savePath, "%s", mod.savePath.c_str());
            redirectSave = true;
        }
        Engine.forceSonic1 |= mod.forceSonic1;
    }

    Engine.gameType = (strstr(Engine.gameWindowText, "Sonic 1") || Engine.forceSonic1) ? GAME_SONIC1 : GAME_SONIC2;

    achievementCount = 0;
    if (Engine.gameType == GAME_SONIC1) {
        AddAchievement("Ramp Ring Acrobatics", "Without touching the ground,collect all the rings in atrapezoid formation in GreenHill Zone Act 1");
        AddAchievement("Blast Processing", "Clear Green Hill Zone Act 1in under 30 seconds");
        AddAchievement("Secret of Marble Zone", "Travel though a secretroom in Marbale Zone Act 3");
        AddAchievement("Block Buster", "Break 16 blocks in a rowwithout stopping");
        AddAchievement("Ring King", "Collect 200 Rings");
        AddAchievement("Secret of Labyrinth Zone", "Activate and ride thehidden platform inLabyrinth Zone Act 1");
        AddAchievement("Flawless Pursuit", "Clear the boss in LabyrinthZone without getting hurt");
        AddAchievement("Bombs Away", "Defeat the boss in Starlight Zoneusing only the see-saw bombs");
        AddAchievement("Hidden Transporter", "Collect 50 Rings and take the hidden transporter pathin Scrap Brain Act 2");
        AddAchievement("Chaos Connoisseur", "Collect all the chaosemeralds");
        AddAchievement("One For the Road", "As a parting gift, land afinal hit on Dr. Eggman'sescaping Egg Mobile");
        AddAchievement("Beat The Clock", "Clear the Time Attackmode in less than 45minutes");
    }
    else if (Engine.gameType == GAME_SONIC2) {
        AddAchievement("Quick Run", "Complete Emerald HillZone Act 1 in under 35seconds");
        AddAchievement("100% Chemical Free", "Complete Chemical Plantwithout going underwater");
        AddAchievement("Early Bird Special", "Collect all the ChaosEmeralds before ChemicalPlant");
        AddAchievement("Superstar", "Complete any Act asSuper Sonic");
        AddAchievement("Hit it Big", "Get a jackpot on the Casino Night slot machines");
        AddAchievement("Bop Non-stop", "Defeat any boss in 8consecutive hits withouttouching he ground");
        AddAchievement("Perfectionist", "Get a Perfect Bonus bycollecting every Ring in anAct");
        AddAchievement("A Secret Revealed", "Find and completeHidden Palace Zone");
        AddAchievement("Head 2 Head", "Win a 2P Versus raceagainst a friend");
        AddAchievement("Metropolis Master", "Complete Any MetropolisZone Act without gettinghurt");
        AddAchievement("Scrambled Egg", "Defeat Dr. Eggman's BossAttack mode in under 7minutes");
        AddAchievement("Beat the Clock", "Complete the Time Attackmode in less than 45minutes");
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
    scriptEng.checkResult = (*id < modList.size()) ? modList[*id].active : false;
}

void SetModActive(uint *id, int *active)
{
    if (*id < modList.size())
        modList[*id].active = *active;
}

void MoveMod(uint *id, int *up)
{
    if (!id || *id >= modList.size())
        return;

    int option = *id + (*up ? -1 : 1);
    if (option < 0 || option >= (int)modList.size())
        return;

    std::swap(modList[*id], modList[option]);
}

#endif

#if RETRO_USE_MOD_LOADER || !RETRO_USE_ORIGINAL_CODE
int GetSceneID(byte listID, const char *sceneName)
{
    if (listID >= 3)
        return -1;

    char scnName[0x40] = {0};
    int pos = 0;
    for (int i = 0; sceneName[i]; ++i) {
        if (sceneName[i] != ' ')
            scnName[pos++] = sceneName[i];
    }

    for (int s = 0; s < stageListCount[listID]; ++s) {
        char nameBuffer[0x40] = {0};
        pos = 0;
        for (int i = 0; stageList[listID][s].name[i]; ++i) {
            if (stageList[listID][s].name[i] != ' ')
                nameBuffer[pos++] = stageList[listID][s].name[i];
        }

        if (StrComp(scnName, nameBuffer)) {
            return s;
        }
    }
    return -1;
}
#endif