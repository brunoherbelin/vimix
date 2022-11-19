#ifndef __IMGUI_TOOLKIT_H_
#define __IMGUI_TOOLKIT_H_

#include <glib.h>
#include <string>
#include <vector>

#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "IconsFontAwesome5.h"

namespace ImGuiToolkit
{
    // Icons from resource icon.dds
    void Icon (int i, int j, bool enabled = true);
    bool IconButton (int i, int j, const char *tooltips = nullptr);
    bool IconButton (const char* icon, const char *tooltips = nullptr);
    bool IconMultistate (std::vector<std::pair<int, int> > icons, int* state, std::vector<std::string> tooltips);
    bool IconToggle (int i, int j, int i_toggle, int j_toggle, bool* toggle, const char *tooltips[] = nullptr);
    void ShowIconsWindow(bool* p_open);

    // buttons and gui items with icon
    bool ButtonIcon (int i, int j, const char* tooltip = nullptr);
    bool ButtonIconToggle (int i, int j, int i_toggle, int j_toggle, bool* toggle, const char *tooltip = nullptr);
    bool ButtonIconMultistate (std::vector<std::pair<int, int> > icons, int* state, std::vector<std::string> tooltips);
    bool MenuItemIcon (int i, int j, const char* label, bool selected = false, bool enabled = true);
    bool SelectableIcon(const char* label, int i, int j, bool selected = false);
    bool ComboIcon (std::vector<std::pair<int, int> > icons, std::vector<std::string> labels, int* state);
    bool ComboIcon (const char* label, std::vector<std::pair<int, int> > icons, std::vector<std::string> items, int* i);

    // buttons
    bool ButtonToggle  (const char* label, bool* toggle, const char *tooltip = nullptr);
    bool ButtonSwitch  (const char* label, bool* toggle, const char *shortcut = nullptr);
    void ButtonOpenUrl (const char* label, const char* url, const ImVec2& size_arg = ImVec2(0,0));

    // tooltip and mouse over help
    void setToolTipsEnabled (bool on);
    bool toolTipsEnabled ();
    void ToolTip    (const char* desc, const char* shortcut = nullptr);
    void HelpToolTip(const char* desc, const char* shortcut = nullptr);
    void Indication (const char* desc, const char* icon, const char* shortcut = nullptr);
    void Indication (const char* desc, int i, int j, const char* shortcut = nullptr);

    // sliders
    bool SliderTiming (const char* label, uint *ms, uint v_min, uint v_max, uint v_step, const char* text_max = nullptr);
    bool TimelineSlider (const char* label, guint64 *time, guint64 begin, guint64 first, guint64 end, guint64 step, const float width, double tempo = 0, double quantum = 0);
    void RenderTimeline (ImVec2 min_bbox, ImVec2 max_bbox, guint64 begin, guint64 end, guint64 step, bool verticalflip = false);
    void RenderTimelineBPM (ImVec2 min_bbox, ImVec2 max_bbox, double tempo, double quantum, guint64 begin, guint64 end, guint64 step, bool verticalflip = false);
    bool InvisibleSliderInt(const char* label, uint *index, uint min, uint max, const ImVec2 size);
    bool InvisibleSliderFloat(const char* label, float *index, float min, float max, const ImVec2 size);
    bool EditPlotLines(const char* label, float *array, int values_count, float values_min, float values_max, const ImVec2 size);
    bool EditPlotHistoLines(const char* label, float *histogram_array, float *lines_array, int values_count, float values_min, float values_max, guint64 begin, guint64 end, bool cut, bool *released, const ImVec2 size);
    void ShowPlotHistoLines(const char* label, float *histogram_array, float *lines_array, int values_count, float values_min, float values_max, const ImVec2 size);

    void ValueBar(float fraction, const ImVec2& size_arg);

    // fonts from resources 'fonts/'
    typedef enum {
        FONT_DEFAULT =0,
        FONT_BOLD,
        FONT_ITALIC,
        FONT_MONO,
        FONT_LARGE
    } font_style;
    void SetFont (font_style type, const std::string &ttf_font_name, int pointsize, int oversample = 2);
    void PushFont (font_style type);
    void ImageGlyph(font_style type, char c, float h = 60);
    void Spacing();

    // text input
    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flag = ImGuiInputTextFlags_CharsNoBlank);
    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0));
    void TextMultiline(const char* label, const std::string &str, float width);

    bool InputCodeMultiline(const char* label, std::string *str, const ImVec2& size = ImVec2(0, 0), int *numline = NULL);
    void CodeMultiline(const char* label, const std::string &str, float width);

    // accent color of UI
    typedef enum {
        ACCENT_BLUE =0,
        ACCENT_ORANGE,
        ACCENT_GREY
    } accent_color;
    void SetAccentColor (accent_color color);
    struct ImVec4 HighlightColor (bool active = true);

    // varia
    void WindowText(const char* window_name, ImVec2 window_pos, const char* text);
    bool WindowButton(const char* window_name, ImVec2 window_pos, const char* text);
    void WindowDragFloat(const char* window_name, ImVec2 window_pos, float* v, float v_speed, float v_min, float v_max, const char* format);

}

#endif // __IMGUI_TOOLKIT_H_
