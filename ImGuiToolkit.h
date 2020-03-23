#ifndef __IMGUI_TOOLKIT_H_
#define __IMGUI_TOOLKIT_H_

#include <glib.h>
#include <string>
#include <list>
#include <utility>
#include "rsc/fonts/IconsFontAwesome5.h"

namespace ImGuiToolkit
{
    // Icons from resource icon.dds
    void Icon(int i, int j); 
    bool ButtonIcon(int i, int j);
    void ShowIconsWindow(bool* p_open);
    void ToggleButton( const char* label, bool& toggle );
    void Bar(float value, float in, float out, float min, float max, const char* title, bool expand);
    bool TimelineSlider(const char* label, guint64 *time, guint64 duration, guint64 step);
    bool TimelineSliderEdit(const char* label, guint64 *time, guint64 duration, guint64 step,
                        std::list<std::pair<guint64, guint64> >& segments);
    
    // fonts from ressources 'fonts/'
    typedef enum {
        FONT_DEFAULT =0,
        FONT_BOLD,
        FONT_ITALIC,
        FONT_MONO
    } font_style;
    void SetFont(font_style type, const std::string &ttf_font_name, int pointsize);
    void PushFont(font_style type);

    // color of gui items
    typedef enum {
        ACCENT_BLUE =0,
        ACCENT_ORANGE,
        ACCENT_GREY
    } accent_color;
    void SetAccentColor(accent_color color);

    void OpenWebpage( const char* url );
}

#endif // __IMGUI_TOOLKIT_H_
