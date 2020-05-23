
#include <map>
#include <algorithm>

#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif

#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#define MILISECOND 1000000L
#define SECOND 1000000000L
#define MINUTE 60000000000L

#include "Resource.h"
#include "FileDialog.h"
#include "ImGuiToolkit.h"
#include "GstToolkit.h"
#include "SystemToolkit.h"

unsigned int textureicons = 0;
std::map <ImGuiToolkit::font_style, ImFont*>fontmap;


void ImGuiToolkit::ButtonOpenUrl( const char* url, const ImVec2& size_arg )
{
    char label[512];
    sprintf( label, "%s  %s", ICON_FA_EXTERNAL_LINK_ALT, url );

    if ( ImGui::Button(label, size_arg) )
        SystemToolkit::open(url);
}


void ImGuiToolkit::ButtonToggle( const char* label, bool* toggle )
{
    ImVec4* colors = ImGui::GetStyle().Colors;
    const auto active = *toggle;
    if( active ) {
        ImGui::PushStyleColor( ImGuiCol_Button, colors[ImGuiCol_TabActive] );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colors[ImGuiCol_TabHovered] );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, colors[ImGuiCol_Tab] );
    }
    if( ImGui::Button( label ) ) *toggle = !*toggle;
    if( active ) ImGui::PopStyleColor( 3 );
}


void ImGuiToolkit::ButtonSwitch(const char* label, bool* toggle, const char* help)
{
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
    if (ImGui::IsItemClicked())
        *toggle = !*toggle;
    float t = *toggle ? 1.0f : 0.0f;

    // animation
    ImGuiContext& g = *GImGui;
    float ANIM_SPEED = 0.1f;
    if (g.LastActiveId == g.CurrentWindow->GetID(label))// && g.LastActiveIdTimer < ANIM_SPEED)
    {
        float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
        t = *toggle ? (t_anim) : (1.0f - t_anim);
    }

    // hover
    ImU32 col_bg;
    if (ImGui::IsItemHovered())
        col_bg = ImGui::GetColorU32(ImLerp(colors[ImGuiCol_FrameBgHovered], colors[ImGuiCol_TabHovered], t));
    else
        col_bg = ImGui::GetColorU32(ImLerp(colors[ImGuiCol_FrameBg], colors[ImGuiCol_TabActive], t));

    // draw help text if present
    if (help) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 1.f));
        ImGui::RenderText(draw_pos, help);
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

}


void ImGuiToolkit::Icon(int i, int j)
{
    // icons.dds is a 20 x 20 grid of icons 
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");

    ImVec2 uv0( static_cast<float>(i) * 0.05, static_cast<float>(j) * 0.05 );
    ImVec2 uv1( uv0.x + 0.05, uv0.y + 0.05 );
    ImGui::Image((void*)(intptr_t)textureicons, ImVec2(ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeightWithSpacing()), uv0, uv1);
}

bool ImGuiToolkit::ButtonIcon(int i, int j)
{
    // icons.dds is a 20 x 20 grid of icons 
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");

    ImVec2 uv0( static_cast<float>(i) * 0.05, static_cast<float>(j) * 0.05 );
    ImVec2 uv1( uv0.x + 0.05, uv0.y + 0.05 );

    ImGui::PushID( i*20 + j);
    bool ret =  ImGui::ImageButton((void*)(intptr_t)textureicons, ImVec2(ImGui::GetTextLineHeightWithSpacing(),ImGui::GetTextLineHeightWithSpacing()), uv0, uv1, 3);
    ImGui::PopID();

    return ret;
}

bool ImGuiToolkit::ButtonIconToggle(int i, int j, int i_toggle, int j_toggle, bool* toggle)
{
    bool ret = false;
    ImGui::PushID( i * 20 + j + i_toggle * 20 + j_toggle);

    if (*toggle) {
        if ( ButtonIcon(i_toggle, j_toggle)) {
            *toggle = false;
            ret = true;
        }
    }
    else {
        if ( ButtonIcon(i, j)) {
            *toggle = true;
            ret = true;
        }
    }

    ImGui::PopID();
    return ret;
}

struct Sum
{
    void operator()(std::pair<int, int> n) { sum += n.first * 20 + n.second; }
    int sum{0};
};

