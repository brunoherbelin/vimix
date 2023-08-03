#ifndef INPUTMAPPINGWINDOW_H
#define INPUTMAPPINGWINDOW_H

#include <string>
#include <array>

#include "SourceList.h"
#include "WorkspaceWindow.h"

class SourceCallback;

class InputMappingWindow : public WorkspaceWindow
{
    std::array< std::string, 4 > input_mode;
    std::array< uint, 4 > current_input_for_mode;
    uint current_input_;

    Target ComboSelectTarget(const Target &current);
    uint ComboSelectCallback(uint current, bool imageprocessing);
    void SliderParametersCallback(SourceCallback *callback, const Target &target);

public:
    InputMappingWindow();

    void Render();
    void setVisible(bool on);

    // from WorkspaceWindow
    bool Visible() const override;
};

#endif // INPUTMAPPINGWINDOW_H
