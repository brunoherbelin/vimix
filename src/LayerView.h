#ifndef LAYERVIEW_H
#define LAYERVIEW_H

#include "View.h"

class LayerView : public View
{
public:
    LayerView();
    // non assignable class
    LayerView(LayerView const&) = delete;
    LayerView& operator=(LayerView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void arrow (glm::vec2) override;

    float setDepth (Source *, float d = -1.f);

private:
    void updateSelectionOverlay(glm::vec4 color) override;

    float aspect_ratio;
    Mesh *persp_left_, *persp_right_;
    Group *frame_;

};

#endif // LAYERVIEW_H
