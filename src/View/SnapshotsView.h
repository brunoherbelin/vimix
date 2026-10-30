#ifndef SNAPSHOTSVIEW_H
#define SNAPSHOTSVIEW_H

#include <list>

#include "StripView.h"

class Session;

// View to step through the versions (snapshots) of the live session,
// with an animation from one version to the other
class SnapshotsView : public StripView
{
public:
    SnapshotsView();

    // set the list of versions of the live session
    void setSnapshots();
    // open i-th version of the list
    void open(int i) override;
    // finish any animation, free the list and restore workspace
    void leave(bool restore_workspace = true) override;

protected:
    bool busy() const override;
    bool isOpened(int i) const override;
    std::string label(int i) const override;
    std::string title() const override { return title_; }
    void sync() override;
    void drawInformation(bool is_busy) override;
    void drawButtons() override;

private:
    int indexOf(uint64_t id) const;
    void build();

    // ids of versions, in the order of items
    std::list<uint64_t> snapshots_;
    std::vector<uint64_t> ids_;
    std::vector<std::string> labels_;
    Session *session_;
    std::string title_;
    std::string date_;
    uint64_t date_id_;
};

#endif // SNAPSHOTSVIEW_H