bool ImGuiToolkit::ButtonIconMultistate(std::vector<std::pair<int, int> > icons, int* state)
{
    bool ret = false;
    Sum id = std::for_each(icons.begin(), icons.end(), Sum());
    ImGui::PushID( id.sum );

    int s = CLAMP(*state, 0, icons.size() - 1);
    if ( ButtonIcon( icons[s].first, icons[s].second ) ){
        ++s;
        if (s > icons.size() -1)
            *state = 0;
        else
            *state = s;
        ret = true;
    }

    ImGui::PopID();
    return ret;
}


void ImGuiToolkit::ShowIconsWindow(bool* p_open)
{
    if (textureicons == 0)
        textureicons = Resource::getTextureDDS("images/icons.dds");

    ImGuiIO& io = ImGui::GetIO();
    float my_tex_w = 640.0;
    float my_tex_h = 640.0;
    
    ImGui::Begin("Icons", p_open);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Image((void*)(intptr_t)textureicons, ImVec2(640, 640));
    if (ImGui::IsItemHovered())
    {
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

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.txt)
void ImGuiToolkit::HelpMarker(const char* desc)
{
    ImGui::TextDisabled( ICON_FA_QUESTION_CIRCLE );
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Draws a timeline showing
// 1) a cursor at position *time in the range [0 duration]
// 2) a line of tick marks indicating time, every step if possible
// 3) a slider handle below the cursor: user can move the slider
// Behavior
// a) Returns TRUE if the left mouse button LMB is pressed over the timeline
// b) the value of *time is changed to the position of the slider handle from user input (LMB)

bool ImGuiToolkit::TimelineSlider(const char* label, guint64 *time, guint64 duration, guint64 step)
{
    static guint64 optimal_tick_marks[12] = { 100 * MILISECOND, 500 * MILISECOND, 1 * SECOND, 2 * SECOND, 5 * SECOND, 10 * SECOND, 20 * SECOND, 1 * MINUTE, 2 * MINUTE, 5 * MINUTE, 10 * MINUTE, 60 * MINUTE };

    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // get style & id
    const ImGuiStyle& style = ImGui::GetStyle();
    const float fontsize = ImGui::GetFontSize();
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    //
    // FIRST PREPARE ALL data structures
    //

    // widget bounding box
    const float height = 2.f * (fontsize + style.FramePadding.y);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), ImGui::CalcItemWidth(), height);
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // cursor size
    const float cursor_scale = 1.f;
    const float cursor_width = 0.5f * fontsize * cursor_scale;

    // TIMELINE is inside the bbox, in a slightly smaller bounding box
    ImRect timeline_bbox(bbox);
    timeline_bbox.Expand( ImVec2(-cursor_width, -style.FramePadding.y) );

    // SLIDER is inside the timeline
    ImRect slider_bbox( timeline_bbox.GetTL() + ImVec2(-cursor_width + 2.f, cursor_width + 4.f ), timeline_bbox.GetBR() + ImVec2( cursor_width - 2.f, 0.f ) );

    // units conversion: from time to float (calculation made with higher precision first)
    float time_ = static_cast<float> ( static_cast<double>(*time) / static_cast<double>(duration) );
    float step_ = static_cast<float> ( static_cast<double>(step) / static_cast<double>(duration) );

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
        //        g_print("slider %f  %ld \n", time_slider, static_cast<guint64> ( static_cast<double>(time_slider) * static_cast<double>(duration) ));
        *time = static_cast<guint64> ( 0.1 * static_cast<double>(time_slider) * static_cast<double>(duration) );
        grab_slider_color = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    }

    //
    // THIRD RENDER
    //

    // Render the bounding box
    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(bbox.Min, bbox.Max, frame_col, true, style.FrameRounding);

    // by default, put a tick mark at every frame step and a large mark every second
    guint64 tick_step = step;
    guint64 large_tick_step = SECOND;

    // how many pixels to represent one frame step?
    float tick_step_pixels = timeline_bbox.GetWidth() * step_;     
    // while there is less than 3 pixels between two tick marks (or at last optimal tick mark)
    for ( int i=0; i<10 && tick_step_pixels < 5.f; ++i )
    {
        // try to use the optimal tick marks pre-defined
        tick_step = optimal_tick_marks[i];
        large_tick_step = optimal_tick_marks[i+1];
        tick_step_pixels = timeline_bbox.GetWidth() * static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
    }

    // render the tick marks along TIMELINE
    ImU32 color = ImGui::GetColorU32( style.Colors[ImGuiCol_Text] );
    pos = timeline_bbox.GetTL();
    guint64 tick = 0;
    float tick_percent = 0.f;
    while ( tick < duration)
    {
        // large tick mark every large tick
        float tick_length = !(tick%large_tick_step) ? fontsize : style.FramePadding.y;

        // draw a tick mark each step 
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, tick_length), color);

        // next tick
        tick += tick_step;
        tick_percent = static_cast<float> ( static_cast<double>(tick) / static_cast<double>(duration) );
        pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);
    }

    // tick EOF
    window->DrawList->AddLine( timeline_bbox.GetTR(), timeline_bbox.GetTR() + ImVec2(0.f, fontsize), color);

    // render text : duration and current time
    char overlay_buf[24];
    ImVec2 overlay_size = ImVec2(0.f, 0.f);
    ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%s", GstToolkit::time_to_string(duration).c_str());
    overlay_size = ImGui::CalcTextSize(overlay_buf, NULL);
    overlay_size += ImVec2(3.f, 3.f);
    if (overlay_size.x > 0.0f)
        ImGui::RenderTextClipped( bbox.GetBR() - overlay_size, bbox.Max, overlay_buf, NULL, &overlay_size);

    ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%s", GstToolkit::time_to_string(*time).c_str());
    overlay_size = ImGui::CalcTextSize(overlay_buf, NULL);
    overlay_size = ImVec2(3.f, -3.f - overlay_size.y);
    if (overlay_size.x > 0.0f)
        ImGui::RenderTextClipped( bbox.GetBL() + overlay_size, bbox.Max, overlay_buf, NULL, &overlay_size);

    // draw slider grab handle
    if (grab_slider_bb.Max.x > grab_slider_bb.Min.x) {
        window->DrawList->AddRectFilled(grab_slider_bb.Min, grab_slider_bb.Max, grab_slider_color, style.GrabRounding);
    }

    // draw the cursor
    color = ImGui::GetColorU32(style.Colors[ImGuiCol_SliderGrab]);
    pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), time_) - ImVec2(cursor_width, 2.f);
    ImGui::RenderArrow(window->DrawList, pos, color, ImGuiDir_Up, cursor_scale);

    return left_mouse_press;
}

