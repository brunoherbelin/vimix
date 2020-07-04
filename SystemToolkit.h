#ifndef SYSTEMTOOLKIT_H
#define SYSTEMTOOLKIT_H

#ifdef WIN32
#define PATH_SEP '\\'
#elif defined(LINUX) or defined(APPLE)
#define PATH_SEP '/'
#endif

#include <string>
#include <list>

namespace SystemToolkit
{
    // get fixed length string (17 chars) YYYYMMDDHHmmssiii
    std::string date_time_string();

    // get the OS dependent username
    std::string username();

    // get the OS dependent home path
    std::string home_path();

    // get the OS dependent path where to store settings
    std::string settings_path();

    // builds the OS dependent complete file name
    std::string full_filename(const std::string& path, const std::string& filename);

    // extract the filename from a full path / URI (e.g. file:://home/me/toto.mpg -> toto.mpg)
    std::string filename(const std::string& path);

    // extract the base filename from a full path / URI (e.g. file:://home/me/toto.mpg -> toto)
    std::string base_filename(const std::string& path);

    // extract the path of a filename from a full URI (e.g. file:://home/me/toto.mpg -> file:://home/me/)
    std::string path_filename(const std::string& path);

    // Truncate a full filename to display the right part (e.g. file:://home/me/toto.mpg -> ...ome/me/toto.mpg)
    std::string trunc_filename(const std::string& path, int lenght);

    // extract the extension of a filename
    std::string extension_filename(const std::string& filename);

    // list all files of a directory mathing the given filter extension (if any)
    std::list<std::string> list_directory(const std::string& path, const std::string& filter = "");

    // true of file exists
    bool file_exists(const std::string& path);

    // try to open the file with system
    void open(const std::string& path);
}

#endif // SYSTEMTOOLKIT_H
