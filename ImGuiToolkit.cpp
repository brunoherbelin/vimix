/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <map>
#include <algorithm>
#include <string>
#include <cctype>

#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif

#define MILISECOND 1000000UL
#define SECOND 1000000000UL
#define MINUTE 60000000000UL

#include <glad/glad.h>

#include "Resource.h"
#include "GstToolkit.h"
#include "SystemToolkit.h"

#include "ImGuiToolkit.h"

bool tooltips_enabled = true;
unsigned int textureicons = 0;
std::map <ImGuiToolkit::font_style, ImFont*>fontmap;


void ImGuiToolkit::ButtonOpenUrl( const char* label, const char* url, const ImVec2& size_arg )
{
    char _label[512];
    sprintf( _label, "%s  %s", ICON_FA_EXTERNAL_LINK_ALT, label );

    if ( ImGui::Button(_label, size_arg) )
        SystemToolkit::open(url);
}


bool ImGuiToolkit::ButtonToggle( const char* label, bool* toggle, const char *tooltip)
{
    const ImVec4* colors = ImGui::GetStyle().Colors;
    const auto active = *toggle;
    if( active ) {
        ImGui::PushStyleColor( ImGuiCol_Button, colors[ImGuiCol_TabActive] );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colors[ImGuiCol_TabHovered] );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, colors[ImGuiCol_Tab] );
    }
    bool action = ImGui::Button( label );
    if (tooltip != nullptr && ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(tooltip);
    if( action ) *toggle = !*toggle;
    if( active ) ImGui::PopStyleColor( 3 );
    return action;
}


bool ImGuiToolkit::ButtonSwitch(const char* label, bool* toggle, const char* shortcut)
{
    bool ret = false;

    // utility style
    ImVec4* colors = ImGui::GetStyle().Colors;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // draw position when entering
    ImVec2 draw_pos = ImGui::GetCursorScreenPos();

    // layout
    float frame_height = ImGui::GetFrameHeight();
    float frame_width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetFrameHeight() * 0.75f;
    float width = height * 1.6f;
    float radius = height * 0.50f;

    // toggle action : operate on the whole area
    ImGui::InvisibleButton(label, ImVec2(frame_width, frame_height));
    if (ImGui::IsItemClicked()) {
        *toggle = !*toggle;
        ret = true;
    }
    float t = *toggle ? 1.0f : 0.0f;

    // animation
    ImGuiContext& g = *GImGui;
    const float ANIM_SPEED = 0.1f;
    if (g.LastActiveId == g.CurrentWindow->GetID(label))// && g.LastActiveIdTimer < ANIM_SPEED)
    {
        float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
        t = *toggle ? (t_anim) : (1.0f - t_anim);
    }

    // hover
    ImU32 col_bg;
    if (ImGui::IsItemHovered()) //
        col_bg = ImGui::GetColorU32(ImLerp(colors[ImGuiCol_FrameBgHovered], colors[ImGuiCol_TabHovered], t));
    else
        col_bg = ImGui::GetColorU32(ImLerp(colors[ImGuiCol_FrameBg], colors[ImGuiCol_TabActive], t));

    // draw help text if present
    if (shortcut) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
        ImGui::RenderText(draw_pos, shortcut);
        ImGui::PopStyleColor(1);
    }

    // draw the label right aligned
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 text_pos = draw_pos + ImVec2(frame_width -3.5f * ImGui::GetTextLineHeightWithSpacing() -label_size.x, 0.f);
    ImGui::RenderText(text_pos, label);

    // draw switch after the text
    ImVec2 p = draw_pos + ImVec2(frame_width -3.1f * ImGui::GetTextLineHeightWithSpacing(), 0.f);
    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 250));

    return ret;
}


void ImGuiToolkit::Icon(int i, int j, bool enabled)
{
    // icons.dds is a 20 x 20 grid of icons 
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");

    ImVec2 uv0( static_cast<float>(i) * 0.05, static_cast<float>(j) * 0.05 );
    ImVec2 uv1( uv0.x + 0.05, uv0.y + 0.05 );

    ImVec4 tint_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
    if (!enabled)
        tint_color = ImVec4(0.6f, 0.6f, 0.6f, 0.8f);
    ImGui::Image((void*)(intptr_t)textureicons, ImVec2(ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeightWithSpacing()), uv0, uv1, tint_color);
}

bool ImGuiToolkit::ButtonIcon(int i, int j, const char *tooltip)
{
    // icons.dds is a 20 x 20 grid of icons 
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");

    ImVec2 uv0( static_cast<float>(i) * 0.05, static_cast<float>(j) * 0.05 );
    ImVec2 uv1( uv0.x + 0.05, uv0.y + 0.05 );

    ImGui::PushID( i*20 + j);
    ImGuiContext& g = *GImGui;
    bool ret =  ImGui::ImageButton((void*)(intptr_t)textureicons,
                                   ImVec2(g.FontSize, g.FontSize),
                                   uv0, uv1, g.Style.FramePadding.y);
    ImGui::PopID();

    if (tooltip != nullptr && ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(tooltip);

    return ret;
}

bool ImGuiToolkit::ButtonIconToggle(int i, int j, int i_toggle, int j_toggle, bool* toggle, const char *tooltip)
{
    bool ret = false;
    ImGui::PushID( i * 20 + j + i_toggle * 20 + j_toggle);

    const ImVec4* colors = ImGui::GetStyle().Colors;
    const auto active = *toggle;
    if( active ) {
        ImGui::PushStyleColor( ImGuiCol_Button, colors[ImGuiCol_TabActive] );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colors[ImGuiCol_TabHovered] );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, colors[ImGuiCol_Tab] );

        if ( ButtonIcon(i_toggle, j_toggle)) {
            *toggle = false;
            ret = true;
        }
        ImGui::PopStyleColor( 3 );
    }
    else {
        if ( ButtonIcon(i, j)) {
            *toggle = true;
            ret = true;
        }
    }

    if (tooltip != nullptr && ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(tooltip);

    ImGui::PopID();
    return ret;
}


bool ImGuiToolkit::IconButton(int i, int j, const char *tooltip)
{
    ImGui::PushID( i * 20 + j );

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // duplicate of ImGui::InvisibleButton to handle ImGuiButtonFlags_Repeat
    const ImGuiID id = window->GetID("##iconijbutton");
    float h = ImGui::GetFrameHeight();
    ImVec2 size = ImGui::CalcItemSize(ImVec2(h, h), 0.0f, 0.0f);
    ImVec2 draw_pos = window->DC.CursorPos;
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(size);

    if (!ImGui::ItemAdd(bb, id)){
        ImGui::PopID();
        return false;
    }

    ImGuiButtonFlags flags = 0;
    if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

    // tooltip
    if (tooltip != nullptr && hovered)
        ImGuiToolkit::ToolTip(tooltip);

    // draw icon
    ImGui::SetCursorScreenPos(draw_pos);
    Icon(i, j, !pressed);

    ImGui::PopID();
    return pressed;
}


