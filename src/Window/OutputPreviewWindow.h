#ifndef OUTPUTPREVIEWWINDOW_H
#define OUTPUTPREVIEWWINDOW_H

#include "Toolkit/DialogToolkit.h"
#include "WorkspaceWindow.h"

class VideoRecorder;
class ShmdataBroadcast;
class Loopback;

class OutputPreviewWindow : public WorkspaceWindow
{
    // frame grabbers
    VideoRecorder *video_recorder_;

    // delayed trigger for recording
    std::vector< std::future<VideoRecorder *> > _video_recorders;

    // dialog to select record location
    DialogToolkit::OpenFolderDialog *recordFolderDialog;

    // magnifying glass
    bool magnifying_glass;

public:
    OutputPreviewWindow();

    void ToggleRecord(bool save_and_continue = false);
    void ToggleRecordPause();
    inline bool isRecording() const { return video_recorder_ != nullptr; }

    void ToggleVideoBroadcast();
    void ToggleSharedMemory();
    bool ToggleLoopbackCamera();

    void Render();
    void setVisible(bool on);

    // from WorkspaceWindow
    void Update() override;
    bool Visible() const override;
};

#endif // OUTPUTPREVIEWWINDOW_H