// Draws a timeline showing
// 1) a cursor at position *time in the range [0 duration]
// 2) a line of tick marks indicating time, every step if possible
// 3) a slider handle below the cursor: user can move the slider
// 4) different blocs for each time segment [first second] of the list of segments
// Behavior
// a) Returns TRUE if the left mouse button LMB is pressed over the timeline
// b) the value of *time is changed to the position of the slider handle from user input (LMB)
// c) the list segments is replaced with the list of new segments created by user (empty list otherwise)

bool ImGuiToolkit::TimelineSliderEdit(const char* label, guint64 *time, guint64 duration, guint64 step,
                                  std::list<std::pair<guint64, guint64> >& segments)
{
    static guint64 optimal_tick_marks[12] = { 100 * MILISECOND, 500 * MILISECOND, 1 * SECOND, 2 * SECOND, 5 * SECOND, 10 * SECOND, 20 * SECOND, 1 * MINUTE, 2 * MINUTE, 5 * MINUTE, 10 * MINUTE, 60 * MINUTE };

    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // get style & id
    const ImGuiStyle& style = ImGui::GetStyle();
    const float fontsize = ImGui::GetFontSize();
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    //
    // FIRST PREPARE ALL data structures
    //

    // widget bounding box
    const float height = 2.f * (fontsize + style.FramePadding.y);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), ImGui::CalcItemWidth(), height);
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // cursor size
    const float cursor_scale = 1.f;
    const float cursor_width = 0.5f * fontsize * cursor_scale;

    // TIMELINE is inside the bbox, in a slightly smaller bounding box
    ImRect timeline_bbox(bbox);
    timeline_bbox.Expand( ImVec2(-cursor_width, -style.FramePadding.y) );

    // SLIDER is inside the timeline
    ImRect slider_bbox( timeline_bbox.GetTL() + ImVec2(-cursor_width + 2.f, cursor_width + 4.f ), timeline_bbox.GetBR() + ImVec2( cursor_width - 2.f, 0.f ) );

    // units conversion: from time to float (calculation made with higher precision first)
    float time_ = static_cast<float> ( static_cast<double>(*time) / static_cast<double>(duration) );
    float step_ = static_cast<float> ( static_cast<double>(step) / static_cast<double>(duration) );

    //
    // SECOND GET USER INPUT AND PERFORM CHANGES AND DECISIONS
    //

    // read user input from system
    bool left_mouse_press = false;
    bool right_mouse_press = false;
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = ImGui::FocusableItemRegister(window, id);
        left_mouse_press = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        right_mouse_press = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (focus_requested || left_mouse_press || right_mouse_press || g.NavActivateId == id || g.NavInputId == id)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }

    // active slider if inside bb
    ImRect grab_slider_bb;
    ImU32 grab_slider_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    ImGuiID slider_id = window->GetID("werwerwsdvcsdgfdghdfsgagewrgsvdfhfdghkjfghsdgsdtgewfszdvgfkjfg"); // FIXME: cleaner way to create a valid but useless id that is never active
    if ( slider_bbox.Contains(g.IO.MousePos)  ) {
        slider_id = id; // active id
        grab_slider_color = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    }

    // time Slider behavior
    float time_slider = time_ * 10.f; // x 10 precision on grab
    float time_zero = 0.f;
    float time_end = 10.f;
    bool value_changed = ImGui::SliderBehavior(slider_bbox, slider_id, ImGuiDataType_Float, &time_slider, &time_zero,
                                               &time_end, "%.2f", 1.f, ImGuiSliderFlags_None, &grab_slider_bb);
    if (value_changed){
        //        g_print("slider %f  %ld \n", time_slider, static_cast<guint64> ( static_cast<double>(time_slider) * static_cast<double>(duration) ));
        *time = static_cast<guint64> ( 0.1 * static_cast<double>(time_slider) * static_cast<double>(duration) );
    }

    // segments behavior
    float time_segment_begin = 0.f;
    float time_segment_end = 0.f;
    if (right_mouse_press) {
        time_segment_begin = 0.f;
    }

    //
    // THIRD RENDER
    //

    // Render the bounding box
    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(bbox.Min, bbox.Max, frame_col, true, style.FrameRounding);

    // by default, put a tick mark at every frame step and a large mark every second
    guint64 tick_step = step;
    guint64 large_tick_step = SECOND;

    // how many pixels to represent one frame step?
    float tick_step_pixels = timeline_bbox.GetWidth() * step_;
    // while there is less than 3 pixels between two tick marks (or at last optimal tick mark)
    for ( int i=0; i<10 && tick_step_pixels < 3.f; ++i )
    {
        // try to use the optimal tick marks pre-defined
        tick_step = optimal_tick_marks[i];
        large_tick_step = optimal_tick_marks[i+1];
        tick_step_pixels = timeline_bbox.GetWidth() * static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
    }

    // render the tick marks along TIMELINE
    ImU32 color_in = ImGui::GetColorU32( style.Colors[ImGuiCol_Text] );
    ImU32 color_out = ImGui::GetColorU32( style.Colors[ImGuiCol_TextDisabled] );
    ImU32 color = color_in;
    pos = timeline_bbox.GetTL();
    guint64 tick = 0;
    float tick_percent = 0.f;
    std::list< std::pair<guint64, guint64> >::iterator it = segments.begin();
    while ( tick < duration)
    {
        // large tick mark every large tick
        float tick_length = !(tick%large_tick_step) ? fontsize : style.FramePadding.y;

        // different colors for inside and outside [begin end] of segments
        if ( it != segments.end() ) {

            if ( tick < it->first  )
                color = color_out;
            else if ( tick > it->second ){
                color = color_out;
                it ++;
            }
            else
                color = color_in;
        }

        // draw a tick mark each step
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, tick_length), color);

        // next tick
        tick += tick_step;
        tick_percent = static_cast<float> ( static_cast<double>(tick) / static_cast<double>(duration) );
        pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);
    }

    // loop over segments and EMPTY the list
    // ticks for segments begin & end
    for (std::list< std::pair<guint64, guint64> >::iterator s = segments.begin(); s != segments.end(); s++){
        tick_percent = static_cast<float> ( static_cast<double>(s->first) / static_cast<double>(duration) );
        pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, timeline_bbox.GetHeight()), color_in);
        tick_percent = static_cast<float> ( static_cast<double>(s->second) / static_cast<double>(duration) );
        pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, timeline_bbox.GetHeight()), color_in);
    }
    segments.clear();

    // tick EOF
    window->DrawList->AddLine( timeline_bbox.GetTR(), timeline_bbox.GetTR() + ImVec2(0.f, fontsize), color_in);

    // render text : duration and current time
    char overlay_buf[24];
    ImVec2 overlay_size = ImVec2(0.f, 0.f);
    ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%s", GstToolkit::time_to_string(duration).c_str());
    overlay_size = ImGui::CalcTextSize(overlay_buf, NULL);
    overlay_size += ImVec2(3.f, 3.f);
    if (overlay_size.x > 0.0f)
        ImGui::RenderTextClipped( bbox.GetBR() - overlay_size, bbox.Max, overlay_buf, NULL, &overlay_size);

    ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%s", GstToolkit::time_to_string(*time).c_str());
    overlay_size = ImGui::CalcTextSize(overlay_buf, NULL);
    overlay_size = ImVec2(3.f, -3.f - overlay_size.y);
    if (overlay_size.x > 0.0f)
        ImGui::RenderTextClipped( bbox.GetBL() + overlay_size, bbox.Max, overlay_buf, NULL, &overlay_size);


    // draw slider grab handle
    if (grab_slider_bb.Max.x > grab_slider_bb.Min.x) {
        window->DrawList->AddRectFilled(grab_slider_bb.Min, grab_slider_bb.Max, grab_slider_color, style.GrabRounding);
    }

    // draw the cursor
    color = ImGui::GetColorU32(style.Colors[ImGuiCol_SliderGrab]);
    pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), time_) - ImVec2(cursor_width, 2.f);
    ImGui::RenderArrow(window->DrawList, pos, color, ImGuiDir_Up, cursor_scale);

    return left_mouse_press;
}


