#ifndef __NAVIGATOR_H_
#define __NAVIGATOR_H_

#include <string>
#include <list>
#include <vector>
#include <utility>

#include "Source/Source.h"
#include "NavigatorWidgets.h"
#include "NewSourcePanel.h"
#include "PlaylistPanel.h"
#include "SourcePanel.h"
#include "SessionPanel.h"
#include "SettingsPanel.h"

struct ImVec2;

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
    void clearButtonSelection();
    void applyButtonSelection(int index);

    // side pannels
    void RenderMainPannel(const ImVec2 &iconsize);
    void RenderTransitionPannel(const ImVec2 &iconsize);
    void RenderViewOptions(uint *timeout, const ImVec2 &pos, const ImVec2 &size);
    bool RenderMousePointerSelector(const ImVec2 &size);

public:

    // the pannel creating new sources owns these types
    typedef NewSourcePanel::NewSourceType NewSourceType;
    static constexpr NewSourceType SOURCE_FILE      = NewSourcePanel::SOURCE_FILE;
    static constexpr NewSourceType SOURCE_SEQUENCE  = NewSourcePanel::SOURCE_SEQUENCE;
    static constexpr NewSourceType SOURCE_CONNECTED = NewSourcePanel::SOURCE_CONNECTED;
    static constexpr NewSourceType SOURCE_GENERATED = NewSourcePanel::SOURCE_GENERATED;
    static constexpr NewSourceType SOURCE_BUNDLE    = NewSourcePanel::SOURCE_BUNDLE;

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

    typedef NewSourcePanel::MediaCreateMode MediaCreateMode;
    static constexpr MediaCreateMode MEDIA_RECENT    = NewSourcePanel::MEDIA_RECENT;
    static constexpr MediaCreateMode MEDIA_RECORDING = NewSourcePanel::MEDIA_RECORDING;
    static constexpr MediaCreateMode MEDIA_FOLDER    = NewSourcePanel::MEDIA_FOLDER;
    void setNewMedia(MediaCreateMode mode, std::string path = std::string());

    // source to be replaced by the next created source
    inline void setSourceToReplace(Source *s) { source_to_replace = s; }
    inline Source *&sourceToReplace() { return source_to_replace; }


private:
    // side pannels of the main pannel
    SessionPanel session_panel_;
    PlaylistPanel playlist_panel_;
    SourcePanel source_panel_;
    SettingsPanel settings_panel_;

    NewSourcePanel new_source_panel_;

    // source to be replaced by the next created source
    Source *source_to_replace;

};

#endif /* __NAVIGATOR_H_ */