bool ImGuiToolkit::IconButton(const char* icon, const char *tooltip)
{
    bool ret = false;
    ImGui::PushID( icon );

    float frame_height = ImGui::GetFrameHeight();
    float frame_width = frame_height;

    ImVec2 draw_pos = ImGui::GetCursorScreenPos();

    // toggle action : operate on the whole area
    ImGui::InvisibleButton("##iconcharbutton", ImVec2(frame_width, frame_height));
    if (ImGui::IsItemClicked())
        ret = true;

    ImGui::SetCursorScreenPos(draw_pos);
    ImGui::Text(icon);

    if (tooltip != nullptr && ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(tooltip);

    ImGui::PopID();
    return ret;
}


bool ImGuiToolkit::IconToggle(int i, int j, int i_toggle, int j_toggle, bool* toggle, const char *tooltips[])
{
    bool ret = false;
    ImGui::PushID( i * 20 + j + i_toggle * 20 + j_toggle);

    float frame_height = ImGui::GetFrameHeight();
    float frame_width = frame_height;
    ImVec2 draw_pos = ImGui::GetCursorScreenPos();

    // toggle action : operate on the whole area
    ImGui::InvisibleButton("##icontoggle", ImVec2(frame_width, frame_height));
    if (ImGui::IsItemClicked()) {
        *toggle = !*toggle;
        ret = true;
    }

    ImGui::SetCursorScreenPos(draw_pos);
    if (*toggle) {
        Icon(i_toggle, j_toggle, !ret);
    }
    else {
        Icon(i, j, !ret);
    }

    int tooltipid = *toggle ? 1 : 0;
    if (tooltips != nullptr && tooltips[tooltipid] != nullptr && ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(tooltips[tooltipid]);

    ImGui::PopID();
    return ret;
}


struct Sum
{
    void operator()(std::pair<int, int> n) { sum += n.first * 20 + n.second; }
    int sum{0};
};

bool ImGuiToolkit::ButtonIconMultistate(std::vector<std::pair<int, int> > icons, int* state, const char* tooltip)
{
    bool ret = false;
    Sum id = std::for_each(icons.begin(), icons.end(), Sum());
    ImGui::PushID( id.sum );

    int num_button = static_cast<int>(icons.size()) -1;
    int s = CLAMP(*state, 0, num_button);
    if ( ButtonIcon( icons[s].first, icons[s].second, tooltip ) ){
        ++s;
        if (s > num_button)
            *state = 0;
        else
            *state = s;
        ret = true;
    }

    ImGui::PopID();
    return ret;
}

bool ImGuiToolkit::ComboIcon (std::vector<std::pair<int, int> > icons, std::vector<std::string> labels, int* state)
{
    bool ret = false;
    Sum id = std::for_each(icons.begin(), icons.end(), Sum());
    ImGui::PushID( id.sum );

    ImVec2 draw_pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetTextLineHeight();
    ImGui::SetNextItemWidth(w * 2.7f);
    if (ImGui::BeginCombo("##ComboIcon", "  ") )
    {
        char space_buf[] = "                                                ";
        const ImVec2 space_size = ImGui::CalcTextSize(" ", NULL);
        const int space_num = static_cast<int>( ceil(ImGui::GetTextLineHeightWithSpacing() / space_size.x) );
        space_buf[space_num]='\0';

        std::vector<std::pair<int, int> >::iterator it_icon = icons.begin();
        std::vector<std::string>::iterator it_label = labels.begin();
        for(int i = 0 ; it_icon != icons.end(); i++, ++it_icon, ++it_label) {
            ImGui::PushID( id.sum + i + 1);
            ImVec2 pos = ImGui::GetCursorScreenPos();
            // combo selectable item
            char text_buf[256];
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s %s", space_buf, it_label->c_str());
            if ( ImGui::Selectable(text_buf, i == *state )){
                *state = i;
                ret = true;
            }
            // draw item icon
            ImGui::SetCursorScreenPos( pos + ImVec2(w/6.f,0) );
            Icon( (*it_icon).first, (*it_icon).second );
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    // redraw current ad preview value
    ImGui::SetCursorScreenPos(draw_pos + ImVec2(w/9.f,w/9.f));
    Icon(icons[*state].first, icons[*state].second );

    ImGui::PopID();
    return ret;
}

bool ImGuiToolkit::ComboIcon (const char* label, std::vector<std::pair<int, int> > icons, std::vector<std::string> items, int* i)
{
    bool ret = false;
    ImGuiContext& g = *GImGui;
    ImVec2 draw_pos = ImGui::GetCursorScreenPos() + g.Style.FramePadding * 0.5;

    // make some space
    char space_buf[] = "                                                ";
    const ImVec2 space_size = ImGui::CalcTextSize(" ", NULL);
    const int space_num = static_cast<int>( ceil(g.FontSize / space_size.x) );
    space_buf[space_num]='\0';

    char text_buf[256];
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s %s", space_buf, items[*i].c_str());

    if (ImGui::BeginCombo(label, text_buf))
    {
        for (int p = 0; p < (int) items.size(); ++p){
            if (ImGuiToolkit::SelectableIcon( items[p].c_str(), icons[p].first, icons[p].second, p == *i) ) {
                *i = p;
                ret = true;
            }
        }
        ImGui::EndCombo();
    }
    ImVec2 end_pos = ImGui::GetCursorScreenPos();

    // draw icon
    ImGui::SetCursorScreenPos(draw_pos);
    Icon(icons[*i].first, icons[*i].second);

    ImGui::SetCursorScreenPos(end_pos);

    return ret;
}

bool ImGuiToolkit::SelectableIcon(const char* label, int i, int j, bool selected)
{
    ImGuiContext& g = *GImGui;
    ImVec2 draw_pos = ImGui::GetCursorScreenPos() - g.Style.FramePadding * 0.5;

    // make some space
    char space_buf[] = "                                                ";
    const ImVec2 space_size = ImGui::CalcTextSize(" ", NULL);
    const int space_num = static_cast<int>( ceil(g.FontSize / space_size.x) );
    space_buf[space_num]='\0';

    char text_buf[256];
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s %s", space_buf, label);

    // draw menu item
    bool ret = ImGui::Selectable(text_buf, selected);
    ImVec2 end_pos = ImGui::GetCursorScreenPos();

    // draw icon
    ImGui::SetCursorScreenPos(draw_pos);
    Icon(i, j);

    ImGui::SetCursorScreenPos(end_pos);

    return ret;
}


bool ImGuiToolkit::MenuItemIcon (int i, int j, const char* label, bool selected, bool enabled)
{
    ImVec2 draw_pos = ImGui::GetCursorScreenPos();

    // make some space
    char space_buf[] = "                                                ";
    const ImVec2 space_size = ImGui::CalcTextSize(" ", NULL);
    const int space_num = static_cast<int>( ceil(ImGui::GetTextLineHeightWithSpacing() / space_size.x) );
    space_buf[space_num]='\0';

    char text_buf[256];
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s %s", space_buf, label);

    // draw menu item
    bool ret = ImGui::MenuItem(text_buf, NULL, selected, enabled);

    // draw icon
    ImGui::SetCursorScreenPos(draw_pos);
    Icon(i, j, enabled);

    return ret;
}

void ImGuiToolkit::ShowIconsWindow(bool* p_open)
{
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");

    const ImGuiIO& io = ImGui::GetIO();
    
    if ( ImGui::Begin("Icons", p_open) ) {

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Image((void*)(intptr_t)textureicons, ImVec2(640, 640));
        if (ImGui::IsItemHovered())
        {
            float my_tex_w = 640.0;
            float my_tex_h = 640.0;
            float zoom = 4.0f;
            float region_sz = 32.0f; // 64 x 64 icons
            float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
            if (region_x < 0.0f) region_x = 0.0f; else if (region_x > my_tex_w - region_sz) region_x = my_tex_w - region_sz;
            float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
            if (region_y < 0.0f) region_y = 0.0f; else if (region_y > my_tex_h - region_sz) region_y = my_tex_h - region_sz;
            ImGui::BeginTooltip();
            int i = (int) ( (region_x + region_sz * 0.5f) / region_sz);
            int j = (int) ( (region_y + region_sz * 0.5f)/ region_sz);
            ImGuiToolkit::Icon(i, j);
            ImGui::SameLine();
            ImGui::Text(" Icon (%d, %d)", i,  j);
            ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
            ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);
            ImGui::Image((void*)(intptr_t)textureicons, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
            ImGui::EndTooltip();
        }

        ImGui::End();
    }
}


void ImGuiToolkit::setToolTipsEnabled (bool on)
{
    tooltips_enabled = on;
}

bool ImGuiToolkit::toolTipsEnabled ()
{
    return tooltips_enabled;
}

void ImGuiToolkit::ToolTip(const char* desc, const char* shortcut)
{
    if (!tooltips_enabled)
        return;

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 14.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();

    if (shortcut) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
        ImGui::Text(shortcut);
        ImGui::PopStyleColor();
    }
    ImGui::EndTooltip();
    ImGui::PopFont();
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
void ImGuiToolkit::HelpMarker(const char* desc, const char* icon, const char* shortcut)
{
    if (tooltips_enabled) {
        ImGui::TextDisabled( icon );
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(desc, shortcut);
    }
    else
        ImGui::Text(" ");
}

void ImGuiToolkit::HelpIcon(const char* desc, int i, int j, const char* shortcut)
{
    if (tooltips_enabled) {
        ImGuiToolkit::Icon(i, j, false);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(desc, shortcut);
    }
    else
        ImGui::Text(" ");
}


bool ImGuiToolkit::SliderTiming (const char* label, uint* ms, uint v_min, uint v_max, uint v_step, const char* text_max)
{
    char text_buf[256];
    if ( *ms < v_max || text_max == nullptr)
    {
        int milisec = (*ms)%1000;
        int sec = (*ms)/1000;
        int min = sec/60;

        if (min > 0) {
            if (milisec>0)
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d min %02d s %03d ms", min, sec%60, milisec);
            else
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d min %02d s", min, sec%60);
        }
        else if (sec > 0) {
            if (milisec>0)
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d s %03d ms", sec, milisec);
            else
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d s", sec);
        }
        else
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%03d ms", milisec);
    }
    else {
        ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", text_max);
    }

//    int val = *ms / v_step;
//    bool ret = ImGui::SliderInt(label, &val, v_min / v_step, v_max / v_step, text_buf);
//    *ms = val * v_step;

    // quadratic scale
    float val = *ms / v_step;
    bool ret = ImGui::SliderFloat(label, &val, v_min / v_step, v_max / v_step, text_buf, 2.f);
    *ms = int(floor(val)) * v_step;

    return ret;
}

// Draws a timeline showing
// 1) a cursor at position *time in the range [0 duration]
// 2) a line of tick marks indicating time, every step if possible
// 3) a slider handle below the cursor: user can move the slider
// Behavior
// a) Returns TRUE if the left mouse button LMB is pressed over the timeline
// b) the value of *time is changed to the position of the slider handle from user input (LMB)

#define NUM_MARKS 10
#define LARGE_TICK_INCREMENT 1
#define LABEL_TICK_INCREMENT 3

void ImGuiToolkit::RenderTimeline (ImGuiWindow* window, ImRect timeline_bbox,
                                   guint64 begin, guint64 end, guint64 step, bool verticalflip)
{
    static guint64 optimal_tick_marks[NUM_MARKS + LABEL_TICK_INCREMENT] = { 100 * MILISECOND, 500 * MILISECOND, 1 * SECOND, 2 * SECOND, 5 * SECOND, 10 * SECOND, 20 * SECOND, 1 * MINUTE, 2 * MINUTE, 5 * MINUTE, 10 * MINUTE, 60 * MINUTE, 60 * MINUTE };

    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float fontsize = g.FontSize;
    const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);

    // by default, put a tick mark at every frame step and a large mark every second
    guint64 tick_step = step;
    guint64 large_tick_step = optimal_tick_marks[1+LARGE_TICK_INCREMENT];
    guint64 label_tick_step = optimal_tick_marks[1+LABEL_TICK_INCREMENT];
    guint64 tick_delta = 0;

    // keep duration
    const guint64 duration = end - begin;

    // how many pixels to represent one frame step?
    const float step_ = static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
    float tick_step_pixels = timeline_bbox.GetWidth() * step_;

    // large space
    if (tick_step_pixels > 5.f && step > 0)
    {
        // try to put a label ticks every second
        label_tick_step = (SECOND / step) * step;
        large_tick_step = label_tick_step % 5 ? (label_tick_step % 2 ?  label_tick_step : label_tick_step / 2 ) : label_tick_step / 5;
        tick_delta = SECOND - label_tick_step;

        // round to nearest
        if (tick_delta > step / 2) {
            label_tick_step += step;
            large_tick_step += step;
            tick_delta = SECOND - label_tick_step;
        }
    }
    else {
        // while there is less than 5 pixels between two tick marks (or at last optimal tick mark)
        for ( int i=0; i<10 && tick_step_pixels < 5.f; ++i )
        {
            // try to use the optimal tick marks pre-defined
            tick_step = optimal_tick_marks[i];
            large_tick_step = optimal_tick_marks[i+LARGE_TICK_INCREMENT];
            label_tick_step = optimal_tick_marks[i+LABEL_TICK_INCREMENT];
            tick_step_pixels = timeline_bbox.GetWidth() * static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
        }
    }

    // render tics and text
    char text_buf[24];

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);

    // render tick and text END
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s",
                   GstToolkit::time_to_string(end, GstToolkit::TIME_STRING_MINIMAL).c_str());
    ImVec2 duration_label_size = ImGui::CalcTextSize(text_buf, NULL);
    ImVec2 duration_label_pos = timeline_bbox.GetTR() + ImVec2( -2.f -duration_label_size.x, fontsize);
    if (verticalflip)
        duration_label_pos.y -= fontsize;
    ImGui::RenderTextClipped( duration_label_pos, duration_label_pos + duration_label_size,
                              text_buf, NULL, &duration_label_size);
    window->DrawList->AddLine( timeline_bbox.GetTR(), timeline_bbox.GetBR(), text_color, 1.5f);

    // render tick and text START
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s",
                   GstToolkit::time_to_string(begin, GstToolkit::TIME_STRING_MINIMAL).c_str());
    ImVec2 beginning_label_size = ImGui::CalcTextSize(text_buf, NULL);
    ImVec2 beginning_label_pos = timeline_bbox.GetTL() + ImVec2(3.f, fontsize);
    if (verticalflip)
        beginning_label_pos.y -= fontsize;
    if ( beginning_label_pos.x + beginning_label_size.x < duration_label_pos . x) {
        ImGui::RenderTextClipped( beginning_label_pos, beginning_label_pos + beginning_label_size,
                                  text_buf, NULL, &beginning_label_size);
    }
    window->DrawList->AddLine( timeline_bbox.GetTL(), timeline_bbox.GetBL(), text_color, 1.5f);

    ImGui::PopFont();

    // render the tick marks along TIMELINE
    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text] -ImVec4(0.f,0.f,0.f,0.4f));
    ImVec2 pos = verticalflip ? timeline_bbox.GetBL() : timeline_bbox.GetTL();

    // loop ticks from begin to end
    guint64 tick = tick_step > 0 ? (begin / tick_step) * tick_step : 0;
    while ( tick < end )
    {
        // large tick mark ?
        float tick_length = (tick%large_tick_step) ? style.FramePadding.y : fontsize - style.FramePadding.y;

        // label tick mark
        if ( (tick%label_tick_step) < 1 )
        {
            // larger tick mark for label
            tick_length = fontsize;

            // correct tick value for delta for approximation to rounded second marks
            guint64 ticklabel = tick + ( tick_delta * tick / label_tick_step);
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s",
                           GstToolkit::time_to_string(ticklabel, GstToolkit::TIME_STRING_MINIMAL).c_str());
            ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
            ImVec2 mini = ImVec2( pos.x - label_size.x / 2.f, pos.y);
            ImVec2 maxi = ImVec2( pos.x + label_size.x / 2.f, pos.y);

            if (verticalflip)   {
                mini.y -= tick_length + label_size.y;
                maxi.y -= tick_length;
            }
            else {
                mini.y += tick_length;
                maxi.y += tick_length + label_size.y;
            }

            // do not overlap with labels for beginning and duration
            if (mini.x - style.ItemSpacing.x > (beginning_label_pos.x + beginning_label_size.x) &&  maxi.x + style.ItemSpacing.x < duration_label_pos.x)
                ImGui::RenderTextClipped(mini, maxi, text_buf, NULL, &label_size);
        }

        // draw the tick mark each step
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, verticalflip ? -tick_length : tick_length), text_color);

        // next tick
        tick += tick_step;
        float tick_percent = static_cast<float> ( static_cast<double>(tick-begin) / static_cast<double>(duration) );
        if (verticalflip)
            pos = ImLerp(timeline_bbox.GetBL(), timeline_bbox.GetBR(), tick_percent);
        else
            pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);

    }

    ImGui::PopStyleColor(1);
}


