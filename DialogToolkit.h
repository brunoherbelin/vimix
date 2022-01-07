#ifndef DIALOGTOOLKIT_H
#define DIALOGTOOLKIT_H

#include <string>
#include <list>
#include <vector>
#include <future>

#define VIMIX_FILES_PATTERN "*.mix"
#define MEDIA_FILES_PATTERN "*.mix", "*.mp4", "*.mpg", "*.mpeg", "*.m2v", "*.m4v", "*.avi", "*.mov",\
    "*.mkv", "*.webm", "*.mod", "*.wmv", "*.mxf", "*.ogg",\
    "*.flv", "*.hevc", "*.asf", "*.jpg", "*.png",  "*.gif",\
    "*.tif", "*.tiff", "*.webp", "*.bmp", "*.ppm", "*.svg,"
#define IMAGES_FILES_PATTERN "*.jpg", "*.png", "*.bmp", "*.ppm", "*.gif"

namespace DialogToolkit
{

void ErrorDialog(const char* message);

class FileDialog
{
protected:
    std::string id_;
    std::string directory_;
    std::string path_;
    std::vector< std::future<std::string> >promises_;
    static bool busy_;

public:
    FileDialog(const std::string &name);

    virtual void open() = 0;
    virtual bool closed();
    inline std::string path() const { return path_; }

    static bool busy() { return busy_; }
};

class OpenImageDialog : public FileDialog
{
public:
    OpenImageDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class OpenSessionDialog : public FileDialog
{
public:
    OpenSessionDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class OpenMediaDialog : public FileDialog
{
public:
    OpenMediaDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class SaveSessionDialog : public FileDialog
{
public:
    SaveSessionDialog(const std::string &name) : FileDialog(name) {}
    void setFolder(std::string path);
    void open();
};

class OpenFolderDialog : public FileDialog
{
public:
    OpenFolderDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class MultipleImagesDialog : public FileDialog
{
    std::list<std::string> pathlist_;
    std::vector< std::future< std::list<std::string> > > promisedlist_;
public:
    MultipleImagesDialog(const std::string &name) : FileDialog(name) {}
    void open() override;
    bool closed() override;
    inline std::list<std::string> images() const { return pathlist_; }
};

}



#endif // DIALOGTOOLKIT_H
