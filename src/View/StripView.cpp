/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glib.h>
#include <glm/glm.hpp>

#include "IconsFontAwesome5.h"
#include "Toolkit/ImGuiToolkit.h"

#include <string>

#include "defines.h"
#include "Mixer.h"
#include "Scene/Decorations.h"
#include "Scene/UpdateCallback.h"
#include "Source/SessionThumbnailSource.h"
#include "Toolkit/BaseToolkit.h"
#include "UserInterfaceManager.h"

#include "imgui.h"
#include "StripView.h"

// half-height of thumbnails, in fraction of the strip height
#define STRIP_ITEM_HEIGHT 0.28f
// gap between thumbnails, in fraction of thumbnail half-height
#define STRIP_ITEM_GAP 0.6f
// duration of the scrolling of the strip, in ms
#define STRIP_SCROLL_DURATION 300.f
// margin of the output in its area
#define OUTPUT_MARGIN 0.82f
// padding of the information window, in fraction of window height
#define INFO_PADDING 0.04f
// default aspect ratio of thumbnails not yet loaded
#define DEFAULT_ASPECT_RATIO (16.f / 9.f)

StripView::StripView(Mode m, const std::string &item_name) : View(m), current_(-1), leave_target_(-1),
    item_name_(item_name), strip_animate_(false), strip_scroll_(0.f), strip_target_(glm::vec3(1e6f)),
    item_height_(1.f), arrow_latched_(false), start_time_(0)
{
    // dark background covering the view, behind all
    Surface *background = new Surface( new Shader );
    background->scale_ = glm::vec3(20.f, 20.f, 1.f);
    background->translation_.z = -0.5f;
    background->shader()->color = glm::vec4( COLOR_PRESENTATION_BGROUND, 0.7f );
    scene.bg()->attach(background);

    // output of the mix in background
    output_surface_ = new Surface;
    scene.bg()->attach(output_surface_);

    output_frame_ = new Frame(Frame::SHARP, Frame::THIN, Frame::GLOW);
    output_frame_->color = glm::vec4( COLOR_FRAME, 1.0f );
    scene.bg()->attach(output_frame_);

    // strip of thumbnails in workspace
    strip_ = new Group;
    scene.ws()->attach(strip_);
}

void StripView::clearItems()
{
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        strip_->detach( (*it)->group(View::PRESENTATION) );
        delete (*it);
    }
    items_.clear();
    current_ = -1;
    leave_target_ = -1;
}

void StripView::addItem(SessionThumbnailSource *item)
{
    Group *g = item->group(View::PRESENTATION);
    g->visible_ = true;
    strip_->attach(g);
    items_.push_back(item);
}

void StripView::restart()
{
    // start of presentation
    start_time_ = g_get_monotonic_time();

    // jump to strip position (no animation)
    strip_->clearCallbacks();
    strip_target_ = glm::vec3(1e6f);
    ++View::need_deep_update_;
}

void StripView::setCurrent(int i, bool animate)
{
    current_ = i;

    // current item has a large frame
    for (size_t j = 0; j < items_.size(); ++j)
        items_[j]->setMode( static_cast<int>(j) == current_ ? Source::CURRENT : Source::VISIBLE );

    // scroll strip to center on current
    strip_scroll_ = 0.f;
    strip_animate_ = animate;
}

void StripView::zoom(float factor)
{
    // scroll the strip by about one thumbnail per step
    // (clamped to the ends of the list in layout)
    strip_scroll_ += factor * (2.f * DEFAULT_ASPECT_RATIO + STRIP_ITEM_GAP) * item_height_;
    strip_animate_ = true;
}

int StripView::nextIndex() const
{
    // nearest valid item after current (no wrap-around)
    for (int i = current_ + 1; i < static_cast<int>(items_.size()); ++i) {
        if ( available(i) )
            return i;
    }
    return -1;
}

