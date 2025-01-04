/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <vector>
#include <climits>

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
#include <fnmatch.h>
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
    unsigned long size = 0;
    FILE *file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long m = 0;
        int ret = 0, ret2 = 0;
        ret = fscanf (file, "%lu", &m);  // virtual mem program size,
        ret2 = fscanf (file, "%lu", &m);  // resident set size,
        fclose (file);
        if (ret>0 && ret2>0)
            size = m * getpagesize();
    }
    return (long)size;

// ALTERNATIVE implementation (less reactive)
//    struct rusage r_usage;
//    getrusage(RUSAGE_SELF,&r_usage);
//    return 1024 * r_usage.ru_maxrss;

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

string SystemToolkit::extension_filename(const string& filename)
{
    string ext;
    auto loc = filename.find_last_of(".");
    if (loc != string::npos)
        ext = filename.substr( loc + 1 );
    return ext;
}

bool SystemToolkit::has_extension(const std::string& filename, const std::string& extension)
{
    std::string ext = extension_filename(filename);
    std::transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext.compare(extension) == 0;
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
    if (path.back() != PATH_SEP)
        fullfilename += PATH_SEP;
    fullfilename += filename;

    return fullfilename;
}

unsigned long SystemToolkit::file_modification_time(const std::string& path)
{
    if (file_exists(path)) {
        struct stat statsfile;
        // fill statistics of given file path
        if( stat( path.c_str(), &statsfile) > -1 ) {
            // return modification time
            return (unsigned long) statsfile.st_mtime;
        }
    }

    return 0;
}

std::string SystemToolkit::file_modification_time_string(const std::string& path)
{
    ostringstream oss;

    if (file_exists(path)) {
        struct stat statsfile;
        // fill statistics of given file path
        if( stat( path.c_str(), &statsfile) > -1 ) {
            // read modification time
            tm *datetime = localtime(&statsfile.st_mtime);
            ostringstream oss;
            oss << setw(4) << setfill('0') << to_string(datetime->tm_year + 1900);
            oss << setw(2) << setfill('0') << to_string(datetime->tm_mon + 1);
            oss << setw(2) << setfill('0') << to_string(datetime->tm_mday );
            oss << setw(2) << setfill('0') << to_string(datetime->tm_hour );
            oss << setw(2) << setfill('0') << to_string(datetime->tm_min );
            oss << setw(2) << setfill('0') << to_string(datetime->tm_sec );
        }

    }

    return oss.str();
}


void SystemToolkit::reorder_file_list(std::list<string> &filelist, Ordering m)
{
    if ( m >= DATE ) {
        auto dateComparator = [](const std::string &a, const std::string &b) {
            return SystemToolkit::file_modification_time(a) < SystemToolkit::file_modification_time(b);
        };
        filelist.sort( dateComparator );
        if ( m == DATE_INVERSE )
            filelist.reverse();
    }
    else  {
        auto alphaComparator = [](const std::string &a, const std::string &b) {
            std::string _a = a;
            std::string _b = b;
            std::transform(_a.begin(), _a.end(), _a.begin(), ::tolower);
            std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
            return _a < _b;
        };
        filelist.sort( alphaComparator );
        if ( m == ALPHA_INVERSE )
            filelist.reverse();
    }
}


bool SystemToolkit::file_exists(const string& path)
{
    if (path.empty())
        return false;

    return access(path.c_str(), R_OK) == 0;

    // TODO : WIN32 implementation (see tinyfd)
}


// tests if dir is a directory and return its path, empty string otherwise
string SystemToolkit::path_directory(const string& path)
{
    string directorypath = "";

    DIR *dir;
    if ((dir = opendir (path.c_str())) != NULL) {
        directorypath = path;
        if (path.back() != PATH_SEP)
            directorypath += PATH_SEP;
        closedir (dir);
    }

    return directorypath;
}

list<string> SystemToolkit::list_directory(const string& path, const list<string>& patterns, Ordering m)
{
    list<string> ls;

    DIR *dir;
    if ((dir = opendir (path.c_str())) != NULL) {
        // list all the files and directories within directory
        struct dirent *ent;
        while ((ent = readdir (dir)) != NULL) {
            if ( ent->d_type == DT_REG)
            {
                int found = FNM_NOMATCH;
                for (auto it = patterns.cbegin(); it != patterns.cend() && found == FNM_NOMATCH; ++it)
                    // test pattern in CASE insensitive
                    found = fnmatch( it->c_str(), ent->d_name, FNM_CASEFOLD );
                if (found != FNM_NOMATCH)
                    ls.push_back( full_filename(path, ent->d_name) );
            }
        }
        closedir (dir);
    }

    reorder_file_list( ls, m );

    return ls;
}

