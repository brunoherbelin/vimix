#ifndef TRANSITIONVIEW_H
#define TRANSITIONVIEW_H

#include "View.h"

class TransitionView : public View
{
public:
    TransitionView();
    // non assignable class
    TransitionView(TransitionView const&) = delete;
    TransitionView& operator=(TransitionView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    bool doubleclic (glm::vec2) override;
    void arrow (glm::vec2) override;
    Cursor over (glm::vec2) override;
    Cursor drag (glm::vec2, glm::vec2) override;

    void attach(SessionFileSource *ts);
    Session *detach();
    void play(bool open);
    void open();
    void cancel();

private:
    Surface *output_surface_;
    Symbol *fastopenicon;
    Mesh *mark_100ms_, *mark_1s_;
    Switch *gradient_;
    SessionFileSource *transition_source_;
    bool transition_cross_fade;
};


#endif // TRANSITIONVIEW_H
