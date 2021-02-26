#ifndef TRANSITIONVIEW_H
#define TRANSITIONVIEW_H

#include "View.h"

class TransitionView : public View
{
public:
    TransitionView();

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void arrow (glm::vec2) override;
    Cursor drag (glm::vec2, glm::vec2) override;

    void attach(SessionFileSource *ts);
    Session *detach();
    void play(bool open);

private:
    Surface *output_surface_;
    Mesh *mark_100ms_, *mark_1s_;
    Switch *gradient_;
    SessionFileSource *transition_source_;
};


#endif // TRANSITIONVIEW_H