void ImGuiToolkit::RenderTimelineBPM (ImGuiWindow* window, ImRect timeline_bbox, double tempo, double quantum,
                                      guint64 begin, guint64 end, guint64 step, bool verticalflip)
{
    if (tempo<1.)
        return;


    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float fontsize = g.FontSize;
    const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);

    // keep duration
    const guint64 duration = end - begin;

    // by default, put a tick mark at every frame step
    guint64 tick_step = step;

    // how many pixels to represent one frame step?
    const float step_ = static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );

    // spacing between small tick marks for frames
    for (float tick_step_pixels = timeline_bbox.GetWidth() * step_; tick_step_pixels < 5.f; tick_step *= 2)
    {
        tick_step_pixels = timeline_bbox.GetWidth() * static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
    }

    // by default, put large mark every beat and label every phase
    guint64 time_beat = GST_SECOND * (60.0 / tempo) ;
    guint64 large_tick_step = time_beat;

    // how many pixels to represent one beat?
    const float beat_ = static_cast<float> ( static_cast<double>(large_tick_step) / static_cast<double>(duration) );

    // spacing between small tick marks for frames
    for (float tick_beat_pixels = timeline_bbox.GetWidth() * beat_; tick_beat_pixels < 10.f; large_tick_step *= 2)
    {
        tick_beat_pixels = timeline_bbox.GetWidth() * static_cast<float> ( static_cast<double>(large_tick_step) / static_cast<double>(duration) );
    }

    guint64 label_tick_step = large_tick_step * (guint64) ceil(quantum);

    // render tics and text
    char text_buf[24];

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);

    // render tick and text END
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s",
                   GstToolkit::time_to_string(end, GstToolkit::TIME_STRING_MINIMAL).c_str());
    ImVec2 duration_label_size = ImGui::CalcTextSize(text_buf, NULL);
    ImVec2 duration_label_pos = timeline_bbox.GetTR() + ImVec2( -2.f -duration_label_size.x, fontsize);
    if (verticalflip)
        duration_label_pos.y -= fontsize;
    ImGui::RenderTextClipped( duration_label_pos, duration_label_pos + duration_label_size,
                              text_buf, NULL, &duration_label_size);
    window->DrawList->AddLine( timeline_bbox.GetTR(), timeline_bbox.GetBR(), text_color, 1.5f);

    // render tick and text START
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s",
                   GstToolkit::time_to_string(begin, GstToolkit::TIME_STRING_MINIMAL).c_str());
    ImVec2 beginning_label_size = ImGui::CalcTextSize(text_buf, NULL);
    ImVec2 beginning_label_pos = timeline_bbox.GetTL() + ImVec2(3.f, fontsize);
    if (verticalflip)
        beginning_label_pos.y -= fontsize;
    if ( beginning_label_pos.x + beginning_label_size.x < duration_label_pos . x) {
        ImGui::RenderTextClipped( beginning_label_pos, beginning_label_pos + beginning_label_size,
                                  text_buf, NULL, &beginning_label_size);
    }
    window->DrawList->AddLine( timeline_bbox.GetTL(), timeline_bbox.GetBL(), text_color, 1.5f);

    ImGui::PopFont();

    // render the tick marks along TIMELINE
    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text] -ImVec4(0.f,0.f,0.f,0.4f));
    ImVec2 pos = verticalflip ? timeline_bbox.GetBL() : timeline_bbox.GetTL();

    // SMALL ticks every step
    float tick_length = style.FramePadding.y;
    guint64 tick = 0;
    while ( tick < end )
    {
        // draw the tick mark each step
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, verticalflip ? -tick_length : tick_length), text_color);

        // next tick
        tick += tick_step;
        float tick_percent = static_cast<float> ( static_cast<double>(tick-begin) / static_cast<double>(duration) );
        if (verticalflip)
            pos = ImLerp(timeline_bbox.GetBL(), timeline_bbox.GetBR(), tick_percent);
        else
            pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);

    }

    // LARGE tick marks every tempo
    pos = verticalflip ? timeline_bbox.GetBL() : timeline_bbox.GetTL();
    tick_length =  fontsize - style.FramePadding.y;
    tick = 0;
    while ( tick < end )
    {
        // label tick mark
        if ( (tick%label_tick_step) < 1 )
        {
            // beat count label
            guint64 ticklabel = tick / time_beat;
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%ld", ticklabel);
            ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
            ImVec2 mini = ImVec2( pos.x - label_size.x / 2.f, pos.y);
            ImVec2 maxi = ImVec2( pos.x + label_size.x / 2.f, pos.y);

            if (verticalflip)   {
                mini.y -= tick_length + label_size.y;
                maxi.y -= tick_length;
            }
            else {
                mini.y += tick_length;
                maxi.y += tick_length + label_size.y;
            }

            // do not overlap with labels for beginning and duration
            if (mini.x - style.ItemSpacing.x > (beginning_label_pos.x + beginning_label_size.x) &&  maxi.x + style.ItemSpacing.x < duration_label_pos.x)
                ImGui::RenderTextClipped(mini, maxi, text_buf, NULL, &label_size);
        }

        // draw the tick mark each step
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, verticalflip ? -tick_length : tick_length), text_color);

        // next tick
        tick += large_tick_step;
        float tick_percent = static_cast<float> ( static_cast<double>(tick-begin) / static_cast<double>(duration) );
        if (verticalflip)
            pos = ImLerp(timeline_bbox.GetBL(), timeline_bbox.GetBR(), tick_percent);
        else
            pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);

    }

    ImGui::PopStyleColor(1);
}


