#ifndef DIALOGTOOLKIT_H
#define DIALOGTOOLKIT_H

#include <string>


namespace DialogToolkit
{

std::string saveSessionFileDialog(const std::string &path);

std::string openSessionFileDialog(const std::string &path);

std::string ImportFileDialog(const std::string &path);

std::string FolderDialog(const std::string &path);

void ErrorDialog(const char* message);

}



#endif // DIALOGTOOLKIT_H