void ImGuiToolkit::Bar(float value, float in, float out, float min, float max, const char* title, bool expand)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 size_arg = expand ? ImVec2(-FLT_MIN, 0.0f) : ImVec2(0.0f, 0.0f);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, ImGui::CalcItemWidth(), (g.FontSize + style.FramePadding.y)* 2.0f);
    ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    // calculations
    float range_in = in / (max - min) + min;
    float range_out = out / (max - min) + min;
    float slider = value / (max - min) + min;
    slider = ImSaturate(slider);

    // Render
    ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    // tics
    const ImU32 col_base = ImGui::GetColorU32( ImGuiCol_PlotLines );


    ImVec2 pos0 = ImLerp(bb.Min, bb.Max, 0.1);
    ImVec2 pos1 = ImLerp(bb.Min, bb.Max, 0.2);
    float step = (bb.Max.x - bb.Min.x) / 100.f;
    int i = 0;
    for (float tic = bb.Min.x; tic < bb.Max.x; tic += step, ++i) {
        float tic_lengh = i % 10 ?  style.FramePadding.y : g.FontSize;
        window->DrawList->AddLine( ImVec2(tic, bb.Min.y), ImVec2(tic, bb.Min.y + tic_lengh), col_base);
    }

    bb.Min.y += g.FontSize;
    bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
    const ImVec2 fill_br = ImVec2(ImLerp(bb.Min.x, bb.Max.x, slider), bb.Max.y);

    ImGui::RenderRectFilledRangeH(window->DrawList, bb, ImGui::GetColorU32(ImGuiCol_CheckMark), range_in, range_out, style.FrameRounding);

    char overlay_buf[32];
    ImVec2 overlay_size = ImVec2(0.f, 0.f);
    ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.0f", in);
    overlay_size = ImGui::CalcTextSize(overlay_buf, NULL);
    if (overlay_size.x > 0.0f)
        ImGui::RenderTextClipped(ImVec2( ImClamp(fill_br.x + style.ItemSpacing.x,bb.Min.x, bb.Max.x - overlay_size.x - style.ItemInnerSpacing.x), bb.Min.y), 
                                                bb.Max, title, NULL, &overlay_size, ImVec2(0.0f,0.5f), &bb);


    // Draw handle to move cursor (seek position)
    // ImRect grab_bb;
    // grab_bb.Min.x = 
    // window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(ImGuiCol_SliderGrabActive), style.GrabRounding);
    
    // Draw cursor (actual position)
    ImU32 color = ImGui::GetColorU32(1 ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImGui::RenderArrow(window->DrawList, pos0, color, ImGuiDir_Up);


    // window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);


    // Default displaying the fraction as percentage string, but user can override it
    // char overlay_buf[32];
    // if (!title)
    // {
    //     ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.0f", fraction*100+0.01f);
    //     title = overlay_buf;
    // }

    // ImVec2 overlay_size = ImGui::CalcTextSize(title, NULL);
    // if (overlay_size.x > 0.0f)
    //     ImGui::RenderTextClipped(ImVec2( ImClamp(fill_br.x + style.ItemSpacing.x,bb.Min.x, bb.Max.x - overlay_size.x - style.ItemInnerSpacing.x), bb.Min.y), 
    //                                             bb.Max, title, NULL, &overlay_size, ImVec2(0.0f,0.5f), &bb);
}