int StripView::previousIndex() const
{
    // nearest valid item before current (no wrap-around)
    for (int i = current_ - 1; i > -1; --i) {
        if ( available(i) )
            return i;
    }
    return -1;
}

void StripView::next()
{
    if (!busy())
        open( nextIndex() );
}

void StripView::previous()
{
    if (!busy())
        open( previousIndex() );
}

void StripView::layout()
{
    // visible area is x in [-S, S] and y in [-S/ar, S/ar]
    const float S = SCENE_UNIT;
    const float ar = Rendering::manager().mainWindow().aspectRatio();

    // the area left of the navigator bar is not used
    // (NB: window width in screen coordinates, as navigator width)
    const float W = static_cast<float>(Rendering::manager().mainWindow().width()) / Rendering::manager().mainWindow().dpiScale();
    const float L = W > 0.f ? UserInterface::manager().navigator.width() / W : 0.f;

    // coordinates from fractions of the area (fy from top)
    auto X = [&](float fx) { return -S + 2.f * S * (L + (1.f - L) * fx); };
    auto Y = [&](float fy) { return S / ar - 2.f * S / ar * fy; };

    // Output in the top-left 2/3 x 2/3 area
    FrameBuffer *output = Mixer::manager().liveSession()->frame();
    const float output_ar = output ? output->aspectRatio() : DEFAULT_ASPECT_RATIO;
    const float half_w = 0.5f * (X(2.f / 3.f) - X(0.f));
    const float half_h = 0.5f * (Y(0.f) - Y(2.f / 3.f));
    const float s = OUTPUT_MARGIN * MIN(half_h, half_w / output_ar);
    scene.bg()->translation_ = glm::vec3(X(1.f / 3.f), Y(1.f / 3.f), 0.f);
    scene.bg()->scale_ = glm::vec3(s, s, 1.f);
    output_surface_->scale_.x = output_ar;
    output_frame_->scale_.x = output_ar;
    if (output)
        output_surface_->setTextureIndex( output->texture() );

    // Strip of thumbnails in the bottom third
    item_height_ = STRIP_ITEM_HEIGHT * (Y(2.f / 3.f) - Y(1.f));
    float x = 0.f;
    float previous_half_width = 0.f;
    for (size_t i = 0; i < items_.size(); ++i) {
        const FrameBuffer *fb = items_[i]->frame();
        const float half_width = item_height_ * (fb ? fb->aspectRatio() : DEFAULT_ASPECT_RATIO);
        if (i > 0)
            x += previous_half_width + STRIP_ITEM_GAP * item_height_ + half_width;
        Group *g = items_[i]->group(View::PRESENTATION);
        g->translation_ = glm::vec3(x, 0.f, 0.f);
        g->scale_ = glm::vec3(item_height_, item_height_, 1.f);
        previous_half_width = half_width;
    }

    // strip is positionned to center the current item, shifted by user scroll
    // (keeping the center of the area between the first and last items)
    float x_current = 0.f;
    if (!items_.empty()) {
        x_current = items_[ MAX(current_, 0) ]->group(View::PRESENTATION)->translation_.x;
        strip_scroll_ = CLAMP(strip_scroll_, x_current - x, x_current);
    }
    else
        strip_scroll_ = 0.f;
    const glm::vec3 target(X(0.5f) - x_current + strip_scroll_, Y(5.f / 6.f), 0.f);

    if ( glm::distance(target, strip_target_) > EPSILON ) {
        // animate if requested or already moving
        if (strip_animate_ || !strip_->update_callbacks_.empty()) {
            strip_->clearCallbacks();
            strip_->update_callbacks_.push_back( new MoveToCallback(target, STRIP_SCROLL_DURATION) );
        }
        // otherwise jump to target
        else
            strip_->translation_ = target;
        strip_target_ = target;
    }
    strip_animate_ = false;
}

