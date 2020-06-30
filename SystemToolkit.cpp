#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <filesystem>

using namespace std;

#ifdef WIN32
#include <windows.h>
#elif defined(LINUX) or defined(APPLE)
#include <pwd.h>
#endif

#include "defines.h"
#include "SystemToolkit.h"


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
    return filesystem::path(path).filename();
}

string SystemToolkit::base_filename(const string& path)
{
    return filesystem::path(path).stem();
}

string SystemToolkit::path_filename(const string& path)
{
    return filesystem::path(path).parent_path();
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
    return filesystem::path(filename).extension();
}

string SystemToolkit::home_path()
{
    // find home
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

    return filesystem::path(mHomePath).string();
}

string SystemToolkit::settings_path()
{
    filesystem::path settings(home_path());

    // platform dependent location of settings
#ifdef WIN32
    settings /= "AppData";
    settings /= "Roaming";
#elif defined(APPLE)
    settings /= "Library";
    settings /= "Application Support";
#elif defined(LINUX)
    settings /= ".config";
#endif

    // append folder location for vimix settings
    settings /= APP_NAME;

    // create folder if not existint
    if (!filesystem::exists(settings)) {
        if (!filesystem::create_directories(settings) )
            return home_path();
    }

    return settings.string();
}

string SystemToolkit::full_filename(const std::string& path, const string &filename)
{
    filesystem::path fullfilename( path );
    fullfilename /= filename;

    return fullfilename.string();
}

bool SystemToolkit::file_exists(const string& path)
{
    return filesystem::exists( filesystem::status(path) );
}

list<string> SystemToolkit::list_directory(const string& path, const string& filter)
{
    list<string> ls;
    // loop over elements of the directory
    for (const auto & entry : filesystem::directory_iterator(path)) {
        // list only files, not directories
        if ( filesystem::is_regular_file(entry)) {
            // add the path if no filter, or if filter is matching
            if (filter.empty() || entry.path().extension() == filter )
                ls.push_back( entry.path().string() );
        }
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
        system( buf );
#endif
}