bool ImGuiToolkit::TimelineSlider (const char* label, guint64 *time, guint64 begin, guint64 first,
                                   guint64 end, guint64 step, const float width, double tempo, double quantum)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // get style & id
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float fontsize = g.FontSize;
    const ImGuiID id = window->GetID(label);

    //
    // FIRST PREPARE ALL data structures
    //

    // widget bounding box
    const float height = 2.f * (fontsize + style.FramePadding.y);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, height);
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // cursor size
    const float cursor_width = 0.5f * fontsize;

    // TIMELINE is inside the bbox, in a slightly smaller bounding box
    ImRect timeline_bbox(bbox);
    timeline_bbox.Expand( ImVec2() - style.FramePadding );

    // SLIDER is inside the timeline
    ImRect slider_bbox( timeline_bbox.GetTL() + ImVec2(-cursor_width + 2.f, cursor_width + 4.f ), timeline_bbox.GetBR() + ImVec2( cursor_width - 2.f, 0.f ) );

    // units conversion: from time to float (calculation made with higher precision first)
    float time_ = static_cast<float> ( static_cast<double>(*time - begin) / static_cast<double>(end - begin) );

    //
    // SECOND GET USER INPUT AND PERFORM CHANGES AND DECISIONS
    //

    // read user input from system
    bool left_mouse_press = false;
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = ImGui::FocusableItemRegister(window, id);
        left_mouse_press = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (focus_requested || left_mouse_press || g.NavActivateId == id || g.NavInputId == id)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }

    // time Slider behavior
    ImRect grab_slider_bb;
    ImU32 grab_slider_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    float time_slider = time_ * 10.f; // x 10 precision on grab
    float time_zero = 0.f;
    float time_end = 10.f;
    bool value_changed = ImGui::SliderBehavior(slider_bbox, id, ImGuiDataType_Float, &time_slider, &time_zero,
                                               &time_end, "%.2f", 1.f, ImGuiSliderFlags_None, &grab_slider_bb);
    if (value_changed){
        *time = static_cast<guint64> ( 0.1 * static_cast<double>(time_slider) * static_cast<double>(end - begin) );
        if (first != GST_CLOCK_TIME_NONE)
            *time -= first;
        grab_slider_color = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    }

    //
    // THIRD RENDER
    //

    // Render the bounding box
    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(bbox.Min, bbox.Max, frame_col, true, style.FrameRounding);

    // render the timeline
    if (tempo > 0 && quantum > 0)
        RenderTimelineBPM(window, timeline_bbox, tempo, quantum, begin, end, step);
    else
        RenderTimeline(window, timeline_bbox, begin, end, step);

    // draw slider grab handle
    if (grab_slider_bb.Max.x > grab_slider_bb.Min.x) {
        window->DrawList->AddRectFilled(grab_slider_bb.Min, grab_slider_bb.Max, grab_slider_color, style.GrabRounding);
    }

    // draw the cursor
    pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), time_) - ImVec2(cursor_width, 2.f);
    ImGui::RenderArrow(window->DrawList, pos, ImGui::GetColorU32(ImGuiCol_SliderGrab), ImGuiDir_Up);

    return left_mouse_press;
}