void StripView::update(float dt)
{
    // follow changes of live session
    sync();

    // place elements (adapts to window size)
    layout();

    // update scene
    View::update(dt);

    // update thumbnails
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        (*it)->update(dt);
        if ( !(*it)->ready() )
            (*it)->render();
    }

    // perform transition
    animate(dt);

    // leave view once the requested item is opened
    if ( leave_target_ > -1 && !busy() ) {
        if ( isOpened(leave_target_) )
            Mixer::manager().setView(View::MIXING);
        // failed to open: stay
        else
            leave_target_ = -1;
    }
}

void StripView::draw()
{
    // draw scene of this view
    View::draw();

    const ImGuiIO& io = ImGui::GetIO();

    // labels of thumbnails
    ImDrawList *draw_list = ImGui::GetBackgroundDrawList();
    for (size_t i = 0; i < items_.size(); ++i) {
        const Group *g = items_[i]->group(View::PRESENTATION);
        const glm::vec3 center = strip_->translation_ + g->translation_;
        const float half_width = g->scale_.x * (items_[i]->frame() ? items_[i]->frame()->aspectRatio() : DEFAULT_ASPECT_RATIO);
        const glm::vec2 left  = Rendering::manager().project(center + glm::vec3(-half_width, 1.1f * item_height_, 0.f), scene.root()->transform_, false);
        const glm::vec2 right = Rendering::manager().project(center + glm::vec3( half_width, 1.1f * item_height_, 0.f), scene.root()->transform_, false);

        // ignore labels outside of window
        if (right.x < 0.f || left.x > io.DisplaySize.x)
            continue;

        // truncate label to the width of thumbnail
        std::string text = label( static_cast<int>(i) );
        const float width = right.x - left.x;
        ImVec2 size = ImGui::CalcTextSize(text.c_str());
        if (size.x > width && size.x > 0.f) {
            text = BaseToolkit::truncated(text, MAX(1, static_cast<int>(text.size() * width / size.x) - 1));
            size = ImGui::CalcTextSize(text.c_str());
        }

        ImVec4 color = static_cast<int>(i) == current_ ? ImVec4(COLOR_TRANSITION_SOURCE, 1.f) : ImVec4(COLOR_DEFAULT_SOURCE, 1.f);
        if ( !available( static_cast<int>(i) ) )
            color.w = 0.4f;
        draw_list->AddText(ImVec2(0.5f * (left.x + right.x - size.x), left.y - ImGui::GetTextLineHeightWithSpacing()), ImGui::GetColorU32(color), text.c_str());
    }

    const float pad = INFO_PADDING * io.DisplaySize.y;

    // information window in the top-right of output surface
    glm::vec2 P(output_surface_->translation_.x + output_surface_->scale_.x, output_surface_->translation_.y + output_surface_->scale_.y);
    P = Rendering::manager().project(glm::vec3(P, 0.f), scene.bg()->transform_, false);
    ImGui::SetNextWindowPos(ImVec2(P.x + pad, P.y), ImGuiCond_Always);

    // information window with same height of output surface
    glm::vec2 botton_right = glm::vec2(output_surface_->translation_.x + output_surface_->scale_.x, output_surface_->translation_.y - output_surface_->scale_.y);
    botton_right = Rendering::manager().project(glm::vec3(botton_right, 0.f), scene.bg()->transform_, false);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - botton_right.x - 2.f * pad, botton_right.y - P.y), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    const std::string window_name = "##Strip" + std::to_string(mode_);
    const bool visible = ImGui::Begin(window_name.c_str(), NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground
                                      | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar();
    if (visible)
    {
        // colors for UI
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.16f, 0.99f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 0.99f));

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);

        // no navigation during a transition
        const bool is_busy = busy();

        // Button PREVIOUS
        const bool no_previous = is_busy || previousIndex() < 0;
        ImGuiToolkit::PushDisabled(no_previous);
        if (ImGui::Button(ICON_FA_ANGLE_LEFT ))
            previous();
        ImGuiToolkit::PopDisabled(no_previous);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip( ("Previous " + item_name_).c_str() );

        // Indication of advance in the list
        ImGuiToolkit::PushDisabled(is_busy);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (current_ > -1) {
            ImGui::Text(" %d/%ld ", current_ + 1, items_.size());
            ImGui::SameLine(0, IMGUI_SAME_LINE);
        }
        ImGuiToolkit::PopDisabled(is_busy);

        // Button NEXT
        const bool no_next = is_busy || nextIndex() < 0;
        ImGuiToolkit::PushDisabled(no_next);
        if (ImGui::Button(ICON_FA_ANGLE_RIGHT ))
            next();
        ImGuiToolkit::PopDisabled(no_next);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip( ("Next " + item_name_).c_str() );

        // time elapsed since start of presentation
        ImGui::SameLine();
        const gint64 elapsed = (g_get_monotonic_time() - start_time_) / G_USEC_PER_SEC;
        char text[256];
        snprintf(text, sizeof(text), "%02d:%02d:%02d",
                 static_cast<int>(elapsed / 3600), static_cast<int>((elapsed / 60) % 60), static_cast<int>(elapsed % 60));
        const ImVec2 text_size = ImGui::CalcTextSize(text, NULL, true);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - text_size.x);
        ImGui::Text("%s", text);
        ImGui::PopFont();

        // Name of the list (small text)
        ImGui::Separator();
        ImGui::Text("%s", title().c_str());

        // Information on current item
        drawInformation(is_busy);

        // Buttons
        const float bottom = ImGui::GetScrollY() + ImGui::GetWindowHeight()
                             - ImGui::GetStyle().WindowPadding.y - ImGui::GetFrameHeight();
        ImGui::SetCursorPosY( MAX(ImGui::GetCursorPosY(), bottom) );
        drawButtons();

        ImGui::PopStyleColor(6);
    }

    ImGui::End();
}

