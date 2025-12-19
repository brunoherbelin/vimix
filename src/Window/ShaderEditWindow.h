#ifndef SHADEREDITWINDOW_H
#define SHADEREDITWINDOW_H

#include <string>
#include <future>
#include <map>

#include "Filter/ImageFilter.h"
#include "WorkspaceWindow.h"


class ShaderEditWindow : public WorkspaceWindow
{
    uint64_t _cs_id;
    ImageFilter *current_;
    std::map<ImageFilter *, FilteringProgram> filters_;
    std::promise<std::string> *compilation_;
    std::future<std::string>  compilation_return_;

    bool show_shader_inputs_;
    std::string status_;

public:
    ShaderEditWindow();

    void Render();
    void Refresh();
    void setVisible(bool on);

    void BuildShader();
    void BuildAll();

    // from WorkspaceWindow
    bool Visible() const override;
};


#endif // SHADEREDITWINDOW_H
