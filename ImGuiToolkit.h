#ifndef __IMGUI_TOOLKIT_H_
#define __IMGUI_TOOLKIT_H_

#include <glib.h>
#include <string>
#include <list>
#include <vector>
#include <utility>
#include "rsc/fonts/IconsFontAwesome5.h"

namespace ImGuiToolkit
{
    // Icons from resource icon.dds
    void Icon(int i, int j);
    void ShowIconsWindow(bool* p_open);

    // utility buttons
    bool ButtonIcon (int i, int j);
    bool ButtonIconToggle (int i, int j, int i_toggle, int j_toggle, bool* toggle);
    bool ButtonIconMultistate (std::vector<std::pair<int, int> > icons, int* state);
    void ButtonToggle (const char* label, bool* toggle);
    void ButtonSwitch (const char* label, bool* toggle , const char *help = nullptr);
    bool IconToggle (int i, int j, int i_toggle, int j_toggle, bool* toggle, const char *tooltips[] = nullptr);
    void ButtonOpenUrl (const char* url, const ImVec2& size_arg = ImVec2(0,0));

    void HelpMarker (const char* desc);

    // utility sliders
    void Bar (float value, float in, float out, float min, float max, const char* title, bool expand);
    bool TimelineSlider (const char* label, guint64 *time, guint64 duration, guint64 step);
    bool TimelineSliderEdit (const char* label, guint64 *time, guint64 duration, guint64 step,
                        std::list<std::pair<guint64, guint64> >& segments);
    
    // fonts from ressources 'fonts/'
    typedef enum {
        FONT_DEFAULT =0,
        FONT_BOLD,
        FONT_ITALIC,
        FONT_MONO,
        FONT_LARGE
    } font_style;
    void SetFont (font_style type, const std::string &ttf_font_name, int pointsize, int oversample = 2);
    void PushFont (font_style type);

    void WindowText(const char* window_name, ImVec2 window_pos, const char* text);
    bool WindowButton(const char* window_name, ImVec2 window_pos, const char* text);
    void WindowDragFloat(const char* window_name, ImVec2 window_pos, float* v, float v_speed, float v_min, float v_max, const char* format);


    // color of gui items
    typedef enum {
        ACCENT_BLUE =0,
        ACCENT_ORANGE,
        ACCENT_GREY
    } accent_color;
    void SetAccentColor (accent_color color);
    struct ImVec4 GetHighlightColor ();

    void ShowStats (bool* p_open, int* p_corner);

}

#endif // __IMGUI_TOOLKIT_H_
