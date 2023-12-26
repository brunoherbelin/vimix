#ifndef DIALOGTOOLKIT_H
#define DIALOGTOOLKIT_H

#include <string>
#include <list>
#include <vector>
#include <future>

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

class OpenSessionDialog : public FileDialog
{
public:
    OpenSessionDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class SaveSessionDialog : public FileDialog
{
public:
    SaveSessionDialog(const std::string &name) : FileDialog(name) {}
    void setFolder(std::string path);
    void open();
};

class MultipleSessionsDialog : public FileDialog
{
    std::list<std::string> pathlist_;
    std::vector< std::future< std::list<std::string> > > promisedlist_;
public:
    MultipleSessionsDialog(const std::string &name) : FileDialog(name) {}
    void open() override;
    bool closed() override;
    inline std::list<std::string> files() const { return pathlist_; }
};

class OpenPlaylistDialog : public FileDialog
{
public:
    OpenPlaylistDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class SavePlaylistDialog : public FileDialog
{
public:
    SavePlaylistDialog(const std::string &name) : FileDialog(name) {}
    void setFolder(std::string path);
    void open();
};

class OpenMediaDialog : public FileDialog
{
public:
    OpenMediaDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class OpenSubtitleDialog : public FileDialog
{
public:
    OpenSubtitleDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class OpenFolderDialog : public FileDialog
{
public:
    OpenFolderDialog(const std::string &name) : FileDialog(name) {}
    void open();
};

class OpenImageDialog : public FileDialog
{
public:
    OpenImageDialog(const std::string &name) : FileDialog(name) {}
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


class ColorPickerDialog
{
protected:
    std::tuple<float, float, float> rgb_;
    std::vector< std::future< std::tuple<float, float, float> > > promises_;
    static bool busy_;

public:
    ColorPickerDialog();

    void open();
    bool closed();

    inline void setRGB(std::tuple<float, float, float> rgb) { rgb_ = rgb; }
    inline std::tuple<float, float, float> RGB() const { return rgb_; }

    static bool busy() { return busy_; }
};


}



#endif // DIALOGTOOLKIT_H