bool ImGuiToolkit::InvisibleSliderInt (const char* label, uint *index, uint min, uint max, ImVec2 size)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // get id
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // read user input from system
    const bool left_mouse_press = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = ImGui::FocusableItemRegister(window, id);
        if (focus_requested || (hovered && left_mouse_press) )
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }
    else
        return false;

    bool value_changed = false;

    if (ImGui::GetActiveID() == id) {
        // Slider behavior
        ImRect grab_slider_bb;
        uint _zero = min;
        uint _end = max;
        value_changed = ImGui::SliderBehavior(bbox, id, ImGuiDataType_U32, index, &_zero,
                                              &_end, "%ld", 1.f, ImGuiSliderFlags_None, &grab_slider_bb);
    }

    return value_changed;
}

bool ImGuiToolkit::EditPlotLines (const char* label, float *array, int values_count, float values_min, float values_max, const ImVec2 size)
{
    bool array_changed = false;

    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // capture coordinates before any draw or action
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_pos.x, ImGui::GetIO().MousePos.y - canvas_pos.y);

    // get id
    const ImGuiID id = window->GetID(label);

    // add item
    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // read user input and activate widget
    const bool left_mouse_press = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = ImGui::FocusableItemRegister(window, id);
        if (focus_requested || (hovered && left_mouse_press) )
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }
    else
        return false;

    ImVec4* colors = ImGui::GetStyle().Colors;
    ImVec4 bg_color = hovered ? colors[ImGuiCol_FrameBgHovered] : colors[ImGuiCol_FrameBg];

    // enter edit if widget is active
    if (ImGui::GetActiveID() == id) {

        static uint previous_index = UINT32_MAX;
        bg_color = colors[ImGuiCol_FrameBgActive];

        // keep active area while mouse is pressed
        if (left_mouse_press)
        {
            float x = (float) values_count * mouse_pos_in_canvas.x / bbox.GetWidth();
            uint index = CLAMP( (int) floor(x), 0, values_count-1);

            float y = mouse_pos_in_canvas.y / bbox.GetHeight();
            y = CLAMP( (y * (values_max-values_min)) + values_min, values_min, values_max);


            if (previous_index == UINT32_MAX)
                previous_index = index;

            array[index] = values_max - y;
            for (int i = MIN(previous_index, index); i < MAX(previous_index, index); ++i)
                array[i] = values_max - y;

            previous_index = index;

            array_changed = true;
        }
        // release active widget on mouse release
        else {
            ImGui::ClearActiveID();
            previous_index = UINT32_MAX;
        }

    }

    // back to draw
    ImGui::SetCursorScreenPos(canvas_pos);

    // plot lines
    char buf[128];
    sprintf(buf, "##Lines%s", label);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::PlotLines(buf, array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(1);

    return array_changed;
}


