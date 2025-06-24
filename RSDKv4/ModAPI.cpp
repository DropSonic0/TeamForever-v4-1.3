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

#ifndef PS3
#include <filesystem>
#include <locale>
#else // PS3
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h> // For getcwd
#include <string.h> // For C string manipulation
#include <stdio.h>  // For sprintf
#include <vector>
#include <string>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// PS3 Helper Functions
inline bool ps3_path_exists(const std::string& path_str) {
    struct stat buffer;
    return (stat(path_str.c_str(), &buffer) == 0);
}

inline bool ps3_is_directory(const std::string& path_str) {
    struct stat buffer;
    if (stat(path_str.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISDIR(buffer.st_mode);
}

inline bool ps3_is_regular_file(const std::string& path_str) {
    struct stat buffer;
    if (stat(path_str.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISREG(buffer.st_mode);
}

inline std::string ps3_get_filename(const std::string& path_str) {
    size_t last_slash = path_str.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        return path_str.substr(last_slash + 1);
    }
    return path_str;
}

inline std::string ps3_get_parent_path(const std::string& path_str) {
    size_t last_slash = path_str.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        return path_str.substr(0, last_slash);
    }
    return ""; // Or "." for current directory, depending on desired behavior
}

inline std::string ps3_join_path(const std::string& p1, const std::string& p2) {
    if (p1.empty()) return p2;
    if (p2.empty()) return p1;

    std::string result = p1;
    // Ensure there's a single separator
    if (result.back() != '/' && result.back() != '\\') {
        result += '/';
    }
    if (p2.front() == '/' || p2.front() == '\\') {
        result += p2.substr(1);
    } else {
        result += p2;
    }
    return result;
}

struct PS3DirEntry {
    std::string path_str;
    std::string name;
    bool is_dir;
    bool is_file;
};

inline std::vector<PS3DirEntry> ps3_directory_iterator(const std::string& dir_path_str) {
    std::vector<PS3DirEntry> entries;
    DIR* dir = opendir(dir_path_str.c_str());
    if (dir == NULL) {
        // perror("opendir failed"); // Optional: error logging
        return entries;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string entry_name = entry->d_name;
        if (entry_name == "." || entry_name == "..") {
            continue;
        }

        PS3DirEntry de;
        de.name = entry_name;
        de.path_str = ps3_join_path(dir_path_str, entry_name);

        struct stat entry_stat;
        if (stat(de.path_str.c_str(), &entry_stat) == 0) {
            de.is_dir = S_ISDIR(entry_stat.st_mode);
            de.is_file = S_ISREG(entry_stat.st_mode);
        } else {
            #ifdef _DIRENT_HAVE_D_TYPE
            de.is_dir = (entry->d_type == DT_DIR);
            de.is_file = (entry->d_type == DT_REG);
            #else
            de.is_dir = false;
            de.is_file = false;
            #endif
        }
        entries.push_back(de);
    }
    closedir(dir);
    return entries;
}

inline void ps3_recursive_directory_iterator_impl(const std::string& dir_path_str, std::vector<PS3DirEntry>& all_entries) {
    std::vector<PS3DirEntry> current_level_entries = ps3_directory_iterator(dir_path_str);
    for (const auto& de : current_level_entries) {
        all_entries.push_back(de); // Add current entry
        if (de.is_dir) {
            ps3_recursive_directory_iterator_impl(de.path_str, all_entries); // Recurse
        }
    }
}

inline std::vector<PS3DirEntry> ps3_recursive_directory_iterator(const std::string& dir_path_str) {
    std::vector<PS3DirEntry> all_entries;
    ps3_recursive_directory_iterator_impl(dir_path_str, all_entries);
    return all_entries;
}

// PS3 version of resolvePath (basic, case-sensitive)
std::string resolvePath_ps3(const std::string& given_str) {
    if (given_str.empty()) {
        return "";
    }
    if (given_str[0] == '/') {
        std::string temp = given_str;
        while (temp.length() > 1 && temp.back() == '/') {
            temp.pop_back();
        }
        return temp;
    }

    char cwd_buf[PATH_MAX];
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != NULL) {
        std::string current_dir = cwd_buf;
        return ps3_join_path(current_dir, given_str);
    }
    return given_str; 
}

#endif // !PS3

int OpenModMenu()
{
    Engine.gameMode      = ENGINE_INITMODMENU;
    Engine.modMenuCalled = true;
    return 1;
}

#if (RETRO_PLATFORM == RETRO_ANDROID)
#ifndef PS3
#if (RETRO_PLATFORM == RETRO_ANDROID)
namespace fs = std::__fs::filesystem; 
#else
namespace fs = std::filesystem;
#endif

fs::path resolvePath(fs::path given)
{
#if RETRO_PLATFORM != RETRO_SWITCH
    if (given.is_relative())
        given = fs::current_path() / given; 
#endif
    if (given.has_parent_path() && given.parent_path() != given.root_path() && fs::exists(given.parent_path())) {
        for (auto &p : fs::directory_iterator{ given.parent_path() }) {
            char pbuf[0x100];
            char gbuf[0x100];
            auto pf   = p.path().filename();
            auto pstr = pf.string();
            StringLowerCase(pbuf, pstr.c_str());
            auto gf   = given.filename();
            auto gstr = gf.string();
            StringLowerCase(gbuf, gstr.c_str());
            if (StrComp(pbuf, gbuf)) {
                return p.path();
            }
        }
    }
    return given; 
}
#endif 
#endif 

void InitMods()
{
    modList.clear();
    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");

    PrintLog("InitMods: Mod loading sequence started."); 
    char modConfigDirBuf[0x100];
    sprintf(modConfigDirBuf, "%smods", modsPath);
    PrintLog("InitMods: Mods directory should be: %s", modConfigDirBuf); 

#ifdef PS3
    std::string mods_base_dir_str = resolvePath_ps3(modConfigDirBuf);
    PrintLog("InitMods (PS3): Resolved mods base directory to: %s", mods_base_dir_str.c_str()); 
    if (ps3_path_exists(mods_base_dir_str) && ps3_is_directory(mods_base_dir_str)) {
        PrintLog("InitMods (PS3): Mods base directory exists and is a directory."); 
        std::string mod_config_path_str = ps3_join_path(mods_base_dir_str, "modconfig.ini");
        PrintLog("InitMods (PS3): Looking for modconfig.ini at: %s", mod_config_path_str.c_str()); 
        FileIO *configFile = fOpen(mod_config_path_str.c_str(), "r");
        if (configFile) {
            PrintLog("InitMods (PS3): Found modconfig.ini."); 
            fClose(configFile);
            IniParser modConfig(mod_config_path_str.c_str(), false);
            for (size_t m = 0; m < modConfig.items.size(); ++m) {
                bool active = false;
                ModInfo info;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                if (LoadMod(&info, mods_base_dir_str, modConfig.items[m].key, active))
                    modList.push_back(info);
            }
        } else { 
            PrintLog("InitMods (PS3): modconfig.ini not found at: %s", mod_config_path_str.c_str()); 
        }

        PrintLog("InitMods (PS3): Scanning for individual mod folders in: %s", mods_base_dir_str.c_str()); 
        std::vector<PS3DirEntry> dir_entries = ps3_directory_iterator(mods_base_dir_str);
        for (const auto& de : dir_entries) {
            if (de.is_dir) {
                PrintLog("InitMods (PS3): Found potential mod directory: %s", de.name.c_str()); 
                ModInfo info;
                std::string folder_name = de.name;
                bool flag = true;
                for (size_t m = 0; m < modList.size(); ++m) {
                    if (modList[m].folder == folder_name) {
                        flag = false;
                        break;
                    }
                }
                if (flag) {
                    if (LoadMod(&info, mods_base_dir_str, folder_name, false))
                        modList.insert(modList.begin(), info);
                }
            }
        }
    }
#else
    fs::path mods_base_dir_fs = resolvePath(modConfigDirBuf);
    std::string mods_base_dir_str = mods_base_dir_fs.string();
    PrintLog("InitMods (non-PS3): Resolved mods base directory to: %s", mods_base_dir_str.c_str()); 

    if (fs::exists(mods_base_dir_fs) && fs::is_directory(mods_base_dir_fs)) {
        PrintLog("InitMods (non-PS3): Mods base directory exists and is a directory."); 
        std::string mod_config_path_str = (mods_base_dir_fs / "modconfig.ini").string();
        PrintLog("InitMods (non-PS3): Looking for modconfig.ini at: %s", mod_config_path_str.c_str()); 
        FileIO *configFile     = fOpen(mod_config_path_str.c_str(), "r");
        if (configFile) {
            PrintLog("InitMods (non-PS3): Found modconfig.ini."); 
            fClose(configFile);
            IniParser modConfig(mod_config_path_str.c_str(), false);

            for (size_t m = 0; m < modConfig.items.size(); ++m) {
                bool active = false;
                ModInfo info;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                if (LoadMod(&info, mods_base_dir_str, modConfig.items[m].key, active))
                    modList.push_back(info);
            }
        } else { 
             PrintLog("InitMods (non-PS3): modconfig.ini not found at: %s", mod_config_path_str.c_str()); 
        }

        PrintLog("InitMods (non-PS3): Scanning for individual mod folders in: %s", mods_base_dir_str.c_str()); 
        try {
            auto rdi = fs::directory_iterator(mods_base_dir_fs);
            for (auto de : rdi) {
                if (de.is_directory()) {
                    fs::path modDirPath_fs = de.path();
                    PrintLog("InitMods (non-PS3): Found potential mod directory: %s", modDirPath_fs.filename().string().c_str()); 
                    ModInfo info;
                    std::string folder_name = modDirPath_fs.filename().string();

                    bool flag = true;
                    for (size_t m = 0; m < modList.size(); ++m) {
                        if (modList[m].folder == folder_name) {
                            flag = false;
                            break;
                        }
                    }

                    if (flag) {
                        if (LoadMod(&info, mods_base_dir_str, folder_name, false))
                            modList.insert(modList.begin(), info);
                    }
                }
            }
        } catch (fs::filesystem_error const& fe) { 
            PrintLog("Mods Folder Scanning Error: ");
            PrintLog(fe.what());
        }
    }
#endif

    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");
    for (size_t m = 0; m < modList.size(); ++m) { 
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
bool LoadMod(ModInfo *info, std::string modsPath, std::string folder, bool active)
{
    if (!info)
        return false;

    PrintLog("LoadMod: Attempting to load mod from folder: %s (Active: %s)", folder.c_str(), active ? "yes" : "no"); 

    info->fileMap.clear();
    info->name    = "";
    info->desc    = "";
    info->author  = "";
    info->version = "";
    info->folder  = "";
    info->active  = false;

    const std::string modDir_str = ps3_join_path(modsPath, folder); 

    FileIO *f = fOpen(ps3_join_path(modDir_str, "mod.ini").c_str(), "r");
    if (f) {
        PrintLog("LoadMod: Found mod.ini in %s", modDir_str.c_str()); 
        fClose(f);
        IniParser modSettings(ps3_join_path(modDir_str, "mod.ini").c_str(), false);

        info->name    = "Unnamed Mod";
        info->desc    = "";
        info->author  = "Unknown Author";
        info->version = "1.0.0";
        info->folder  = folder;

        char infoBuf[0x100];
        // Name
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Name", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->name = infoBuf;
        // Desc
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Description", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->desc = infoBuf;
        // Author
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Author", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->author = infoBuf;
        // Version
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Version", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->version = infoBuf;

        info->active = active;

    // Store the full path to the mod's directory in info->path
    // This path (modDir_str) is correctly /dev_hdd0/.../USRDIR/mods/ModFolder/
    info->path = modDir_str;
    PrintLog("LoadMod: Stored mod base path in info->path: %s", info->path.c_str());

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
            sprintf(path, "mods/%s/", folder.c_str());
            info->savePath = path;
        }

        info->forceSonic1 = false;
        modSettings.GetBool("", "ForceSonic1", &info->forceSonic1);
        if (info->forceSonic1 && info->active)
            Engine.forceSonic1 = true;
        
        PrintLog("LoadMod: Parsed mod.ini for %s. Name: %s, Author: %s, Version: %s, Scripts: %s", 
            folder.c_str(), info->name.c_str(), info->author.c_str(), info->version.c_str(), info->useScripts ? "yes" : "no"); 

        return true;
    }
    PrintLog("LoadMod: mod.ini not found in %s", modDir_str.c_str()); 
    return false;
}

void ScanModFolder(ModInfo *info)
{
    if (!info)
        return;

    char modBuf[0x100];
    char modConfigPathBuf[0x100];
    sprintf(modConfigPathBuf, "%smods", modsPath);

    info->fileMap.clear();
    int initialMapSize = info->fileMap.size(); 

    // Check for Data/ replacements
#ifdef PS3
    // std::string dataPath_str = ps3_join_path(ps3_join_path(modsPath, info->folder), "Data"); // OLD LINE
    std::string dataPath_str = ps3_join_path(info->path, "Data"); // NEW LINE - info->path should be the full path to the mod's root
    PrintLog("ScanModFolder (PS3, Data): Scanning %s (using info->path: %s)", dataPath_str.c_str(), info->path.c_str());
    if (ps3_path_exists(dataPath_str) && ps3_is_directory(dataPath_str)) {
        std::vector<PS3DirEntry> data_entries = ps3_recursive_directory_iterator(dataPath_str);
        for (const auto& data_de : data_entries) {
            if (data_de.is_file) {
                char modBuf[0x100];
                StrCopy(modBuf, data_de.path_str.c_str());
#else
    fs::path dataPath_fs = resolvePath(ps3_join_path(ps3_join_path(modsPath, info->folder), "Data")); 
    PrintLog("ScanModFolder (non-PS3, Data): Scanning %s", dataPath_fs.string().c_str());
    if (fs::exists(dataPath_fs) && fs::is_directory(dataPath_fs)) {
        try {
            auto data_rdi = fs::recursive_directory_iterator(dataPath_fs);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
#endif
                    char folderTest[4][0x10] = {
                        "Data/",
                        "Data\\",
                        "data/",
                        "data\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindLastStringToken(modBuf, folderTest[i]);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        // Correctly extract the relative path part
                        const char* relativePart = modBuf + tokenPos;
                        StrCopy(buffer, relativePart);
                        for(char* p = buffer; *p; ++p) if (*p == '\\') *p = '/'; // Normalize to forward slashes
                        
                        std::string path_key_str(buffer); // This is the relative path, e.g., "Data/Music/Boss.ogg"
                        // modBuf already contains the full absolute path to the mod file, e.g., "/dev_hdd0/.../mods/MyMod/Data/Music/Boss.ogg"
                        
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        StringLowerCase(pathLower, path_key_str.c_str()); // Lowercase the relative path for the key

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                        PrintLog("ScanModFolder: Added to fileMap: Key='%s', Value='%s'", pathLower, modBuf);
                    }
                }
            }
#ifndef PS3 
        } catch (fs::filesystem_error const& fe) { 
            PrintLog("Data Folder Scanning Error: %s", fe.what());
        }
#endif
    }

    PrintLog("ScanModFolder (Data): Found %d file mappings.", (int)(info->fileMap.size() - initialMapSize)); 
    initialMapSize = info->fileMap.size(); 

    // Check for Scripts/ replacements
#ifdef PS3
    // std::string scriptPath_str = ps3_join_path(ps3_join_path(modsPath, info->folder), "Scripts"); // OLD LINE
    std::string scriptPath_str = ps3_join_path(info->path, "Scripts"); // NEW LINE
    PrintLog("ScanModFolder (PS3, Scripts): Scanning %s (using info->path: %s)", scriptPath_str.c_str(), info->path.c_str());
    if (ps3_path_exists(scriptPath_str) && ps3_is_directory(scriptPath_str)) {
        std::vector<PS3DirEntry> script_entries = ps3_recursive_directory_iterator(scriptPath_str);
        for (const auto& data_de : script_entries) {
            if (data_de.is_file) {
                char modBuf[0x100];
                StrCopy(modBuf, data_de.path_str.c_str());
#else
    fs::path scriptPath_fs = resolvePath(ps3_join_path(ps3_join_path(modsPath, info->folder), "Scripts"));
    PrintLog("ScanModFolder (non-PS3, Scripts): Scanning %s", scriptPath_fs.string().c_str());
    if (fs::exists(scriptPath_fs) && fs::is_directory(scriptPath_fs)) {
        try {
            auto data_rdi = fs::recursive_directory_iterator(scriptPath_fs);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
#endif
                    char folderTest[4][0x10] = {
                        "Scripts/",
                        "Scripts\\",
                        "scripts/",
                        "scripts\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindLastStringToken(modBuf, folderTest[i]);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        const char* relativePart = modBuf + tokenPos;
                        StrCopy(buffer, relativePart);
                        for(char* p = buffer; *p; ++p) if (*p == '\\') *p = '/';
                        
                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        StringLowerCase(pathLower,path.c_str()); // Lowercase the relative path for the key

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                        PrintLog("ScanModFolder: Added to fileMap: Key='%s', Value='%s'", pathLower, modBuf);
                    }
                }
            }
#ifndef PS3 
        } catch (fs::filesystem_error const& fe) { 
            PrintLog("Script Folder Scanning Error: %s", fe.what());
        }
#endif
    }

    PrintLog("ScanModFolder (Scripts): Found %d file mappings.", (int)(info->fileMap.size() - initialMapSize)); 
    initialMapSize = info->fileMap.size(); 

    // Check for Bytecode/ replacements
#ifdef PS3
    // std::string bytecodePath_str = ps3_join_path(ps3_join_path(modsPath, info->folder), "Bytecode"); // OLD LINE
    std::string bytecodePath_str = ps3_join_path(info->path, "Bytecode"); // NEW LINE
    PrintLog("ScanModFolder (PS3, Bytecode): Scanning %s (using info->path: %s)", bytecodePath_str.c_str(), info->path.c_str());
    if (ps3_path_exists(bytecodePath_str) && ps3_is_directory(bytecodePath_str)) {
        std::vector<PS3DirEntry> bytecode_entries = ps3_recursive_directory_iterator(bytecodePath_str);
        for (const auto& data_de : bytecode_entries) {
            if (data_de.is_file) {
                char modBuf[0x100];
                StrCopy(modBuf, data_de.path_str.c_str());
#else
    fs::path bytecodePath_fs = resolvePath(ps3_join_path(ps3_join_path(modsPath, info->folder), "Bytecode"));
    PrintLog("ScanModFolder (non-PS3, Bytecode): Scanning %s", bytecodePath_fs.string().c_str());
    if (fs::exists(bytecodePath_fs) && fs::is_directory(bytecodePath_fs)) {
        try {
            auto data_rdi = fs::recursive_directory_iterator(bytecodePath_fs);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
#endif
                    char folderTest[4][0x10] = {
                        "Bytecode/",
                        "Bytecode\\",
                        "bytecode/",
                        "bytecode\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindLastStringToken(modBuf, folderTest[i]);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        const char* relativePart = modBuf + tokenPos;
                        StrCopy(buffer, relativePart);
                        for(char* p = buffer; *p; ++p) if (*p == '\\') *p = '/';

                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        StringLowerCase(pathLower,path.c_str()); // Lowercase the relative path for the key

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                        PrintLog("ScanModFolder: Added to fileMap: Key='%s', Value='%s'", pathLower, modBuf);
                    }
                }
            }
#ifndef PS3 
        } catch (fs::filesystem_error const& fe) { 
            PrintLog("Bytecode Folder Scanning Error: %s", fe.what());
        }
#endif
    }
    PrintLog("ScanModFolder (Bytecode): Found %d file mappings.", (int)(info->fileMap.size() - initialMapSize)); 
}

void SaveMods()
{
    char modConfigDirBuf[0x100];
    sprintf(modConfigDirBuf, "%smods", modsPath);

#ifdef PS3
    std::string mods_base_dir_str = resolvePath_ps3(modConfigDirBuf);
    if (ps3_path_exists(mods_base_dir_str) && ps3_is_directory(mods_base_dir_str)) {
        std::string mod_config_path_str = ps3_join_path(mods_base_dir_str, "modconfig.ini");
        IniParser modConfig;
        for (size_t m = 0; m < modList.size(); ++m) { 
#else
    fs::path mods_base_dir_fs = resolvePath(modConfigDirBuf);
    if (fs::exists(mods_base_dir_fs) && fs::is_directory(mods_base_dir_fs)) {
        std::string mod_config_path_str = (mods_base_dir_fs / "modconfig.ini").string();
        IniParser modConfig;
        for (size_t m = 0; m < modList.size(); ++m) { 
#endif
            ModInfo *info = &modList[m];
            modConfig.SetBool("mods", info->folder.c_str(), info->active);
        }
        modConfig.Write(mod_config_path_str.c_str(), false);
    }
}

void RefreshEngine()
{
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
    for (size_t m = 0; m < modList.size(); ++m) { 
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

#endif // RETRO_USE_MOD_LOADER

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