#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <chrono>

#include <locale>
#include <unicode/ustream.h>
#include <unicode/translit.h>

using namespace std;

#ifdef WIN32
#include <windows.h>
#define mkdir(dir, mode) _mkdir(dir)
#include <include/dirent.h>
#include <sys/resource.h>
#define PATH_SEP '\\'
#define PATH_SETTINGS "\\\AppData\\Roaming\\"
#elif defined(LINUX) or defined(APPLE)
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

#if defined(APPLE)
#define PATH_SETTINGS "/Library/Application Support/"
#include <mach/task.h>
#include <mach/mach_init.h>
#elif defined(LINUX)
#include <sys/sysinfo.h>
#define PATH_SETTINGS "/.config/"
#endif

#include "defines.h"
#include "SystemToolkit.h"

/// The amount of memory currently being used by this process, in bytes.
/// it will try to report the resident set in RAM
long SystemToolkit::memory_usage()
{
#if defined(LINUX)
    // Grabbing info directly from the /proc pseudo-filesystem.  Reading from
    // /proc/self/statm gives info on your own process, as one line of
    // numbers that are: virtual mem program size, resident set size,
    // shared pages, text/code, data/stack, library, dirty pages.  The
    // mem sizes should all be multiplied by the page size.
    size_t size = 0;
    FILE *file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long m = 0;
        int ret = 0, ret2 = 0;
        ret = fscanf (file, "%lu", &m);  // virtual mem program size,
        ret2 = fscanf (file, "%lu", &m);  // resident set size,
        fclose (file);
        if (ret>0 && ret2>0)
            size = (size_t)m * getpagesize();
    }
    return (long)size;

#elif defined(APPLE)
    // Inspired from
    // http://miknight.blogspot.com/2005/11/resident-set-size-in-mac-os-x.html
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    task_info(current_task(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
    return t_info.resident_size;

#elif defined(WIN32)
    // According to MSDN...
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo (GetCurrentProcess(), &counters, sizeof (counters)))
        return counters.PagefileUsage;
    else return 0;

#else
    return 0;
#endif
}

long SystemToolkit::memory_max_usage() {

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
        ++i;
        numbytes /= 1024.0;
    }
    oss << std::fixed << std::setprecision(2) << numbytes << *i;
    return oss.str();
}

string SystemToolkit::bits_to_string(long b)
{
    double numbytes = static_cast<double>(b);
    ostringstream oss;

    std::list<std::string> list = {" bit", " Kbit", " Mbit", " Gbit", " Tbit"};
    std::list<std::string>::iterator i = list.begin();

    while(numbytes >= 1000.0 && i != list.end())
    {
        ++i;
        numbytes /= 1000.0;
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
    string homePath;
    // try the system user info
    // NB: avoids depending on changes of the $HOME env. variable
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
        homePath = std::string(pwd->pw_dir);
    else
        // try the $HOME environment variable
        homePath = std::string(getenv("HOME"));

    return homePath + PATH_SEP;
}


std::string SystemToolkit::cwd_path()
{
    char mCwdPath[PATH_MAX];

    if (getcwd(mCwdPath, sizeof(mCwdPath)) != NULL)
        return string(mCwdPath) + PATH_SEP;
    else
        return string();
}

std::string SystemToolkit::username()
{
    string userName;
    // try the system user info
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
        userName = std::string(pwd->pw_name);
    else
        // try the $USER environment variable
        userName = std::string(getenv("USER"));

    return userName;
}

bool SystemToolkit::create_directory(const string& path)
{
    return !mkdir(path.c_str(), 0755) || errno == EEXIST;

    // TODO : verify WIN32 implementation
}

bool SystemToolkit::remove_file(const string& path)
{
    bool ret = true;
    if (file_exists(path)) {
        ret = (remove(path.c_str()) == 0);
    }

    return ret;
    // TODO : verify WIN32 implementation
}

string SystemToolkit::settings_path()
{
    // start from home folder
    // NB: use the env.variable $HOME to allow system to specify
    // another directory (e.g. inside a snap)
    string home(getenv("HOME"));

    // 2. try to access user settings folder
    string settingspath = home + PATH_SETTINGS;
    if (SystemToolkit::file_exists(settingspath)) {
        // good, we have a place to put the settings file
        // settings should be in 'vimix' subfolder
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

string SystemToolkit::temp_path()
{
    string temp;

    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir)
        temp = std::string(tmpdir);
    else
        temp = std::string( P_tmpdir );

    temp += PATH_SEP;
    return temp;
    // TODO : verify WIN32 implementation
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
    if (path.empty())
        return false;

    return access(path.c_str(), R_OK) == 0;

    // TODO : WIN32 implementation (see tinyfd)
}


// tests if dir is a directory and return its path, empty string otherwise
std::string SystemToolkit::path_directory(const std::string& path)
{
    string directorypath = "";

    DIR *dir;
    if ((dir = opendir (path.c_str())) != NULL) {
        directorypath = path + PATH_SEP;
        closedir (dir);
    }

    return directorypath;
}

list<string> SystemToolkit::list_directory(const string& path, const string& filter)
{
    list<string> ls;

    DIR *dir;
    if ((dir = opendir (path.c_str())) != NULL) {
        // list all the files and directories within directory
        struct dirent *ent;
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
        (void) system( buf );
#else
        char buf[2048];
        sprintf( buf, "xdg-open '%s'", url.c_str() );
        (void) system( buf );
#endif
}

void SystemToolkit::execute(const string& command)
{
#ifdef WIN32
        ShellExecuteA( nullptr, nullptr, url.c_str(), nullptr, nullptr, 0 );
#elif defined APPLE
    (void) system( command.c_str() );
#else
    (void) system( command.c_str() );
#endif
}
// example :
//      std::thread (SystemToolkit::execute,
//                   "gst-launch-1.0 udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink").detach();;


// Using ICU transliteration :
// https://unicode-org.github.io/icu/userguide/transforms/general/#icu-transliterators

std::string SystemToolkit::transliterate(std::string input)
{
    auto ucs = icu::UnicodeString::fromUTF8(input);

    UErrorCode status = U_ZERO_ERROR;
    icu::Transliterator *firstTrans = icu::Transliterator::createInstance(
                "any-NFKD ; [:Nonspacing Mark:] Remove; NFKC; Latin", UTRANS_FORWARD, status);
    firstTrans->transliterate(ucs);
    delete firstTrans;

    icu::Transliterator *secondTrans = icu::Transliterator::createInstance(
                "any-NFKD ; [:Nonspacing Mark:] Remove; [@!#$*%~] Remove; NFKC", UTRANS_FORWARD, status);
    secondTrans->transliterate(ucs);
    delete secondTrans;

    std::ostringstream output;
    output << ucs;

    return output.str();
}
