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

class OpenFileDialog : public FileDialog
{
    std::string type_;
    std::vector<std::string> patterns_;

public:
    OpenFileDialog(const std::string &name,
                   const std::string &type,
                   std::vector<std::string> patterns)
        : FileDialog(name)
        , type_(type)
        , patterns_(patterns)
    {}

    void open() override;
};

class OpenManyFilesDialog : public FileDialog
{
    std::string type_;
    std::vector<std::string> patterns_;
    std::list<std::string> pathlist_;
    std::vector< std::future< std::list<std::string> > > promisedlist_;

public:
    OpenManyFilesDialog(const std::string &name,
                        const std::string &type,
                        std::vector<std::string> patterns)
        : FileDialog(name)
        , type_(type)
        , patterns_(patterns)
    {}

    void open() override;
    bool closed() override;
    inline std::list<std::string> files() const { return pathlist_; }
};

class SaveFileDialog : public FileDialog
{
    std::string type_;
    std::vector<std::string> patterns_;

public:
    SaveFileDialog(const std::string &name,
                   const std::string &type,
                   std::vector<std::string> patterns)
        : FileDialog(name)
        , type_(type)
        , patterns_(patterns)
    {}

    void setFolder(std::string path);
    void open();
};

class OpenFolderDialog : public FileDialog
{
public:
    OpenFolderDialog(const std::string &name) : FileDialog(name) {}
    void open();
};


class ColorPickerDialog
{
protected:
    std::tuple<float, float, float> rgb_;
    std::atomic<bool> busy_;

    // private function to open dialog
    static void openColorDialog( std::function<void(std::tuple<float, float, float>)> func);

    // Private Constructor
    ColorPickerDialog();
    ColorPickerDialog(ColorPickerDialog const& copy) = delete;
    ColorPickerDialog& operator=(ColorPickerDialog const& copy) = delete;

public:

    static ColorPickerDialog& instance()
    {
        // The only instance
        static ColorPickerDialog _instance;
        return _instance;
    }

    void open( std::function<void(std::tuple<float, float, float>)> func);

    inline bool busy() { return busy_; }
    inline void setRGB(std::tuple<float, float, float> rgb) { rgb_ = rgb; }
    inline std::tuple<float, float, float> RGB() const { return rgb_; }
};


}



#endif // DIALOGTOOLKIT_H
