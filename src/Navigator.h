#ifndef __NAVIGATOR_H_
#define __NAVIGATOR_H_

#include <string>
#include <list>
#include <vector>
#include <utility>

#include "Source/Source.h"

struct ImVec2;

class SourcePreview
{
    Source *source_;
    std::string label_;
    bool reset_;

public:
    SourcePreview();

    void setSource(Source *s = nullptr, const std::string &label = "");
    Source *getSource();

    void Render(float width);
    bool ready() const;
    inline bool filled() const { return source_ != nullptr; }
};

class Thumbnail
{
    float aspect_ratio_;
    uint texture_;

public:
    Thumbnail();
    ~Thumbnail();

    void reset();
    void fill (const FrameBufferImage *image);
    bool filled();
    void Render(float width);
};

class Navigator
{
    // geometry left bar & pannel
    float width_;
    float height_;
    float pannel_width_;
    float padding_width_;

    // behavior pannel
    bool pannel_visible_;
    int  pannel_main_mode_;
    float pannel_alpha_;
    bool view_pannel_visible;
    bool selected_button[68];  // NAV_COUNT
    int  selected_index;
    int  pattern_type;
    int  generated_type;
    bool custom_connected;
    bool custom_screencapture;
    void clearButtonSelection();
    void clearNewPannel();
    void applyButtonSelection(int index);

    // side pannels
    void RenderSourcePannel(Source *s, const ImVec2 &iconsize, bool reset = false);
    void RenderMainPannel(const ImVec2 &iconsize);
    void RenderMainPannelSession();
    void RenderMainPannelPlaylist();
    void RenderMainPannelSettings();
    void RenderTransitionPannel(const ImVec2 &iconsize);
    void RenderNewPannel(const ImVec2 &iconsize);
    void RenderViewOptions(uint *timeout, const ImVec2 &pos, const ImVec2 &size);
    bool RenderMousePointerSelector(const ImVec2 &size);

public:

    typedef enum {
        SOURCE_FILE = 0,
        SOURCE_SEQUENCE,
        SOURCE_CONNECTED,
        SOURCE_GENERATED,
        SOURCE_BUNDLE
    } NewSourceType;

    Navigator();
    void Render();

    bool pannelVisible();
    void discardPannel();
    void showPannelSource(int index);
    int  selectedPannelSource();
    void togglePannelMenu();
    void togglePannelNew();
    void showConfig();
    void togglePannelAutoHide();

    typedef enum {
        MEDIA_RECENT = 0,
        MEDIA_RECORDING,
        MEDIA_FOLDER
    } MediaCreateMode;
    void setNewMedia(MediaCreateMode mode, std::string path = std::string());


private:
    // for new source panel
    SourcePreview new_source_preview_;
    std::list<std::string> sourceSequenceFiles;
    std::list<std::string> sourceMediaFiles;
    std::string sourceMediaFileCurrent;
    MediaCreateMode new_media_mode;
    bool new_media_mode_changed;
    Source *source_to_replace;

    static std::vector< std::pair<int, int> > icons_ordering_files;
    static std::vector< std::string > tooltips_ordering_files;
};

#endif /* __NAVIGATOR_H_ */
