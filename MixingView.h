#ifndef MIXINGVIEW_H
#define MIXINGVIEW_H

#include "View.h"

//class MixingGroup;

class MixingView : public View
{
public:
    MixingView();
    // non assignable class
    MixingView(MixingView const&) = delete;
    MixingView& operator=(MixingView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    void centerSource(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>) override;
    void terminate() override;
    Cursor over (glm::vec2) override;
    void arrow (glm::vec2) override;

    void setAlpha (Source *s);

private:
    void updateSelectionOverlay() override;

    float limbo_scale_;

    Group *slider_root_;
    Disk *slider_;
    Disk *button_white_;
    Disk *button_black_;
//    Disk *stashCircle_;
    Mesh *mixingCircle_;
    Mesh *circle_;
    Mesh *limbo_;
    Group *limbo_slider_root_;
    Mesh *limbo_up_, *limbo_down_;
    Disk *limbo_slider_;
};


#endif // MIXINGVIEW_H
