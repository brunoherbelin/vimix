#ifndef STRIPVIEW_H
#define STRIPVIEW_H

#include <string>
#include <vector>

#include "View.h"

class SessionThumbnailSource;

// Base class of views switching between items of a list (e.g. sessions, versions)
// showing the output, a strip of thumbnails of the items, and an information window.
// NB: update, layout and leave never call ImGui (they can be called after its termination)
class StripView : public View
{
public:
    StripView(Mode m, const std::string &item_name);
    // non assignable class
    StripView(StripView const&) = delete;
    StripView& operator=(StripView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    void recenter () override {}
    bool canSelect(Source *) override { return false; }
    void select(glm::vec2, glm::vec2) override {}
    void selectAll() override {}

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    bool doubleclic (glm::vec2) override;
    void arrow (glm::vec2) override;
    Cursor over (glm::vec2) override;
    Cursor drag (glm::vec2, glm::vec2) override { return Cursor(); }
    void terminate (bool force = false) override;

    // open next, previous or i-th item of the list
    void next();
    void previous();
    virtual void open(int i) = 0;
    // finish any transition, free the list and restore workspace
    virtual void leave(bool restore_workspace = true) = 0;

protected:
    // the list is being changed (no navigation)
    virtual bool busy() const = 0;
    // the i-th item is the current state of the live session
    virtual bool isOpened(int i) const = 0;
    // the i-th item can be opened
    virtual bool available(int) const { return true; }
    virtual std::string unavailableReason() const { return "Not available"; }
    // name of the i-th item, and of the list
    virtual std::string label(int i) const = 0;
    virtual std::string title() const = 0;
    // called at the start of update (e.g. to follow changes of the live session)
    virtual void sync() {}
    // called during update to perform transition
    virtual void animate(float) {}
    // content of information window, below the title
    virtual void drawInformation(bool is_busy) = 0;
    // line of buttons at the bottom of information window
    virtual void drawButtons() = 0;

    void clearItems();
    void addItem(SessionThumbnailSource *item);
    // start of a new list: jump to the current item
    void restart();
    void setCurrent(int i, bool animate);
    int  nextIndex() const;
    int  previousIndex() const;

    std::vector<SessionThumbnailSource *> items_;
    int current_;
    // leave the view once this item is opened (double clic)
    int leave_target_;

private:
    void layout();

    std::string item_name_;

    Group *strip_;
    bool strip_animate_;
    float strip_scroll_;
    glm::vec3 strip_target_;
    float item_height_;

    Surface *output_surface_;
    Frame *output_frame_;

    bool arrow_latched_;
    int64_t start_time_;
};

#endif // STRIPVIEW_H