std::pair<Node *, glm::vec2> StripView::pick(glm::vec2 P)
{
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // open item clicked
    if (pick.first != nullptr) {
        for (size_t i = 0; i < items_.size(); ++i) {
            if ( items_[i]->contains(pick.first) ) {
                if ( static_cast<int>(i) != current_ || !isOpened( static_cast<int>(i) ) )
                    open( static_cast<int>(i) );
                break;
            }
        }
    }

    return pick;
}

bool StripView::doubleclic (glm::vec2 P)
{
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // double clic on an item
    if (pick.first != nullptr) {
        for (size_t i = 0; i < items_.size(); ++i) {
            if ( items_[i]->contains(pick.first) ) {
                const int index = static_cast<int>(i);
                if ( !busy() ) {
                    // double clic on the currently opened item: just exit
                    if ( isOpened(index) ) {
                        Mixer::manager().setView(View::MIXING);
                        return true;
                    }
                    // otherwise open it
                    open(index);
                }
                // exit when done opening the item
                // (NB: usually already started on first clic)
                if ( index == current_ && busy() )
                    leave_target_ = index;
                break;
            }
        }
    }

    // double clic is always used in this view
    return true;
}

View::Cursor StripView::over(glm::vec2 P)
{
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    if (pick.first != nullptr) {
        for (size_t i = 0; i < items_.size(); ++i) {
            if ( items_[i]->contains(pick.first) ) {
                if ( !available( static_cast<int>(i) ) )
                    return Cursor(Cursor_NotAllowed, unavailableReason());
                return Cursor(Cursor_Hand);
            }
        }
    }

    return Cursor();
}

void StripView::arrow (glm::vec2 movement)
{
    // react once to left / right key press
    if (!arrow_latched_ && movement.x != 0.f) {
        arrow_latched_ = true;
        if (movement.x > 0.f)
            next();
        else
            previous();
    }
}

void StripView::terminate (bool force)
{
    View::terminate(force);

    // release arrow key
    arrow_latched_ = false;
}
