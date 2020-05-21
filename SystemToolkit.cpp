#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

using namespace std;

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

#include "defines.h"
#include "SystemToolkit.h"



string SystemToolkit::date_time_string()
{
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    std::tm* datetime = std::localtime(&t);

    auto duration = now.time_since_epoch();
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count() % 1000;

    ostringstream oss;
    oss << setw(4) << setfill('0') << std::to_string(datetime->tm_year + 1900);
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_mon + 1);
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_mday );
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_hour );
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_min );
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_sec );
    oss << setw(3) << setfill('0') << std::to_string(millis);

    // fixed length string (17 chars) YYYYMMDDHHmmssiii
    return oss.str();
}

std::string SystemToolkit::base_filename(const std::string& filename)
{
    std::string basefilename = filename.substr(filename.find_last_of(PATH_SEP) + 1);
    const size_t period_idx = basefilename.rfind('.');
    if (std::string::npos != period_idx)
    {
        basefilename.erase(period_idx);
    }
    return basefilename;
}

std::string SystemToolkit::path_filename(const std::string& filename)
{
    std::string path = filename.substr(0, filename.find_last_of(PATH_SEP) + 1);
    return path;
}

std::string SystemToolkit::extension_filename(const std::string& filename)
{
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    return ext;
}

std::string SystemToolkit::home_path()
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

    return string(mHomePath) + PATH_SEP;
}

string SystemToolkit::settings_path()
{
    string home(home_path());

    // 2. try to access user settings folder
    string settingspath = home + PATH_SETTINGS;
    if (SystemToolkit::file_exists(settingspath)) {
        // good, we have a place to put the settings file
        // settings should be in 'vmix' subfolder
        settingspath += APP_NAME;

        // 3. create the vmix subfolder in settings folder if not existing already
        if ( !SystemToolkit::file_exists(settingspath)) {
            if ( !SystemToolkit::create_directory(settingspath) )
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

string SystemToolkit::settings_prepend_path(const string &basefilename)
{
    string path = SystemToolkit::settings_path();
    path += PATH_SEP;
    path += basefilename;

    return path;
}

bool SystemToolkit::file_exists(const string& path)
{
    return access(path.c_str(), R_OK) == 0;

    // TODO : WIN32 implementation
}


bool SystemToolkit::create_directory(const string& path)
{
    return !mkdir(path.c_str(), 0755) || errno == EEXIST;

    // TODO : verify WIN32 implementation
}

void SystemToolkit::open(const std::string& url)
{
#ifdef WIN32
        ShellExecuteA( nullptr, nullptr, url.c_str(), nullptr, nullptr, 0 );
#elif defined APPLE
        char buf[2048];
        sprintf( buf, "open '%s'", url.c_str() );
        system( buf );
#else
        char buf[2048];
        sprintf( buf, "xdg-open '%s'", url.c_str() );
        system( buf );
#endif
}
