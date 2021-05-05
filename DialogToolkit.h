#ifndef DIALOGTOOLKIT_H
#define DIALOGTOOLKIT_H

#include <string>
#include <list>


namespace DialogToolkit
{

std::string saveSessionFileDialog(const std::string &path);

std::string openSessionFileDialog(const std::string &path);

std::string openMediaFileDialog(const std::string &path);

std::string openFolderDialog(const std::string &path);

std::list<std::string> selectImagesFileDialog(const std::string &path);

void ErrorDialog(const char* message);

}



#endif // DIALOGTOOLKIT_H
