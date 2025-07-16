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
    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");

    char modBuf[0x100];
    sprintf(modBuf, "%s/mods", modsPath);

    if (ModPathExists(modBuf) && IsDirectory(modBuf)) {
        std::string mod_config = std::string(modBuf) + "/modconfig.ini";
        FileIO *configFile     = fOpen(mod_config.c_str(), "r");
        if (configFile) {
            fClose(configFile);
            IniParser modConfig(mod_config.c_str(), false);

            for (int m = 0; m < modConfig.items.size(); ++m) {
                bool active = false;
                ModInfo info;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                if (LoadMod(&info, modBuf, modConfig.items[m].key, active))
                    modList.push_back(info);
            }
        }

        DIR *dir = opendir(modBuf);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;

                std::string modDirPath = std::string(modBuf) + "/" + entry->d_name;
                if (IsDirectory(modDirPath.c_str())) {
                    ModInfo info;
                    std::string folder = entry->d_name;

                    bool flag = true;
                    for (int m = 0; m < modList.size(); ++m) {
                        if (modList[m].folder == folder) {
                            flag = false;
                            break;
                        }
                    }

                    if (flag) {
                        if (LoadMod(&info, modBuf, folder.c_str(), false))
                            modList.insert(modList.begin(), info);
                    }
                }
            }
            closedir(dir);
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

bool LoadMod(ModInfo *info, const char *modsPath, const char *folder, bool active)
{
    if (!info)
        return false;

    info->fileMap.clear();
    info->name    = "";
    info->desc    = "";
    info->author  = "";
    info->version = "";
    info->folder  = "";
    info->active  = false;

    std::string modDir = std::string(modsPath) + "/" + folder;

    FileIO *f = fOpen((modDir + "/mod.ini").c_str(), "r");
    if (f) {
        fClose(f);
        IniParser modSettings((modDir + "/mod.ini").c_str(), false);

        info->name    = "Unnamed Mod";
        info->desc    = "";
        info->author  = "Unknown Author";
        info->version = "1.0.0";
        info->folder  = folder;

        char infoBuf[0x100];
        // Name
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Name", infoBuf);
        if (StrLength(infoBuf))
            info->name = infoBuf;
        // Desc
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Description", infoBuf);
        if (StrLength(infoBuf))
            info->desc = infoBuf;
        // Author
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Author", infoBuf);
        if (StrLength(infoBuf))
            info->author = infoBuf;
        // Version
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Version", infoBuf);
        if (StrLength(infoBuf))
            info->version = infoBuf;

        info->active = active;

        ScanModFolder(info);

        info->useScripts = false;
        modSettings.GetBool("", "TxtScripts", &info->useScripts);
        if (info->useScripts && info->active)
            forceUseScripts = true;

        info->skipStartMenu = false;
        modSettings.GetBool("", "SkipStartMenu", &info->skipStartMenu);
        if (info->skipStartMenu && info->active)
            skipStartMenu = true;

        info->disableFocusPause = false;
        modSettings.GetInteger("", "DisableFocusPause", &info->disableFocusPause);
        if (info->disableFocusPause && info->active)
            disableFocusPause |= info->disableFocusPause;

        info->redirectSave = false;
        modSettings.GetBool("", "RedirectSaveRAM", &info->redirectSave);
        if (info->redirectSave) {
            char path[0x100];
            sprintf(path, "mods/%s/", folder);
            info->savePath = path;
        }

        info->forceSonic1 = false;
        modSettings.GetBool("", "ForceSonic1", &info->forceSonic1);
        if (info->forceSonic1 && info->active)
            Engine.forceSonic1 = true;

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
                std::string entryFullPath = fullPath + "/" + entry->d_name;

                if (IsDirectory(entryFullPath.c_str())) {
                    ScanModFolderSub(info, modDir, path.c_str());
                }
                else {
                    std::string modPath = entryFullPath;
                    char pathLower[0x100];
                    memset(pathLower, 0, sizeof(char) * 0x100);
                    for (int c = 0; c < path.size(); ++c) {
                        pathLower[c] = tolower(path.c_str()[c]);
                    }
                    info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modPath));
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

        for (int m = 0; m < modList.size(); ++m) {
            ModInfo *info = &modList[m];
            modConfig.SetBool("mods", info->folder.c_str(), info->active);
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
                       "Without touching the ground,collect all the rings in atrapezoid formation in GreenHill Zone Act 1");
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