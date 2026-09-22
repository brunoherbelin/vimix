#ifndef INPUTMAPPINGWINDOW_H
#define INPUTMAPPINGWINDOW_H

#include <string>
#include <array>
#include <list>
#include <memory>

#include "Source/SourceList.h"
#include "WorkspaceWindow.h"

class SourceCallback;
class Session;

class InputMappingWindow : public WorkspaceWindow
{
    std::array< std::string, 5 > input_mode;
    std::array< uint, 5 > current_input_for_mode;
    uint current_input_;

    // A callback assigned to an input inside a nested session (i.e. a bundle).
    // Displayed disabled, except for a button to test it on its target.
    struct NestedCallback {
        // where it lives, e.g. "Bundle > Sub-bundle > "
        std::string path;
        // label of the target
        std::string label;
        // icon indications
        std::list< std::pair<int, int> > icons;
        std::string value;
        // target of the callback, to apply it when testing
        Target target;
        // session holding this callback (the one to open to edit it)
        Session *session;
        // our own copy of the callback (we do not own the one in the session)
        std::unique_ptr< SourceCallback > callback;
        // constructors
        NestedCallback();
        ~NestedCallback();
        NestedCallback(NestedCallback &&);
        NestedCallback &operator=(NestedCallback &&);
    };

    // Callbacks assigned to current_input_ in the sessions nested inside SessionSources (i.e. bundles).
    std::list< NestedCallback > nested_callbacks_;
    // (re)build nested_callbacks_ when the input or the session tree changed
    void updateNestedCallbacks(Session *se, uint current_input);
    // recursively fill nested_callbacks_ from the bundles of the given session
    void listNestedInputCallbacks(Session *se, uint input, const std::string &path, int level = 0);

    Target ComboSelectTarget(const Target &current);
    uint ComboSelectCallback(uint current, bool imageprocessing, bool mediaplayer);
    void SliderParametersCallback(SourceCallback *callback, const Target &target);

public:
    InputMappingWindow();

    void Render();
    void setVisible(bool on);

    // from WorkspaceWindow
    bool Visible() const override;
};

#endif // INPUTMAPPINGWINDOW_H
