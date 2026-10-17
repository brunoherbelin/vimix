#ifndef PRESENTATIONVIEW_H
#define PRESENTATIONVIEW_H

#include <vector>

#include "Playlist.h"
#include "View.h"

class SessionThumbnailSource;

class PresentationView : public View
{
public:
    PresentationView();
    // non assignable class
    PresentationView(PresentationView const&) = delete;
    PresentationView& operator=(PresentationView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    void recenter () override {}
    bool canSelect(Source *) override { return false; }
    void select(glm::vec2, glm::vec2) override {}
    void selectAll() override {}

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    bool doubleclic (glm::vec2) override { return true; }
    void arrow (glm::vec2) override;
    Cursor over (glm::vec2) override;
    Cursor drag (glm::vec2, glm::vec2) override { return Cursor(); }
    void terminate (bool force = false) override;

    // set the list of sessions to present, and its name (default to name of playlist file)
    void setPlaylist(const Playlist &playlist, const std::string &name = "");
    // open next, previous or i-th session of the list
    void next();
    void previous();
    void open(int i);
    // finish any transition, free the list and restore workspace
    void leave(bool restore_workspace = true);

private:
    void clearItems();
    void setCurrent(int i, bool animate);
    int  indexOf(const std::string &path) const;
    int  nextIndex() const;
    int  previousIndex() const;
    bool busy() const;
    void layout();
    void updateTransition(float dt);
    bool validTransition();

    Playlist playlist_;
    std::string playlist_name_;
    std::vector<SessionThumbnailSource *> items_;
    int current_;
    std::string live_filename_;

    Group *strip_;
    bool strip_animate_;
    float strip_scroll_;
    glm::vec3 strip_target_;
    float item_height_;

    Surface *output_surface_;
    Frame *output_frame_;

    SessionFileSource *transition_source_;
    float transition_d_;
    bool transition_cross_fade_;

    bool arrow_latched_;
    int64_t start_time_;
};


#endif // PRESENTATIONVIEW_H
