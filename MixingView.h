#ifndef MIXINGVIEW_H
#define MIXINGVIEW_H

#include "View.h"

class MixingGroup;

class MixingView : public View
{
public:
    MixingView();

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    void centerSource(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>) override;
    void arrow (glm::vec2) override;

    void setAlpha (Source *s);
    inline float limboScale() { return limbo_scale_; }

private:
    void updateSelectionOverlay() override;

    float limbo_scale_;

    Group *slider_root_;
    Disk *slider_;
    Disk *button_white_;
    Disk *button_black_;
    Disk *stashCircle_;
    Mesh *mixingCircle_;
    Mesh *circle_;

    // linked sources
    std::list< MixingGroup *> groups_;
};


#endif // MIXINGVIEW_H
