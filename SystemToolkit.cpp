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
#include <sys/resource.h>
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



long SystemToolkit::memory_usage() {
    struct rusage r_usage;
    getrusage(RUSAGE_SELF,&r_usage);
    return r_usage.ru_maxrss;
//    return r_usage.ru_isrss;
}

string SystemToolkit::byte_to_string(long b)
{
    double numbytes = static_cast<double>(b);
    ostringstream oss;

    std::list<std::string> list = {" Bytes", " KB", " MB", " GB", " TB"};
    std::list<std::string>::iterator i = list.begin();

    while(numbytes >= 1024.0 && i != list.end())
    {
        i++;
        numbytes /= 1024.0;
    }
    oss << std::fixed << std::setprecision(2) << numbytes << *i;
    return oss.str();
}


string SystemToolkit::date_time_string()
{
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm* datetime = localtime(&t);

    auto duration = now.time_since_epoch();
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count() % 1000;

    ostringstream oss;
    oss << setw(4) << setfill('0') << to_string(datetime->tm_year + 1900);
    oss << setw(2) << setfill('0') << to_string(datetime->tm_mon + 1);
    oss << setw(2) << setfill('0') << to_string(datetime->tm_mday );
    oss << setw(2) << setfill('0') << to_string(datetime->tm_hour );
    oss << setw(2) << setfill('0') << to_string(datetime->tm_min );
    oss << setw(2) << setfill('0') << to_string(datetime->tm_sec );
    oss << setw(3) << setfill('0') << to_string(millis);

    // fixed length string (17 chars) YYYYMMDDHHmmssiii
    return oss.str();
}

string SystemToolkit::filename(const string& path)
{
    return path.substr(path.find_last_of(PATH_SEP) + 1);
}

string SystemToolkit::base_filename(const string& path)
{
    string basefilename = SystemToolkit::filename(path);
    const size_t period_idx = basefilename.rfind('.');
    if (string::npos != period_idx)
    {
        basefilename.erase(period_idx);
    }
    return basefilename;
}

string SystemToolkit::path_filename(const string& path)
{
    return path.substr(0, path.find_last_of(PATH_SEP) + 1);
}

string SystemToolkit::trunc_filename(const string& path, int lenght)
{
    string trunc = path;
    int l = path.size();
    if ( l > lenght ) {
        trunc = string("...") + path.substr( l - lenght + 3 );
    }
    return trunc;
}

string SystemToolkit::extension_filename(const string& filename)
{
    string ext = filename.substr(filename.find_last_of(".") + 1);
    return ext;
}

std::string SystemToolkit::home_path()
{
    // 1. find home
    char *mHomePath;
    // try the system user info
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
        mHomePath = pwd->pw_dir;
    else
        // try the $HOME environment variable
        mHomePath = getenv("HOME");

    return string(mHomePath) + PATH_SEP;
}

std::string SystemToolkit::username()
{
    // 1. find home
    char *user;
    // try the system user info
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
        user = pwd->pw_name;
    else
        // try the $USER environment variable
        user = getenv("USER");

    return string(user);
}

bool create_directory(const string& path)
{
    return !mkdir(path.c_str(), 0755) || errno == EEXIST;

    // TODO : verify WIN32 implementation
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
            if ( !create_directory(settingspath) )
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

string SystemToolkit::full_filename(const std::string& path, const string &filename)
{
    string fullfilename = path;
    fullfilename += PATH_SEP;
    fullfilename += filename;

    return fullfilename;
}

bool SystemToolkit::file_exists(const string& path)
{
    return access(path.c_str(), R_OK) == 0;

    // TODO : WIN32 implementation
}

list<string> SystemToolkit::list_directory(const string& path, const string& filter)
{
    list<string> ls;

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (path.c_str())) != NULL) {
      // list all the files and directories within directory
      while ((ent = readdir (dir)) != NULL) {
          if ( ent->d_type == DT_REG)
          {
              string filename = string(ent->d_name);
              if ( extension_filename(filename) == filter)
                  ls.push_back( full_filename(path, filename) );
          }
      }
      closedir (dir);
    }

    return ls;
}

void SystemToolkit::open(const string& url)
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
        int r = system( buf );
#endif
}