std::string SystemToolkit::get_text_content(const std::string& path)
{
    string content = "";

    // Open the file
    std::ifstream file(path);

    // Check if the file is opened successfully
    if (file.is_open()) {
        // Read the content of the file into an std::string
        std::string fileContent((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

        // return content
        content = fileContent;
    }

    // Close the file
    file.close();

    return content;
}

void SystemToolkit::open(const string& url)
{
    int ignored __attribute__((unused));
#ifdef WIN32
        ShellExecuteA( nullptr, nullptr, url.c_str(), nullptr, nullptr, 0 );
#elif defined APPLE
        char buf[2048];
        snprintf( buf, 2048, "open '%s'", url.c_str() );
        ignored = system( buf );
#else
        char buf[2048];
        snprintf( buf, 2048, "xdg-open '%s'", url.c_str() );
        ignored = system( buf );
#endif
}

void SystemToolkit::execute(const string& command)
{
    int ignored __attribute__((unused));
#ifdef WIN32
        ShellExecuteA( nullptr, nullptr, url.c_str(), nullptr, nullptr, 0 );
#elif defined APPLE
    (void) system( command.c_str() );
#else
    ignored = system( command.c_str() );
#endif
}
// example :
//      std::thread (SystemToolkit::execute,
//                   "gst-launch-1.0 udpsrc port=5000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink").detach();;



vector<string> split_path(const string& path) {
    vector<string> strings;
    size_t start;
    size_t end = 0;
    while ((start = path.find_first_not_of(PATH_SEP, end)) != string::npos) {
        end = path.find(PATH_SEP, start);
        strings.push_back(path.substr(start, end - start));
    }
    return strings;
}

//
// loosely inspired from http://mrpmorris.blogspot.com/2007/05/convert-absolute-path-to-relative-path.html
//
string SystemToolkit::path_relative_to_path( const string& absolutePath, const string& relativeTo )
{
    string relativePath = "";
    vector<string> absoluteDirectories = split_path(absolutePath);
    vector<string> relativeToDirectories = split_path(relativeTo);

    // Get the shortest of the two paths
    size_t length = MINI( absoluteDirectories.size(), relativeToDirectories.size() );

    // Use to determine where in the loop we exited
    size_t lastCommonRoot = SIZE_MAX;
    size_t index = 0;

    // Find common root
    for (; index < length; ++index) {
        if (absoluteDirectories[index].compare(relativeToDirectories[index]) == 0)
            lastCommonRoot = index;
        else
            break;
    }

    // If we didn't find a common prefix then return base absolute path
    if (lastCommonRoot == SIZE_MAX || absoluteDirectories.size() < 1)
        return absolutePath;

    // Add the '..'
    for (index = lastCommonRoot + 1; index < relativeToDirectories.size(); ++index) {
        if (relativeToDirectories[index].size() > 0)
            relativePath += string("..") + PATH_SEP;
    }

    // Add the relative folders
    for (index = lastCommonRoot + 1; index < absoluteDirectories.size() - 1; ++index) {
        relativePath += absoluteDirectories[index];
        relativePath += PATH_SEP;
    }

    relativePath += absoluteDirectories[absoluteDirectories.size() - 1];

    return relativePath;
}

string SystemToolkit::path_absolute_from_path(const string& relativePath, const string& relativeTo)
{
    string absolutePath = string(1, PATH_SEP);
    vector<string> relativeDirectories = split_path(relativePath);
    vector<string> relativeToDirectories = split_path(relativeTo);

    // how many ".."
    size_t count_relative = 0;
    for (; count_relative < relativeDirectories.size() - 1; ++count_relative) {
        if (relativeDirectories[count_relative].compare("..") != 0)
            break;
    }
    // take the left part of relativeTo path
    if (relativeToDirectories.size() > count_relative ) {
        for (size_t i = 0; i < relativeToDirectories.size() -count_relative; ++i) {
            absolutePath += relativeToDirectories[i];
            absolutePath += PATH_SEP;
        }
    }
    // add the rest of the relative path
    for (; count_relative < relativeDirectories.size() - 1; ++count_relative) {
        absolutePath += relativeDirectories[count_relative];
        absolutePath += PATH_SEP;
    }
    absolutePath += relativeDirectories[count_relative];

    return absolutePath;
}

std::string SystemToolkit::filename_sequential(const std::string& path, const std::string& base, const std::string& extension)
{
    std::ostringstream filename;

    // make sure extension is without the dot
    string ext = extension;
    auto loc = extension.find_last_of(".");
    if (loc != string::npos)
        ext = extension.substr( loc + 1 );

    // make sure path is ok
    std::string p = SystemToolkit::path_directory(path);
    if (p.empty())
        p = home_path();

    // list all files in the target directory that potentially match the sequence naming pattern
    std::list<std::string> pattern;
    pattern.push_back( base + "*." + ext );
    std::list<std::string> ls = SystemToolkit::list_directory(p, pattern);

    // establish a filename for a consecutive sequence of numbers
    for (int i = 0; i < (int) ls.size() + 1; ++i) {
        // clear
        filename.str(std::string());
        // add path
        filename << p;
        // add base filename
        if (base.empty())
            filename << "vimix";
        else
            filename << base ;
        // add sequential number
        filename  << "_" << std::setw(4) << std::setfill('0') << i;
        // add extension
        if (!ext.empty())
            filename << "." << ext;
        // use that filename if was not already used
        if ( std::find( ls.begin(), ls.end(), filename.str() ) == ls.end() )
            break;
    }

    return filename.str();
}

std::string SystemToolkit::filename_dateprefix(const std::string& path, const std::string& base, const std::string& extension)
{
    // make sure extension is without the dot
    string ext = extension;
    auto loc = extension.find_last_of(".");
    if (loc != string::npos)
        ext = extension.substr( loc + 1 );

    // make sure path is ok
    std::string p = SystemToolkit::path_directory(path);
    if (p.empty())
        p = home_path();

    // build filename
    std::ostringstream filename;
    // add path
    filename << p;
    // add date prefix
    filename << SystemToolkit::date_time_string() << "_";
    // add base filename
    if (base.empty())
        filename << "vimix";
    else
        filename << base ;
    // add extension
    if (!ext.empty())
        filename << "." << ext;

    return filename.str();
}


