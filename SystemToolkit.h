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
    // get the OS dependent path where to store settings
    std::string settingsPath();
    // builds the OS dependent complete file name for a settings file
    std::string settingsFileCompletePath(std::string basefilename);

    bool fileExists(const std::string& path);
    bool createDirectory(const std::string& path);

}

#endif // SYSTEMTOOLKIT_H