bool ImGuiToolkit::EditPlotHistoLines (const char* label, float *histogram_array, float *lines_array,
                                      int values_count, float values_min, float values_max, guint64 begin, guint64 end,
                                      bool edit_histogram, bool *released, const ImVec2 size)
{
    bool array_changed = false;

    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // capture coordinates before any draw or action
    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_pos.x, ImGui::GetIO().MousePos.y - canvas_pos.y);

    // get id
    const ImGuiID id = window->GetID(label);

    // add item
    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    *released = false;

    // read user input and activate widget
    const bool mouse_press = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = ImGui::FocusableItemRegister(window, id);
        if (focus_requested || (hovered && mouse_press) )
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }
    else
        return false;

    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float _h_space = style.WindowPadding.x;
    ImVec4 bg_color = hovered ? style.Colors[ImGuiCol_FrameBgHovered] : style.Colors[ImGuiCol_FrameBg];

    // prepare index
    double x = (mouse_pos_in_canvas.x - _h_space) / (size.x - 2.f * _h_space);
    size_t index = CLAMP( (int) floor(static_cast<double>(values_count) * x), 0, values_count);
    char cursor_text[64];
    guint64 time = begin + (index * end) / static_cast<guint64>(values_count);
    ImFormatString(cursor_text, IM_ARRAYSIZE(cursor_text), "%s",
                   GstToolkit::time_to_string(time, GstToolkit::TIME_STRING_MINIMAL).c_str());

    // enter edit if widget is active
    if (ImGui::GetActiveID() == id) {

        bg_color = style.Colors[ImGuiCol_FrameBgActive];

        // keep active area while mouse is pressed
        static bool active = false;
        static size_t previous_index = UINT32_MAX;
        if (mouse_press)
        {
            float val = mouse_pos_in_canvas.y / bbox.GetHeight();
            val = CLAMP( (val * (values_max-values_min)) + values_min, values_min, values_max);

            if (previous_index == UINT32_MAX)
                previous_index = index;

            const size_t left = MIN(previous_index, index);
            const size_t right = MAX(previous_index, index);

            if (edit_histogram){
                static float target_value = values_min;

                // toggle value histo
                if (!active) {
                    target_value = histogram_array[index] > 0.f ? 0.f : 1.f;
                    active = true;
                }

                for (size_t i = left; i < right; ++i)
                    histogram_array[i] = target_value;
            }
            else  {
                const float target_value =  values_max - val;

                for (size_t i = left; i < right; ++i)
                    lines_array[i] = target_value;

            }

            previous_index = index;
            array_changed = true;
        }
        // release active widget on mouse release
        else {
            active = false;
            ImGui::ClearActiveID();
            previous_index = UINT32_MAX;
            *released = true;
        }

    }

    // back to draw
    ImGui::SetCursorScreenPos(canvas_pos);

    // plot histogram (with frame)
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, style.Colors[ImGuiCol_TitleBg]); // a dark color
    char buf[128];
    sprintf(buf, "##Histo%s", label);
    ImGui::PlotHistogram(buf, histogram_array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(2);

    ImGui::SetCursorScreenPos(canvas_pos);

    // plot (transparent) lines
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    sprintf(buf, "##Lines%s", label);
    ImGui::PlotLines(buf, lines_array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(1);

    // draw the cursor
    if (hovered) {
        // prepare color and text
        const ImU32 cur_color = ImGui::GetColorU32(ImGuiCol_CheckMark);
        ImGui::PushStyleColor(ImGuiCol_Text, cur_color);
        ImVec2 label_size = ImGui::CalcTextSize(cursor_text, NULL);

        // render cursor depending on action
        mouse_pos_in_canvas.x = CLAMP(mouse_pos_in_canvas.x, _h_space, size.x - _h_space);
        ImVec2 cursor_pos = canvas_pos;
        if (edit_histogram) {
            cursor_pos = cursor_pos + ImVec2(mouse_pos_in_canvas.x, 4.f);
            window->DrawList->AddLine( cursor_pos, cursor_pos + ImVec2(0.f, size.y - 8.f), cur_color);
        }
        else {
            cursor_pos = cursor_pos + mouse_pos_in_canvas;
            window->DrawList->AddCircleFilled( cursor_pos, 3.f, cur_color, 8);
        }

        // draw text
        cursor_pos.y = canvas_pos.y + size.y - label_size.y - 1.f;
        if (mouse_pos_in_canvas.x + label_size.x < size.x - 2.f * _h_space)
            cursor_pos.x += _h_space;
        else
            cursor_pos.x -= label_size.x + _h_space;
        ImGui::RenderTextClipped(cursor_pos, cursor_pos + label_size, cursor_text, NULL, &label_size);

        ImGui::PopStyleColor(1);
    }

    return array_changed;
}

void ImGuiToolkit::ShowPlotHistoLines (const char* label, float *histogram_array, float *lines_array, int values_count, float values_min, float values_max, const ImVec2 size)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    // capture coordinates before any draw or action
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

    // get id
    const ImGuiID id = window->GetID(label);

    // add item
    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return;

    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImVec4 bg_color = style.Colors[ImGuiCol_FrameBg];

    // back to draw
    ImGui::SetCursorScreenPos(canvas_pos);

    // plot transparent histogram
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0, 0, 0, 250));
    char buf[128];
    sprintf(buf, "##Histo%s", label);
    ImGui::PlotHistogram(buf, histogram_array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(2);

    ImGui::SetCursorScreenPos(canvas_pos);

    // plot lines
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    sprintf(buf, "##Lines%s", label);
    ImGui::PlotLines(buf, lines_array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(1);

}


void ImGuiToolkit::SetFont (ImGuiToolkit::font_style style, const std::string &ttf_font_name, int pointsize, int oversample)
{
    // Font Atlas ImGui Management
    ImGuiIO& io = ImGui::GetIO();    
    GLint max = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
    io.Fonts->TexDesiredWidth = max / 2;  // optimize use of texture depending on OpenGL drivers

    // Setup font config
    const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesDefault();
    std::string filename = "fonts/" + ttf_font_name +  ".ttf";
    std::string fontname = ttf_font_name + ", " + std::to_string(pointsize) + "px";
    ImFontConfig font_config;
    fontname.copy(font_config.Name, 40);    
    font_config.FontDataOwnedByAtlas = false; // data will be copied in font atlas
    if ( max * max < 16777216 )  // hack: try to avoid font textures too larges by disabling oversamplig
        oversample = 1;
    font_config.OversampleH = CLAMP( oversample, 1, 5 );
    font_config.OversampleV = CLAMP( oversample, 1, 5 );

    // read font in Resource manager
    size_t data_size = 0;
    const char* data_src = Resource::getData(filename, &data_size);

    // temporary copy for instanciation
    void *data = malloc( sizeof (char) * data_size);
    memcpy(data, data_src, data_size);
    
    // create and add font
    fontmap[style] = io.Fonts->AddFontFromMemoryTTF(data, (int)data_size, static_cast<float>(pointsize), &font_config, glyph_ranges);
    free(data);

    // merge in icons from Font Awesome
    {
        // range for only FontAwesome characters
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        font_config.MergeMode = true; // Merging glyphs into current font
        font_config.PixelSnapH = true;
        fontname = "icons" + fontname;
        fontname.copy(font_config.Name, 40);  
        // load FontAwesome only once
        static size_t icons_data_size = 0;
        static const char* icons_data_src = Resource::getData( "fonts/" FONT_ICON_FILE_NAME_FAS, &icons_data_size);
        // temporary copy for instanciation
        void *icons_data = malloc( sizeof (char) * icons_data_size);
        memcpy(icons_data, icons_data_src, icons_data_size);
        // merge font
        io.Fonts->AddFontFromMemoryTTF(icons_data, (int)icons_data_size, static_cast<float>(pointsize-2), &font_config, icons_ranges);
        free(icons_data);
    }
    
}

void ImGuiToolkit::PushFont (ImGuiToolkit::font_style style)
{
    if (fontmap.count(style) > 0)
        ImGui::PushFont( fontmap[style] );
    else
        ImGui::PushFont( NULL );    
}


void ImGuiToolkit::ImageGlyph(font_style type, char c, float h)
{
    ImGuiIO& io = ImGui::GetIO();
    const ImTextureID my_tex_id = io.Fonts->TexID;
    const ImFontGlyph* glyph =  fontmap[type]->FindGlyph(c);
    const ImVec2 size( h * (glyph->X1 - glyph->X0) /  (glyph->Y1 - glyph->Y0)  , h);
    const ImVec2 uv0( glyph->U0, glyph->V0);
    const ImVec2 uv1( glyph->U1, glyph->V1);
    ImGui::Image((void*)(intptr_t)my_tex_id, size, uv0, uv1);
}


void ImGuiToolkit::Spacing()
{
    ImGui::Dummy(ImVec2(0, 0.5 * ImGui::GetTextLineHeight()));
}

void ImGuiToolkit::WindowText(const char* window_name, ImVec2 window_pos, const char* text)
{
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    if (ImGui::Begin(window_name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text(text);
        ImGui::PopFont();

        ImGui::End();
    }
}

bool ImGuiToolkit::WindowButton(const char* window_name, ImVec2 window_pos, const char* button_text)
{
    bool ret = false;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    if (ImGui::Begin(window_name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ret = ImGui::Button(button_text);
        ImGui::PopFont();

        ImGui::End();
    }
    return ret;
}

void ImGuiToolkit::WindowDragFloat(const char* window_name, ImVec2 window_pos, float* v, float v_speed, float v_min, float v_max, const char* format)
{
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    if (ImGui::Begin(window_name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetNextItemWidth(100.f);
        ImGui::DragFloat("##nolabel", v, v_speed, v_min, v_max, format);
        ImGui::PopFont();

        ImGui::End();
    }
}

ImVec4 ImGuiToolkit::HighlightColor(bool active)
{
    if (active)
        return ImGui::GetStyle().Colors[ImGuiCol_CheckMark];
    else
        return ImGui::GetStyle().Colors[ImGuiCol_TabUnfocusedActive];
}

void ImGuiToolkit::SetAccentColor(accent_color color)
{
    // hack : preload texture icon to prevent slow-down of rendering when creating a new icon for the first time
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");


    ImVec4* colors = ImGui::GetStyle().Colors;

    if (color == ImGuiToolkit::ACCENT_ORANGE) {

        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.13f, 0.13f, 0.14f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.97f);
        colors[ImGuiCol_Border]                 = ImVec4(0.69f, 0.69f, 0.69f, 0.25f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.39f, 0.39f, 0.39f, 0.55f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.29f, 0.29f, 0.29f, 0.60f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.80f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.14f, 0.14f, 0.14f, 0.94f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.36f, 0.36f, 0.36f, 0.62f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 0.63f, 0.31f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.88f, 0.52f, 0.24f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.47f, 0.47f, 0.47f, 0.72f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.24f, 0.24f, 0.24f, 0.90f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.24f, 0.24f, 0.24f, 0.67f);
        colors[ImGuiCol_Header]                 = ImVec4(0.98f, 0.59f, 0.26f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.98f, 0.59f, 0.26f, 0.51f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.50f, 0.43f, 0.43f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.75f, 0.40f, 0.10f, 0.67f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.90f, 0.73f, 0.59f, 0.95f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.52f, 0.49f, 0.49f, 0.50f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.90f, 0.73f, 0.59f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.90f, 0.73f, 0.59f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.58f, 0.35f, 0.18f, 0.82f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.80f, 0.49f, 0.25f, 0.82f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.80f, 0.49f, 0.25f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.10f, 0.07f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.42f, 0.26f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.59f, 0.73f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.59f, 0.73f, 0.90f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.98f, 0.59f, 0.26f, 0.64f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.13f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
        colors[ImGuiCol_DragDropTarget]         = colors[ImGuiCol_HeaderActive];
    }
    else if (color == ImGuiToolkit::ACCENT_GREY) {
        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.13f, 0.13f, 0.14f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.97f);
        colors[ImGuiCol_Border]                 = ImVec4(0.69f, 0.69f, 0.69f, 0.25f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.39f, 0.39f, 0.39f, 0.55f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.29f, 0.29f, 0.29f, 0.60f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.80f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.14f, 0.14f, 0.14f, 0.94f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.36f, 0.36f, 0.36f, 0.62f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.63f, 0.63f, 0.63f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.47f, 0.47f, 0.47f, 0.72f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.24f, 0.24f, 0.24f, 0.90f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.24f, 0.24f, 0.24f, 0.67f);
        colors[ImGuiCol_Header]                 = ImVec4(0.59f, 0.59f, 0.59f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.59f, 0.59f, 0.59f, 0.51f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.75f, 0.75f, 0.75f, 0.67f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.75f, 0.75f, 0.75f, 0.95f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.49f, 0.49f, 0.49f, 0.50f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.90f, 0.90f, 0.90f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.90f, 0.90f, 0.90f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.47f, 0.47f, 0.47f, 0.82f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.39f, 0.39f, 0.39f, 0.82f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.10f, 0.10f, 0.10f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.64f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.13f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
        colors[ImGuiCol_DragDropTarget]         = colors[ImGuiCol_HeaderActive];
    }
    else {
        // default BLUE
        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.13f, 0.13f, 0.14f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.97f);
        colors[ImGuiCol_Border]                 = ImVec4(0.69f, 0.69f, 0.69f, 0.25f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.39f, 0.39f, 0.39f, 0.55f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.29f, 0.29f, 0.29f, 0.60f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.80f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.14f, 0.14f, 0.14f, 0.94f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.36f, 0.36f, 0.36f, 0.62f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.31f, 0.63f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.47f, 0.47f, 0.47f, 0.72f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.24f, 0.24f, 0.24f, 0.90f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.24f, 0.24f, 0.24f, 0.67f);
        colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.51f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 0.71f);
        colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.67f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.59f, 0.73f, 0.90f, 0.95f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.49f, 0.49f, 0.52f, 0.50f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.59f, 0.73f, 0.90f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.59f, 0.73f, 0.90f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.35f, 0.58f, 0.82f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.25f, 0.49f, 0.80f, 0.82f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.25f, 0.49f, 0.80f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.94f, 0.57f, 0.01f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.82f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.94f, 0.57f, 0.01f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.82f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.64f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.13f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
        colors[ImGuiCol_DragDropTarget]         = colors[ImGuiCol_HeaderActive];
    }

}

void word_wrap(std::string *str, unsigned per_line)
{
    unsigned line_begin = 0;
    while (line_begin < str->size())
    {
        const unsigned ideal_end = line_begin + per_line ;
        unsigned line_end = ideal_end < str->size() ? ideal_end : str->size()-1;

        if (line_end == str->size() - 1)
            ++line_end;
        else if (std::isspace(str->at(line_end)))
        {
            str->replace(line_end, 1, 1, '\n' );
            ++line_end;
        }
        else    // backtrack
        {
            unsigned end = line_end;
            while ( end > line_begin && !std::isspace(str->at(end)))
                --end;

            if (end != line_begin)
            {
                line_end = end;
                str->replace(line_end++, 1,  1, '\n' );
            }
            else {
                str->insert(line_end++, 1, '\n' );
            }
        }

        line_begin = line_end;
    }
}


struct InputTextCallback_UserData
{
    std::string*      Str;
    int               WordWrap;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCallback_UserData* user_data = static_cast<InputTextCallback_UserData*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
//        if (user_data->WordWrap > 1)
//            word_wrap(user_data->Str, user_data->WordWrap );

        // Resize string callback
        std::string* str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }

    return 0;
}

bool ImGuiToolkit::InputText(const char* label, std::string* str)
{
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CharsNoBlank;
    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;

    return ImGui::InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

bool ImGuiToolkit::InputTextMultiline(const char* label, std::string* str, const ImVec2& size, int linesize)
{
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize;
    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.WordWrap = linesize;

    return ImGui::InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
}


void ImGuiToolkit::ShowTextMultiline(const char* label, const std::string &str, float width)
{
    size_t numlines = std::count(str.begin(), str.end(), '\n') + 1;

    ImGuiContext& g = *GImGui;
    ImVec2 size(width, numlines * (g.FontSize + g.Style.ItemSpacing.y) + g.Style.FramePadding.y * 2.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, g.Style.Colors[ImGuiCol_FrameBgHovered]);
    ImGui::InputTextMultiline(label, (char*)str.c_str(), str.capacity() + 1, size, ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();

}
