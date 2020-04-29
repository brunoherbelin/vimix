#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifdef WIN32
#include <windows.h>
#define mkdir(dir, mode) _mkdir(dir)
#include <include/dirent.h>
#define PATH_SEP '\\'
#elif defined(LINUX) or defined(APPLE)
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

#ifdef WIN32
#define PATH_SETTINGS "\\\AppData\\Roaming\\"
#elif defined(APPLE)
#define PATH_SETTINGS "/Library/Application Support/"
#elif defined(LINUX)
#define PATH_SETTINGS "/.config/"
#endif

#include "SystemToolkit.h"

std::string SystemToolkit::settingsPath()
{
    // 1. find home
    char *mHomePath;
    // try the system user info
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)  {
        mHomePath = pwd->pw_dir;
    }
    else  {
        // try the $HOME environment variable
        mHomePath = getenv("HOME");
    }
    std::string home(mHomePath);

    // 2. try to access user settings folder
    std::string settingspath = home + PATH_SETTINGS;
    if (SystemToolkit::fileExists(settingspath)) {
        // good, we have a place to put the settings file
        // settings should be in 'vmix' subfolder
        settingspath += "vmix";

        // 3. create the vmix subfolder in settings folder if not existing already
        if ( !SystemToolkit::fileExists(settingspath)) {
            if ( !SystemToolkit::createDirectory(settingspath) )
                // fallback to home if settings path cannot be created
                settingspath = home;
        }

        return settingspath;
    }
    else {
        // fallback to home if settings path does not exists
        return home;
    }

}

std::string SystemToolkit::settingsFileCompletePath(std::string basefilename)
{
    std::string path = SystemToolkit::settingsPath();
    path += PATH_SEP;
    path += basefilename;

    return path;
}

bool SystemToolkit::fileExists(const std::string& path)
{
    return access(path.c_str(), R_OK) == 0;

    // TODO : WIN32 implementation
}


bool SystemToolkit::createDirectory(const std::string& path)
{
    return !mkdir(path.c_str(), 0755) || errno == EEXIST;

    // TODO : verify WIN32 implementation
}
