#ifndef PRESENTATIONVIEW_H
#define PRESENTATIONVIEW_H

#include "Playlist.h"
#include "StripView.h"

class SessionFileSource;

class PresentationView : public StripView
{
public:
    PresentationView();

    // set the list of sessions to present, and its name (default to name of playlist file)
    void setPlaylist(const Playlist &playlist, const std::string &name = "");
    // open i-th session of the list
    void open(int i) override;
    // finish any transition, free the list and restore workspace
    void leave(bool restore_workspace = true) override;

protected:
    bool busy() const override;
    bool isOpened(int i) const override;
    bool available(int i) const override;
    std::string unavailableReason() const override { return "Missing file"; }
    std::string label(int i) const override;
    std::string title() const override { return playlist_name_; }
    void sync() override;
    void animate(float dt) override;
    void drawInformation(bool is_busy) override;
    void drawButtons() override;

private:
    int  indexOf(const std::string &path) const;
    bool validTransition();

    Playlist playlist_;
    std::string playlist_name_;
    std::string live_filename_;

    SessionFileSource *transition_source_;
    float transition_d_;
    bool transition_cross_fade_;
};


#endif // PRESENTATIONVIEW_H
