#ifndef OVERLAY_H
#define OVERLAY_H

#include "View/View.h"

class Overlay
{
public:
    Overlay();

    virtual void update (float dt) = 0;
    virtual void draw () = 0;

    virtual std::pair<Node *, glm::vec2> pick(glm::vec2) {
        return { nullptr, glm::vec2(0.f) };
    }

    virtual View::Cursor grab (Source*, glm::vec2, glm::vec2, std::pair<Node *, glm::vec2>) {
        return View::Cursor ();
    }

    virtual View::Cursor over (glm::vec2) {
        return View::Cursor ();
    }
};


class SnapshotOverlay: public Overlay
{
public:
    SnapshotOverlay();

    void draw () override;
    void update (float dt) override;
};


#endif // OVERLAY_H
