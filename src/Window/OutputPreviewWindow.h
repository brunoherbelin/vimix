#ifndef OUTPUTPREVIEWWINDOW_H
#define OUTPUTPREVIEWWINDOW_H

#include "Toolkit/DialogToolkit.h"
#include "WorkspaceWindow.h"
#include "FrameGrabber.h"

class VideoRecorder;
class ShmdataBroadcast;
class Loopback;

class OutputPreviewWindow : public WorkspaceWindow
{
    FrameGrabber::Type recorder_type_;

    // dialog to select record location
    DialogToolkit::OpenFolderDialog *recordFolderDialog;

    // magnifying glass
    bool magnifying_glass;

public:
    OutputPreviewWindow();

    void ToggleRecord(bool save_and_continue = false);
    void ToggleRecordPause();

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