void ImGuiToolkit::SetFont(ImGuiToolkit::font_style style, const std::string &ttf_font_name, int pointsize, int oversample)
{
    // Font Atlas ImGui Management
    ImGuiIO& io = ImGui::GetIO();    
    const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesDefault();
    std::string filename = "fonts/" + ttf_font_name +  ".ttf";
    std::string fontname = ttf_font_name + ", " + std::to_string(pointsize) + "px";
    ImFontConfig font_config;
    fontname.copy(font_config.Name, 40);    
    font_config.FontDataOwnedByAtlas = false; // data will be copied in font atlas
    // TODO : calculate oversampling as function of the maximum texture size
    font_config.OversampleH = CLAMP( oversample * 2 + 1, 1, 7 );
    font_config.OversampleV = CLAMP( oversample, 0, 5 );

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

void ImGuiToolkit::PushFont(ImGuiToolkit::font_style style)
{
    if (fontmap.count(style) > 0)
        ImGui::PushFont( fontmap[style] );
    else
        ImGui::PushFont( NULL );    
}


void ImGuiToolkit::ShowStats(bool *p_open, int* p_corner)
{
    if (!p_corner || !p_open)
        return;

    const float DISTANCE = 10.0f;
    int corner = *p_corner;
    ImGuiIO& io = ImGui::GetIO();
    if (corner != -1)
    {
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    }

    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

    if (ImGui::Begin("v-mix statistics", NULL, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        if (ImGui::IsMousePosValid())
            ImGui::Text("Mouse  (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
        else
            ImGui::Text("Mouse  <invalid>");

        ImGui::Text("Window  (%.1f,%.1f)", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("HiDPI (retina) %s", io.DisplayFramebufferScale.x > 1.f ? "on" : "off");
//        ImGui::Text("DPI Scale (%.1f,%.1f)", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImGui::Text("Rendering %.1f FPS", io.Framerate);
        ImGui::PopFont();

        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Custom",       NULL, corner == -1)) *p_corner = -1;
//            if (ImGui::MenuItem("Top-left",     NULL, corner == 0)) corner = 0;
            if (ImGui::MenuItem("Top",    NULL, corner == 1)) *p_corner = 1;
//            if (ImGui::MenuItem("Bottom-left",  NULL, corner == 2)) corner = 2;
            if (ImGui::MenuItem("Bottom", NULL, corner == 3)) *p_corner = 3;
            if (p_open && ImGui::MenuItem("Close")) *p_open = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

ImVec4 ImGuiToolkit::GetHighlightColor()
{
    ImVec4* colors = ImGui::GetStyle().Colors;
    return colors[ImGuiCol_CheckMark];
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
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.78f);
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
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.94f, 0.57f, 0.01f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.82f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.98f, 0.59f, 0.26f, 0.64f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.13f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);

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
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.39f, 0.39f, 0.39f, 0.78f);
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
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.94f, 0.57f, 0.01f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.82f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.64f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.13f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
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
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.78f);
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
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
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
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.94f, 0.57f, 0.01f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.82f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.64f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.13f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
    }

}

