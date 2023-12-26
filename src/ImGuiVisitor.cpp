/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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


#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <tinyxml2.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "Log.h"
#include "defines.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "CloneSource.h"
#include "FrameBufferFilter.h"
#include "DelayFilter.h"
#include "ImageFilter.h"
#include "RenderSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "ScreenCaptureSource.h"
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "MultiFileSource.h"
#include "TextSource.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "Settings.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "ActionManager.h"
#include "Mixer.h"

#include "imgui.h"
#include "ImGuiToolkit.h"
#include "BaseToolkit.h"
#include "UserInterfaceManager.h"
#include "SystemToolkit.h"

#include "ImGuiVisitor.h"

ImGuiVisitor::ImGuiVisitor()
{

}

void ImGuiVisitor::visit(Node &)
{

}

void ImGuiVisitor::visit(Group &n)
{
    // MODEL VIEW
    ImGui::PushID(std::to_string(n.id()).c_str());

    if (ImGuiToolkit::ButtonIcon(1, 16)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        n.rotation_.z = 0.f;
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
        Action::manager().store("Geometry Reset");
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::Text("Geometry");

    if (ImGuiToolkit::ButtonIcon(6, 15)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        Action::manager().store("Position 0.0, 0.0");
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    float translation[2] = { n.translation_.x, n.translation_.y};
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("Position", translation, -5.0, 5.0) )
    {
        n.translation_.x = translation[0];
        n.translation_.y = translation[1];
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Position " << std::setprecision(3) << n.translation_.x << ", " << n.translation_.y;
        Action::manager().store(oss.str());
    }
    if (ImGuiToolkit::ButtonIcon(3, 15))  {
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
        Action::manager().store("Scale 1.0 x 1.0");
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    float scale[2] = { n.scale_.x, n.scale_.y} ;
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("Scale", scale, -MAX_SCALE, MAX_SCALE, "%.2f") )
    {
        n.scale_.x = CLAMP_SCALE(scale[0]);
        n.scale_.y = CLAMP_SCALE(scale[1]);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Scale " << std::setprecision(3) << n.scale_.x << " x " << n.scale_.y;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(18, 9)){
        n.rotation_.z = 0.f;
        Action::manager().store("Angle 0.0");
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderAngle("Angle", &(n.rotation_.z), -180.f, 180.f) ;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::ostringstream oss;
        oss << "Angle " << std::setprecision(3) << n.rotation_.z * 180.f / M_PI;
        Action::manager().store(oss.str());
    }


    ImGui::PopID();

    // spacing
    ImGui::Spacing();
}

void ImGuiVisitor::visit(Switch &n)
{
    if (n.numChildren()>0)
        n.activeChild()->accept(*this);
}

void ImGuiVisitor::visit(Scene &n)
{
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Scene Property Tree"))
    {
        n.root()->accept(*this);
    }
}

void ImGuiVisitor::visit(Primitive &n)
{
    ImGui::PushID(std::to_string(n.id()).c_str());
    ImGui::Text("Primitive");

    n.shader()->accept(*this);

    ImGui::PopID();
}

void ImGuiVisitor::visit(FrameBufferSurface &)
{
    ImGui::Text("Framebuffer");
}

void ImGuiVisitor::visit(MediaPlayer &)
{
    ImGui::Text("Media Player");
}

void ImGuiVisitor::visit(Shader &n)
{
    ImGui::PushID(std::to_string(n.id()).c_str());

    // Base color
//    if (ImGuiToolkit::ButtonIcon(10, 2)) {
//        n.blending = Shader::BLEND_OPACITY;
//        n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
//    }
//    ImGui::SameLine(0, IMGUI_SAME_LINE);
//    ImGui::ColorEdit3("Color", glm::value_ptr(n.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel ) ;
//    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int mode = n.blending;
    if (ImGui::Combo("Blending", &mode, "Normal\0Screen\0Subtract\0Multiply\0Soft light"
                     "\0Hard light\0Soft subtract\0Lighten only\0") ) {
        n.blending = Shader::BlendMode(mode);

        std::ostringstream oss;
        oss << "Blending ";
        switch(n.blending) {
        case Shader::BLEND_OPACITY:
            oss<<"Normal";
            break;
        case Shader::BLEND_SCREEN:
            oss<<"Screen";
            break;
        case Shader::BLEND_SUBTRACT:
            oss<<"Subtract";
            break;
        case Shader::BLEND_MULTIPLY:
            oss<<"Multiply";
            break;
        case Shader::BLEND_HARD_LIGHT:
            oss<<"Hard light";
            break;
        case Shader::BLEND_SOFT_LIGHT:
            oss<<"Soft light";
            break;
        case Shader::BLEND_SOFT_SUBTRACT:
            oss<<"Soft subtract";
            break;
        case Shader::BLEND_LIGHTEN_ONLY:
            oss<<"Lighten only";
            break;
        case Shader::BLEND_NONE:
            oss<<"None";
            break;
        }
        Action::manager().store(oss.str());
    }

    ImGui::PopID();
}

void ImGuiVisitor::visit(ImageProcessingShader &n)
{
    ImGuiIO& io = ImGui::GetIO();
    std::ostringstream oss;
    ImGui::PushID(std::to_string(n.id()).c_str());

    ///
    /// GAMMA
    ///
    ImGui::ColorEdit3("Gamma Color", glm::value_ptr(n.gamma), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) ;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        char buf[64];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", ImClamp((int)ceil(255.f * n.gamma.x),0,255),
                       ImClamp((int)ceil(255.f * n.gamma.y),0,255), ImClamp((int)ceil(255.f * n.gamma.z),0,255));
        oss << "Gamma color " << buf;
        Action::manager().store(oss.str());
    }

    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float val = log10f(n.gamma.w);
    if ( ImGui::SliderFloat("##Gamma", &val, -1.f, 1.f, "%.3f", 2.f) )
        n.gamma.w = powf(10.f, val);
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        val = CLAMP(val + 0.001f * io.MouseWheel, -1.f, 1.f);
        n.gamma.w = powf(10.f, val);
        oss << "Gamma " << std::setprecision(3) << val;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        oss << "Gamma " << std::setprecision(3) << val;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Gamma")) {
        n.gamma = glm::vec4(1.f, 1.f, 1.f, 1.f);
        oss << "Gamma 0.0, #FFFFFF";
        Action::manager().store(oss.str());
    }

    ///
    /// BRIGHTNESS
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("##Brightness", &n.brightness, -1.0, 1.0, "%.3f", 2.f);
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        n.brightness = CLAMP(n.brightness + 0.001f * io.MouseWheel, -1.f, 1.f);
        oss << "Brightness " << std::setprecision(3) << n.brightness;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        oss << "Brightness " << std::setprecision(3) << n.brightness;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Brightness")) {
        n.brightness = 0.f;
        oss << "Brightness " << std::setprecision(2) << n.brightness;
        Action::manager().store(oss.str());
    }

    ///
    /// CONTRAST
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("##Contrast", &n.contrast, -1.0, 1.0, "%.3f", 2.f);
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        n.contrast = CLAMP(n.contrast + 0.001f * io.MouseWheel, -1.f, 1.f);
        oss << "Contrast " << std::setprecision(3) << n.contrast;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        oss << "Contrast " << std::setprecision(3) << n.contrast;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Contrast")) {
        n.contrast = 0.f;
        oss << "Contrast " << std::setprecision(2) << n.contrast;
        Action::manager().store(oss.str());
    }

    ///
    /// SATURATION
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("##Saturation", &n.saturation, -1.0, 1.0, "%.3f", 2.f);
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        n.saturation = CLAMP(n.saturation + 0.001f * io.MouseWheel, -1.f, 1.f);
        oss << "Saturation " << std::setprecision(3) << n.saturation;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        oss << "Saturation " << std::setprecision(3) << n.saturation;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Saturation")) {
        n.saturation = 0.f;
        oss << "Saturation " << std::setprecision(2) << n.saturation;
        Action::manager().store(oss.str());
    }

    ///
    /// HUE SHIFT
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("##Hue", &n.hueshift, 0.0, 1.0, "%.3f");
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        n.hueshift = CLAMP(n.hueshift + 0.001f * io.MouseWheel, 0.f, 1.f);
        oss << "Hue shift " << std::setprecision(3) << n.hueshift;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        oss << "Hue shift " << std::setprecision(3) << n.hueshift;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Hue ")) {
        n.hueshift = 0.f;
        oss << "Hue shift  " << std::setprecision(2) << n.hueshift;
        Action::manager().store(oss.str());
    }

    ///
    /// POSTERIZATION
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float posterized = n.nbColors < 1 ? 257.f : n.nbColors;
    if (ImGui::SliderFloat("##Posterize", &posterized, 257.f, 1.f, posterized > 256.f ? "Full range" : "%.0f colors", 0.5f)) {
        n.nbColors = posterized > 256 ? 0.f : posterized;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        n.nbColors = CLAMP( n.nbColors + io.MouseWheel, 1.f, 257.f);
        if (n.nbColors == 0) oss << "Full range"; else oss << n.nbColors;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Posterize ";
        if (n.nbColors == 0) oss << "Full range"; else oss << n.nbColors;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Posterize ")) {
        n.nbColors = 0.f;
        oss << "Posterize Full range";
        Action::manager().store(oss.str());
    }

    ///
    /// THRESHOLD
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float threshold = n.threshold < 0.001f ? 0.f : n.threshold;
    if (ImGui::SliderFloat("##Threshold", &threshold, 0.f, 1.f, threshold < 0.001f ? "None" : "%.3f") ){
        n.threshold = threshold < 0.001f ? 0.f : threshold;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        n.threshold = CLAMP(n.threshold + 0.001f * io.MouseWheel, 0.f, 1.f);
        if (n.threshold < 0.001f) oss << "None"; else oss << std::setprecision(3) << n.threshold;
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        oss << "Threshold ";
        if (n.threshold < 0.001f) oss << "None"; else oss << std::setprecision(3) << n.threshold;
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Threshold ")) {
        n.threshold = 0.f;
        oss << "Threshold None";
        Action::manager().store(oss.str());
    }

    ///
    /// INVERSION
    ///
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::Combo("##Invert", &n.invert, "None\0Color RGB\0Luminance\0"))
        Action::manager().store("Invert " + std::string(n.invert<1 ? "None": (n.invert>1 ? "Luminance" : "Color")));
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Invert ")) {
        n.invert = 0;
        oss << "Invert None";
        Action::manager().store(oss.str());
    }

    ImGui::PopID();

    ImGui::Spacing();
}

void ImGuiVisitor::visit (Source& s)
{
    const float preview_width = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
    const float preview_height = 4.5f * ImGui::GetFrameHeightWithSpacing();
    const float space = ImGui::GetStyle().ItemSpacing.y;

    ImGui::PushID(std::to_string(s.id()).c_str());

    // blending selection
    s.blendingShader()->accept(*this);

    // Draw different info if failed or succeed
    if ( !s.failed() ) {

        // remember where we were...
        ImVec2 pos = ImGui::GetCursorPos();

        // preview
        float width = preview_width;
        float height = s.frame()->projectionSize().y * width / ( s.frame()->projectionSize().x * s.frame()->aspectRatio());
        if (height > preview_height - space) {
            height = preview_height - space;
            width = height * s.frame()->aspectRatio() * ( s.frame()->projectionSize().x / s.frame()->projectionSize().y);
        }
        // centered image
        if (s.ready()) {
            ImGui::SetCursorPos( ImVec2(pos.x + 0.5f * (preview_width-width), pos.y + 0.5f * (preview_height-height-space)) );
            ImGui::Image((void*)(uintptr_t) s.frame()->texture(), ImVec2(width, height));
        }
        // inform on visibility status
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y ) );
        if (s.active()) {
            if (s.blendingShader()->color.a > 0.f)
                ImGuiToolkit::Indication("Visible", ICON_FA_SUN);
            else
                ImGuiToolkit::Indication("Not visible", ICON_FA_CLOUD_SUN);
        }
        else
            ImGuiToolkit::Indication("Inactive", ICON_FA_SNOWFLAKE);

        // Inform on link
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + ImGui::GetFrameHeightWithSpacing()) );
        if (s.mixingGroup() != nullptr) {
            if (ImGuiToolkit::IconButton(ICON_FA_LINK, "Linked")){
                Mixer::selection().clear();
                Mixer::selection().add( s.mixingGroup()->getCopy() );
                UserInterface::manager().setView(View::MIXING);
            }
        }
        else
            ImGuiToolkit::Indication("not Linked", ICON_FA_UNLINK);

        // Inform on workspace
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + 2.1f * ImGui::GetFrameHeightWithSpacing()) );
        static std::map< int, std::pair<int, std::string> > workspaces_ {
            { Source::WORKSPACE_BACKGROUND,  {10, "in Background"}},
            { Source::WORKSPACE_CENTRAL,  {11, "in Workspace"}},
            { Source::WORKSPACE_FOREGROUND,  {12, "in Foreground"}}
        };
        // in Geometry view, offer to switch current workspace to the source workspace
        if (Settings::application.current_view == View::GEOMETRY) {
            if (ImGuiToolkit::IconButton( workspaces_[s.workspace()].first, 16, workspaces_[s.workspace()].second.c_str())) {
                Settings::application.current_workspace=s.workspace();
                Mixer::manager().setCurrentSource(s.id());
            }
        }
        // otherwise in other views, just draw in indicator
        else
            ImGuiToolkit::Indication(workspaces_[s.workspace()].second.c_str(), workspaces_[s.workspace()].first, 16);

        // locking
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + preview_height - ImGui::GetFrameHeightWithSpacing()) );
        static const char *tooltip[2] = {"Unlocked", "Locked"};
        bool l = s.locked();
        if (ImGuiToolkit::IconToggle(15,6,17,6, &l, tooltip ) ) {
            s.setLocked(l);
            if (l) {
                Mixer::selection().clear();
                Action::manager().store(s.name() + std::string(": lock."));
            }
            else {
                Mixer::selection().set(&s);
                Action::manager().store(s.name() + std::string(": unlock."));
            }
        }

        // Filter
        bool on = s.imageProcessingEnabled();
        ImGui::SetCursorPos( ImVec2( pos.x, pos.y + preview_height));
        if (on)
            ImGui::Text(ICON_FA_PALETTE "  Color correction");
        else
            ImGuiToolkit::Indication("Color correction filter is disabled", ICON_FA_PALETTE "  Color correction");
        pos = ImGui::GetCursorPos();

        // menu icon for image processing
        ImGui::SameLine(preview_width, 2 * IMGUI_SAME_LINE);
        static uint counter_menu_timeout = 0;
        if (ImGuiToolkit::IconButton(5, 8) || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            counter_menu_timeout=0;
            ImGui::OpenPopup( "MenuImageProcessing" );
        }

        if (ImGui::BeginPopup( "MenuImageProcessing" ))
        {
            if (ImGui::MenuItem("Enable", NULL, &on)) {
                std::ostringstream oss;
                oss << s.name() << ": " << ( on ? "Enable Color correction" : "Disable Color correction");
                Action::manager().store(oss.str());
                s.setImageProcessingEnabled(on);
            }

            if (s.processingshader_link_.connected()) {
                if (ImGui::MenuItem( "Unfollow", NULL, false, on)){
                    s.processingshader_link_.disconnect();
                }
            }
            else {
                if (ImGui::MenuItem("Reset", NULL, false, on )){
                    ImageProcessingShader defaultvalues;
                    s.processingShader()->copy(defaultvalues);
                    s.processingshader_link_.disconnect();
                    std::ostringstream oss;
                    oss << s.name() << ": " << "Reset Filter";
                    Action::manager().store(oss.str());
                }
                if (ImGui::MenuItem("Copy", NULL, false, on )){
                    std::string clipboard = SessionVisitor::getClipboard(s.processingShader());
                    if (!clipboard.empty())
                        ImGui::SetClipboardText(clipboard.c_str());
                }
                const char *clipboard = ImGui::GetClipboardText();
                const bool can_paste = (clipboard != nullptr && SessionLoader::isClipboard(clipboard));
                if (ImGui::MenuItem("Paste", NULL, false, can_paste)) {
                    SessionLoader::applyImageProcessing(s, clipboard);
                    std::ostringstream oss;
                    oss << s.name() << ": " << "Change Filter";
                    Action::manager().store(oss.str());
                }
                //            // NON-stable style follow mechanism
                //            ImGui::Separator();
                //            if (ImGui::BeginMenu("Follow", on))
                //            {
                //                for (auto mpit = Mixer::manager().session()->begin();
                //                     mpit != Mixer::manager().session()->end(); mpit++ )
                //                {
                //                    std::string label = (*mpit)->name();
                //                    if ( (*mpit)->id() != s.id() &&
                //                         (*mpit)->imageProcessingEnabled() &&
                //                         !(*mpit)->processingshader_link_.connected()) {
                //                        if (ImGui::MenuItem( label.c_str() )){
                //                            s.processingshader_link_.connect(*mpit);
                //                            s.touch();
                //                        }
                //                    }
                //                }
                //                ImGui::EndMenu();
                //            }
            }

            if (ImGui::IsWindowHovered())
                counter_menu_timeout=0;
            else if (++counter_menu_timeout > 10)
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (s.imageProcessingEnabled()) {

            // full panel for image processing
            ImGui::SetCursorPos( pos );
            ImGui::Spacing();

            if (s.processingshader_link_.connected()) {
                Source *target = s.processingshader_link_.source();
                ImGui::Text("Following");
                if ( target != nullptr && ImGui::Button(target->name().c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
                    Mixer::manager().setCurrentSource(target);
            }
            else
                s.processingShader()->accept(*this);
        }

    }
    else {

        // remember where we were...
        ImVec2 pos = ImGui::GetCursorPos();

        // preview (black texture)
        float width = preview_width;
        float height = s.frame()->projectionSize().y * width / ( s.frame()->projectionSize().x * s.frame()->aspectRatio());
        if (height > preview_height - space) {
            height = preview_height - space;
            width = height * s.frame()->aspectRatio() * ( s.frame()->projectionSize().x / s.frame()->projectionSize().y);
        }
        // centered image
        ImGui::SetCursorPos( ImVec2(pos.x + 0.5f * (preview_width-width), pos.y + 0.5f * (preview_height-height-space)) );
        ImGui::Image((void*)(uintptr_t) s.frame()->texture(), ImVec2(width, height));

        // centered icon of failed (skull)
        ImGui::SetCursorPos( ImVec2(pos.x + (width  -ImGui::GetFrameHeightWithSpacing())* 0.5f ,
                                    pos.y + (height -ImGui::GetFrameHeightWithSpacing()) * 0.5f) );
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text(ICON_FA_SKULL);
        ImGui::PopFont();

        // back
        ImGui::SetCursorPos( ImVec2( pos.x, pos.y + preview_height));
    }

    ImGui::PopID();

    ImGui::Spacing();
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::Text("%s", s.info().c_str());

}

void ImGuiVisitor::visit (MediaSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // Media info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip( s.path().c_str() );

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
            top.x += ImGui::GetFrameHeight();
        }
        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(3, 5, "Show in finder"))
            SystemToolkit::open(SystemToolkit::path_filename(s.path()));

        MediaPlayer *mp = s.mediaplayer();
        if (mp && !mp->isImage()) {

            // information on gstreamer effect filter
            if ( mp->videoEffectAvailable() &&  !mp->videoEffect().empty()) {
                top.x += ImGui::GetFrameHeight();
                ImGui::SetCursorPos(top);
                std::string desc = mp->videoEffect();
                desc = desc.substr(0, desc.find_first_of(' '));
                desc = "Has gstreamer effect '" + desc + "'";
                ImGuiToolkit::Indication(desc.c_str(),16,16);
            }

            ImGui::SetCursorPos(botom);

            // Selector for Hardware or software decoding, if available
            if ( Settings::application.render.gpu_decoding ) {

                // Build string information on decoder name with icon HW/SW
                bool hwdec = !mp->softwareDecodingForced();
                std::string decoder = mp->decoderName();
                if (hwdec) {
                    if ( decoder.compare("software") != 0)
                        decoder = ICON_FA_MICROCHIP " Hardware (" + decoder + ")";
                    else
                        decoder = ICON_FA_COGS " Software";
                }
                else
                    decoder = ICON_FA_COGS " Software only";
                // Combo selector shoing the current state and the option to choose
                // between auto select and forced software decoding
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::BeginCombo("Decoding", decoder.c_str() ) )
                {
                    if (ImGui::Selectable( ICON_FA_MICROCHIP " / " ICON_FA_COGS " Auto select", &hwdec ))
                        mp->setSoftwareDecodingForced(false);
                    if (ImGui::IsItemHovered() && !hwdec)
                        ImGuiToolkit::ToolTip( "Changing decoding will\nre-open the media" );
                    hwdec = mp->softwareDecodingForced();
                    if (ImGui::Selectable( ICON_FA_COGS " Software only", &hwdec ))
                        mp->setSoftwareDecodingForced(true);
                    if (ImGui::IsItemHovered() && !hwdec)
                        ImGuiToolkit::ToolTip( "Changing decoding will\nre-open the media" );
                    ImGui::EndCombo();
                }
            }
            else {
                // info of software only decoding if disabled
                ImGuiToolkit::Icon(14,2,false);
                ImGui::SameLine();
                ImGui::TextDisabled("Hardware decoding disabled");
            }

            // enable / disable audio if available
            if (mp->audioAvailable()) {

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::BeginCombo("Audio", mp->audioEnabled() ? ICON_FA_VOLUME_UP " Enabled" : ICON_FA_VOLUME_MUTE " Disabled" ) )
                {
                    if (ImGui::Selectable( ICON_FA_VOLUME_UP " Enable", mp->audioEnabled() ))
                        mp->setAudioEnabled(true);
                    if (ImGui::IsItemHovered() && !mp->audioEnabled())
                        ImGuiToolkit::ToolTip( "Changing audio will\nre-open the media" );

                    if (ImGui::Selectable( ICON_FA_VOLUME_MUTE " Disable", !mp->audioEnabled() ))
                        mp->setAudioEnabled(false);
                    if (ImGui::IsItemHovered() && mp->audioEnabled())
                        ImGuiToolkit::ToolTip( "Changing audio will\nre-open the media" );
                    ImGui::EndCombo();
                }

                if (mp->audioEnabled()) {

                    ImGuiIO& io = ImGui::GetIO();
                    ///
                    /// AUDIO VOLUME
                    ///
                    int vol = mp->audioVolume();
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if ( ImGui::SliderInt("##Volume", &vol, 0, 100, "%d%%") )
                        mp->setAudioVolume(vol);
                    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                        vol = CLAMP(vol + int(10.f * io.MouseWheel), 0, 100);
                        mp->setAudioVolume(vol);
                    }
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Volume")) {
                        mp->setAudioVolume(100);
                    }

                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    int m = mp->audioVolumeMix();
                    if ( ImGui::Combo("##Multiplier", &m, "None\0Alpha\0Opacity\0Alpha * Opacity\0")  ) {
                        mp->setAudioVolumeMix( (MediaPlayer::VolumeFactorsMix) m );
                    }
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Multiplier")) {
                        mp->setAudioVolumeMix( MediaPlayer::VOLUME_ONLY );
                    }
                }
            }
        }
        else
            ImGui::SetCursorPos(botom);
    }
    else {
        // failed
        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy message"))
            ImGui::SetClipboardText(info.str().c_str());
        info.reset();

        ImGui::SetCursorPos(botom);

        // because sometimes the error comes from gpu decoding
        if ( Settings::application.render.gpu_decoding && SystemToolkit::file_exists(s.path()) )
        {
            // offer to reload the source without hardware decoding
            if ( ImGui::Button( ICON_FA_REDO_ALT " Try again without\nhardware decoding", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ) {
                // replace current source with one created with a flag forcing software decoding
                Mixer::manager().replaceSource(Mixer::manager().currentSource(),
                                               Mixer::manager().createSourceFile(s.path(), true));
            }
        }
    }

}

void ImGuiVisitor::visit (SessionFileSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip( s.path().c_str() );

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() && s.session() != nullptr ) {

        if ( s.active() && s.session()->ready() ) {
            // versions
            SessionSnapshots *versions = s.session()->snapshots();
            if (versions->keys_.size()>0) {
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::BeginCombo("Version", ICON_FA_CODE_BRANCH " Select" ) )
                {
                    for (auto v = versions->keys_.crbegin() ; v != versions->keys_.crend(); ++v){
                        std::string label = std::to_string(*v);
                        const tinyxml2::XMLElement *snap = versions->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(*v).c_str() );
                        if (snap)
                            label = snap->Attribute("label");
                        if (ImGui::Selectable( label.c_str() )) {
                            s.session()->applySnapshot(*v);
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            // fading
            std::ostringstream oss;
            int f = 100 - int(s.session()->fading() * 100.f);
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::SliderInt("##Fading", &f, 0, 100, f > 99 ? ICON_FA_ADJUST " None" : ICON_FA_ADJUST " %d %%") )
                s.session()->setFadingTarget( float(100 - f) * 0.01f );
            if (ImGui::IsItemDeactivatedAfterEdit()){
                oss << s.name() << ": Fading " << f << " %";
                Action::manager().store(oss.str());
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton("Fading")) {
                s.session()->setFadingTarget(0.f);
                oss << s.name() << ": Fading 0 %";
                Action::manager().store(oss.str());
            }

            // import
            if ( ImGui::Button( ICON_FA_FILE_EXPORT " Import all", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
                Mixer::manager().import( &s );
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::Text("Sources");

            // file open
            if ( ImGui::Button( ICON_FA_FILE_UPLOAD " Open", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
                Mixer::manager().set( s.detach() );
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::Text("Session");

            botom = ImGui::GetCursorPos();

            // icon (>) to open player
            if ( s.playable() ) {
                ImGui::SetCursorPos(top);
                std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
                if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                    UserInterface::manager().showSourceEditor(&s);
                top.x += ImGui::GetFrameHeight();
            }

        }
        else
        {
            // file open
            if ( ImGui::Button( ICON_FA_ARROW_CIRCLE_RIGHT " Transition", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
            {
                TransitionView *tv = static_cast<TransitionView*>(Mixer::manager().view(View::TRANSITION));
                tv->attach(&s);
                Mixer::manager().setView(View::TRANSITION);
                WorkspaceWindow::clearWorkspace();
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::Text("Session");

            botom = ImGui::GetCursorPos();
        }

        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(3, 5, "Show in finder"))
            SystemToolkit::open(SystemToolkit::path_filename(s.path()));

    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);
}

void ImGuiVisitor::visit (SessionGroupSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    Session *session = s.session();
    if (session == nullptr)
        return;

    // info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // Show list of sources in text bloc (multi line, dark background)
    ImGuiTextBuffer listofsources;
    listofsources.append( BaseToolkit::joinned( session->getNameList(), '\n').c_str() );
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::InputTextMultiline("##sourcesingroup", (char *)listofsources.c_str(), listofsources.size(),
                              ImVec2(IMGUI_RIGHT_ALIGN, CLAMP(session->size(), 2, 5) * ImGui::GetTextLineHeightWithSpacing()),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor(1);

    if ( ImGui::Button( ICON_FA_SIGN_OUT_ALT " Expand", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().import( &s );
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::Text("Sources");

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }
    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);
}

void ImGuiVisitor::visit (RenderSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // loopback provenance
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) s.renderingProvenance();
    if (ImGuiToolkit::ComboIcon("##SelectRender", &m, RenderSource::ProvenanceMethod))
        s.setRenderingProvenance((RenderSource::RenderSourceProvenance)m);

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
            top.x += ImGui::GetFrameHeight();
        }
        // icon to open output view
        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(ICON_FA_DESKTOP, "Open Display"))
            Settings::application.widget.preview = true;
    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);
}

void ImGuiVisitor::visit (FrameBufferFilter&)
{

}

void ImGuiVisitor::visit (PassthroughFilter&)
{

}

void ImGuiVisitor::visit (DelayFilter& f)
{
    ImGuiIO& io = ImGui::GetIO();
    std::ostringstream oss;

//    if (ImGuiToolkit::IconButton(ICON_FILTER_DELAY)) {
//        f.setDelay(0.f);
//        Action::manager().store("Delay 0 s");
//    }
//    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float d = f.delay();
    if (ImGui::SliderFloat("##Delay", &d, 0.f, 2.f, "%.2f s"))
        f.setDelay(d);
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
        d = CLAMP( d + 0.01f * io.MouseWheel, 0.f, 2.f);
        f.setDelay(d);
        oss << "Delay " << std::setprecision(3) << d << " s";
        Action::manager().store(oss.str());
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        oss << "Delay " << std::setprecision(3) << d << " s";
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Delay ")) {
        f.setDelay(0.5f);
        Action::manager().store("Delay 0.5 s");
    }
}

void ImGuiVisitor::visit (ResampleFilter& f)
{
    std::ostringstream oss;

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) f.factor();
    if (ImGui::Combo("##Factor", &m, ResampleFilter::factor_label, IM_ARRAYSIZE(ResampleFilter::factor_label) )) {
        f.setFactor( m );
        oss << "Resample " << ResampleFilter::factor_label[m];
        Action::manager().store(oss.str());
        info.reset();
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Factor")) {
        f.setFactor( 0 );
        oss << "Resample " << ResampleFilter::factor_label[0];
        Action::manager().store(oss.str());
        info.reset();
    }
}

void list_parameters_(ImageFilter &f, std::ostringstream &oss)
{
    ImGuiIO& io = ImGui::GetIO();

    std::map<std::string, float> filter_parameters = f.program().parameters();
    for (auto param = filter_parameters.begin(); param != filter_parameters.end(); ++param)
    {
        ImGui::PushID( param->first.c_str() );
        float v = param->second;
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::SliderFloat( "##ImageFilterParameterEdit", &v, 0.f, 1.f, "%.2f")) {
            f.setProgramParameter(param->first, v);
        }
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
            v = CLAMP( v + 0.01f * io.MouseWheel, 0.f, 1.f);
            f.setProgramParameter(param->first, v);
            oss << " : " << param->first << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            oss << " : " << param->first << " " << std::setprecision(3) <<param->second;
            Action::manager().store(oss.str());
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton( param->first.c_str() )) {
            v = 0.5f;
            f.setProgramParameter(param->first, v);
            oss << " : " << param->first << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
        ImGui::PopID();
    }
}

void ImGuiVisitor::visit (BlurFilter& f)
{
    std::ostringstream oss;
    oss << "Blur ";

    // Method selection
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) f.method();
    if (ImGui::Combo("##MethodBlur", &m, BlurFilter::method_label, IM_ARRAYSIZE(BlurFilter::method_label) )) {
        f.setMethod( m );
        oss << BlurFilter::method_label[m];
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Method ")) {
        f.setMethod( 0 );
        oss << BlurFilter::method_label[0];
        Action::manager().store(oss.str());
    }

    // List of parameters
    list_parameters_(f, oss);
}

void ImGuiVisitor::visit (SharpenFilter& f)
{
    std::ostringstream oss;
    oss << "Sharpen ";

    // Method selection
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) f.method();
    if (ImGui::Combo("##MethodSharpen", &m, SharpenFilter::method_label, IM_ARRAYSIZE(SharpenFilter::method_label) )) {
        f.setMethod( m );
        oss << SharpenFilter::method_label[m];
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Method")) {
        f.setMethod( 0 );
        oss << SharpenFilter::method_label[0];
        Action::manager().store(oss.str());
    }

    // List of parameters
    list_parameters_(f, oss);
}

void ImGuiVisitor::visit (SmoothFilter& f)
{
    std::ostringstream oss;
    oss << "Smooth ";

    // Method selection
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) f.method();
    if (ImGui::Combo("##MethodSmooth", &m, SmoothFilter::method_label, IM_ARRAYSIZE(SmoothFilter::method_label) )) {
        f.setMethod( m );
        oss << SmoothFilter::method_label[m];
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Method")) {
        f.setMethod( 0 );
        oss << SmoothFilter::method_label[0];
        Action::manager().store(oss.str());
    }

    // List of parameters
    list_parameters_(f, oss);
}

void ImGuiVisitor::visit (EdgeFilter& f)
{
    std::ostringstream oss;
    oss << "Edge ";

    // Method selection
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) f.method();
    if (ImGui::Combo("##MethodEdge", &m, EdgeFilter::method_label, IM_ARRAYSIZE(EdgeFilter::method_label) )) {
        f.setMethod( m );
        oss << EdgeFilter::method_label[m];
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Method")) {
        f.setMethod( 0 );
        oss << EdgeFilter::method_label[0];
        Action::manager().store(oss.str());
    }

    // List of parameters
    list_parameters_(f, oss);
}

void ImGuiVisitor::visit (AlphaFilter& f)
{
    ImGuiIO& io = ImGui::GetIO();
    std::ostringstream oss;
    oss << "Alpha ";

    // Alpha operation selection
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int m = (int) f.operation();
    if (ImGui::Combo("##Operation", &m, AlphaFilter::operation_label, IM_ARRAYSIZE(AlphaFilter::operation_label) )) {
        f.setOperation( m );
        oss << AlphaFilter::operation_label[m];
        Action::manager().store(oss.str());
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Operation")) {
        f.setOperation( 0 );
        oss << AlphaFilter::operation_label[0];
        Action::manager().store(oss.str());
    }

    // List of parameters
    std::map<std::string, float> filter_parameters = f.program().parameters();

    if ( m == AlphaFilter::ALPHA_CHROMAKEY || m == AlphaFilter::ALPHA_LUMAKEY)
    {
        float t = filter_parameters["Threshold"];
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::SliderFloat( "##Threshold", &t, 0.f, 1.f, "%.2f")) {
            f.setProgramParameter("Threshold", t);
        }
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
            t = CLAMP( t + 0.01f * io.MouseWheel, 0.f, 1.f);
            f.setProgramParameter("Threshold", t);
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Threshold" << " " << std::setprecision(3) << t;
            Action::manager().store(oss.str());
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Threshold" << " " << std::setprecision(3) << t;
            Action::manager().store(oss.str());
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Threshold")) {
            t = 0.f;
            f.setProgramParameter("Threshold", t);
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Threshold" << " " << std::setprecision(3) << t;
            Action::manager().store(oss.str());
        }

        float v = filter_parameters["Tolerance"];
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::SliderFloat( "##Tolerance", &v, 0.f, 1.f, "%.2f")) {
            f.setProgramParameter("Tolerance", v);
        }
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
            v = CLAMP( v + 0.01f * io.MouseWheel, 0.f, 1.f);
            f.setProgramParameter("Tolerance", v);
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Tolerance" << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Tolerance" << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Tolerance")) {
            v = 0.f;
            f.setProgramParameter("Tolerance", v);
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Tolerance" << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
    }

    if ( m == AlphaFilter::ALPHA_CHROMAKEY || m == AlphaFilter::ALPHA_FILL)
    {        
        static DialogToolkit::ColorPickerDialog colordialog;

        // read color from filter
        float color[3] = {filter_parameters["Red"], filter_parameters["Green"], filter_parameters["Blue"]};

        // show color
        if (ImGui::ColorEdit3("Chromakey Color", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) )
        {
            f.setProgramParameter("Red",   color[0]);
            f.setProgramParameter("Green", color[1]);
            f.setProgramParameter("Blue",  color[2]);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char buf[64];
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", ImClamp((int)ceil(255.f * color[0]),0,255),
                           ImClamp((int)ceil(255.f * color[1]),0,255), ImClamp((int)ceil(255.f * color[2]),0,255));
            oss << " Color " << buf;
            Action::manager().store(oss.str());
        }

        // offer to pick color
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::Button( ICON_FA_EYE_DROPPER " Open selector", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            if ( DialogToolkit::ColorPickerDialog::busy()) {
                Log::Warning("Close previously openned color picker.");
            }
            else {
                colordialog.setRGB( std::make_tuple(color[0], color[1], color[2]) );
                colordialog.open();
            }
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Color")) {
            f.setProgramParameter("Red",   0.f);
            f.setProgramParameter("Green", 1.f);
            f.setProgramParameter("Blue",  0.f);
            oss << " Color #00FF00";
            Action::manager().store(oss.str());
        }

        // get picked color if dialog finished
        if (colordialog.closed()){
            std::tuple<float, float, float> c = colordialog.RGB();
            f.setProgramParameter("Red",   std::get<0>(c));
            f.setProgramParameter("Green", std::get<1>(c));
            f.setProgramParameter("Blue",  std::get<2>(c));
            char buf[64];
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", ImClamp((int)ceil(255.f * std::get<0>(c)),0,255),
                           ImClamp((int)ceil(255.f * std::get<1>(c)),0,255), ImClamp((int)ceil(255.f * std::get<2>(c)),0,255));
            oss << " Color " << buf;
            Action::manager().store(oss.str());
        }
    }
    // Luminance extra parameter
    else {
        float v = filter_parameters["Luminance"];
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::SliderFloat( "##Luminance", &v, 0.f, 1.f, "%.2f")) {
            f.setProgramParameter("Luminance", v);
        }
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
            v = CLAMP( v + 0.01f * io.MouseWheel, 0.f, 1.f);
            f.setProgramParameter("Luminance", v);
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Luminance" << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Luminance" << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Luminance")) {
            v = 0.f;
            f.setProgramParameter("Luminance", v);
            oss << AlphaFilter::operation_label[ f.operation() ];
            oss << " : " << "Luminance" << " " << std::setprecision(3) << v;
            Action::manager().store(oss.str());
        }
    }

}

void ImGuiVisitor::visit (ImageFilter& f)
{
    // Open Editor
    if ( ImGui::Button( ICON_FA_CODE "  Open editor", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        UserInterface::manager().shadercontrol.setVisible(true);
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Code")) {
        FilteringProgram target;
        f.setProgram( target );
    }
}

void ImGuiVisitor::visit (CloneSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if ( !s.failed() ) {

        // link to origin source
        std::string label = std::string(s.origin()->initials()) + " - " + s.origin()->name();
        if (ImGui::Button(label.c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) )) {
            Mixer::manager().setCurrentSource(s.origin());

            if (UserInterface::manager().navigator.pannelVisible())
                UserInterface::manager().navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Origin");

        // filter selection
        std::ostringstream oss;
        oss << s.name();
        int type = (int) s.filter()->type();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::ComboIcon("##SelectFilter", &type, FrameBufferFilter::Types)) {
            s.setFilter( FrameBufferFilter::Type(type) );
            oss << ": Filter " << std::get<2>(FrameBufferFilter::Types[type]);
            Action::manager().store(oss.str());
            info.reset();
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Filter")) {
            s.setFilter( FrameBufferFilter::FILTER_PASSTHROUGH );
            oss << ": Filter None";
            Action::manager().store(oss.str());
            info.reset();
        }

        // filter options
        s.filter()->accept(*this);

        ImVec2 botom = ImGui::GetCursorPos();

        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }

        ImGui::SetCursorPos(botom);
    }
    else {
        ImGuiToolkit::ButtonDisabled("No source", ImVec2(IMGUI_RIGHT_ALIGN, 0) );
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Origin");
        info.reset();
    }
}

void ImGuiVisitor::visit (PatternSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // stream info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##Patterns", Pattern::get(s.pattern()->type()).label.c_str(), ImGuiComboFlags_HeightLarge) )
    {
        for (uint p = 0; p < Pattern::count(); ++p){
            pattern_descriptor pattern = Pattern::get(p);
            std::string label = pattern.label + (pattern.animated ? "  " ICON_FA_PLAY_CIRCLE : " ");
            if (pattern.available && ImGui::Selectable( label.c_str(), p == s.pattern()->type() )) {
                s.setPattern(p, s.pattern()->resolution());
                info.reset();
                std::ostringstream oss;
                oss << s.name() << ": Pattern " << Pattern::get(p).label;
                Action::manager().store(oss.str());
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::Text("Generator");

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
            top.x += ImGui::GetFrameHeight();
        }
        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy"))
            ImGui::SetClipboardText(Pattern::get( s.pattern()->type() ).pipeline.c_str());
    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);
}

void ImGuiVisitor::visit (DeviceSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if ( !s.failed() ) {

        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("Device", s.device().c_str()))
        {
            for (int d = 0; d < Device::manager().numDevices(); ++d){
                std::string namedev = Device::manager().name(d);
                if (ImGui::Selectable( namedev.c_str() )) {
                    if ( namedev.compare(s.device())==0 )
                        s.reconnect();
                    else {
                        s.setDevice(namedev);
                        info.reset();
                        std::ostringstream oss;
                        oss << s.name() << " Device " << namedev;
                        Action::manager().store(oss.str());
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImVec2 botom = ImGui::GetCursorPos();

        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
            top.x += ImGui::GetFrameHeight();
        }

        // icon to show gstreamer properties
        ImGui::SetCursorPos(top);
        ImGuiToolkit::Icon(16, 16, false);
        if (ImGui::IsItemHovered()) {
            int index = Device::manager().index( s.device() );
            std::string prop = Device::manager().properties( index );
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 14.0f);
            ImGui::TextUnformatted( prop.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::SetCursorPos(botom);
    }
    else {

        info.reset();

    }

}


void ImGuiVisitor::visit (ScreenCaptureSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if ( !s.failed() ) {

        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("Window", s.window().c_str()))
        {
            for (int d = 0; d < ScreenCapture::manager().numWindow(); ++d){
                std::string namedev = ScreenCapture::manager().name(d);
                if (ImGui::Selectable( namedev.c_str() )) {
                    if ( namedev.compare(s.window())==0 )
                        s.reconnect();
                    else {
                        s.setWindow(namedev);
                        info.reset();
                        std::ostringstream oss;
                        oss << s.name() << " Window " << namedev;
                        Action::manager().store(oss.str());
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImVec2 botom = ImGui::GetCursorPos();

        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }

        ImGui::SetCursorPos(botom);
    }
    else {

        info.reset();

    }

}


void ImGuiVisitor::visit (NetworkSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
    ImGui::Text("%s", s.connection().c_str());
    ImGui::PopStyleColor(1);

    // network info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }
    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);
}


void ImGuiVisitor::visit (MultiFileSource& s)
{
    static uint64_t id = 0;

    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // information text
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // Filename pattern
        ImGuiTextBuffer info;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        info.appendf("%s", SystemToolkit::base_filename(s.sequence().location).c_str());
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::InputText("Filenames", (char *)info.c_str(), info.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(1);

        // change range
        static int _begin = -1;
        if (_begin < 0 || id != s.id())
            _begin = s.begin();
        static int _end = -1;
        if (_end < 0 || id != s.id())
            _end = s.end();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::DragIntRange2("##Range", &_begin, &_end, 1, s.sequence().min, s.sequence().max);
        if (ImGui::IsItemDeactivatedAfterEdit()){
            s.setRange( _begin, _end );
            std::ostringstream oss;
            oss << s.name() << ": Range " << _begin << "-" << _end;
            Action::manager().store(oss.str());
            _begin = _end = -1;
        }
        ImGui::SameLine(0, 0);
        if (ImGuiToolkit::TextButton("Range")) {
            s.setRange( s.sequence().min, s.sequence().max );
            std::ostringstream oss;
            oss << s.name() << ": Range " << s.sequence().min << "-" << s.sequence().max;
            Action::manager().store(oss.str());
            _begin = _end = -1;
        }

        // change framerate
        static int _fps = -1;
        if (_fps < 0 || id != s.id())
            _fps = s.framerate();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderInt("##Framerate", &_fps, 1, 30, "%d fps");
        if (ImGui::IsItemDeactivatedAfterEdit()){
            s.setFramerate(_fps);
            std::ostringstream oss;
            oss << s.name() << ": Framerate " << _fps << " fps";
            Action::manager().store(oss.str());
            _fps = -1;
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Framerate")) {
            s.setFramerate(25);
            std::ostringstream oss;
            oss << s.name() << ": Framerate 25 fps";
            Action::manager().store(oss.str());
            _fps = -1;
        }

        botom = ImGui::GetCursorPos();

        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
            top.x += ImGui::GetFrameHeight();
        }

    }
    else
        info.reset();

    // offer to open file browser at location
    ImGui::SetCursorPos(top);
    if (ImGuiToolkit::IconButton(3, 5, "Show in finder"))
        SystemToolkit::open(SystemToolkit::path_filename( s.sequence().location ));

    ImGui::SetCursorPos(botom);

    if (id != s.id())
        id = s.id();
}

void ImGuiVisitor::visit (GenericStreamSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    float w = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // stream info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + w);
    s.accept(info);
    if ( !s.failed() )
        ImGui::Text("%s", info.str().c_str());
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 0.9f));
        ImGui::Text("%s", info.str().c_str());
        ImGui::PopStyleColor(1);
    }
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // Prepare display pipeline text
    static int numlines = 0;
    const ImGuiContext& g = *GImGui;
    ImVec2 fieldsize(w,  MAX(3, numlines) * g.FontSize + g.Style.ItemSpacing.y + g.Style.FramePadding.y);

    // Editor
    std::string _description = s.description();
    if ( ImGuiToolkit::InputCodeMultiline("Pipeline", &_description, fieldsize, &numlines) ) {
        info.reset();
        s.setDescription(_description);
        Action::manager().store( s.name() + ": Change pipeline");
    }
    ImVec2 botom = ImGui::GetCursorPos();

    // Actions on the pipeline
    ImGui::SetCursorPos( ImVec2(top.x, botom.y - ImGui::GetFrameHeight()));
    if (ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy"))
        ImGui::SetClipboardText(_description.c_str());
    ImGui::SetCursorPos( ImVec2(top.x + 0.9 * ImGui::GetFrameHeight(), botom.y - ImGui::GetFrameHeight()));
    if (ImGuiToolkit::IconButton(ICON_FA_PASTE, "Paste")) {
        _description = std::string ( ImGui::GetClipboardText() );
        info.reset();
        s.setDescription(_description);
        Action::manager().store( s.name() + ": Change pipeline");
    }

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.playable() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }
    }

    ImGui::SetCursorPos(botom);
}


void ImGuiVisitor::visit (SrtReceiverSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // network info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.stream()->isOpen() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }
        else
            info.reset();
    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);

}


void ImGuiVisitor::visit(TextSource &s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
    float w = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // network info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x
                           + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImVec2 botom = ImGui::GetCursorPos();

    TextContents *tc = s.contents();
    if (tc) {
        // Prepare display text
        static int numlines = 0;
        const ImGuiContext &g = *GImGui;
        ImVec2 fieldsize(w, MAX(4, numlines) * g.FontSize + g.Style.ItemSpacing.y + g.Style.FramePadding.y);

        // Text contents
        std::string _contents = tc->text();
        // if the content is a subtitle file
        if (tc->isSubtitle()) {
            static char dummy_str[1024];
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            snprintf(dummy_str, 1024, "%s", _contents.c_str());
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
            ImGui::InputText("##Filesubtitle", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(1);
            botom = ImGui::GetCursorPos();
            // Action on the filename
            ImGui::SetCursorPos(ImVec2(top.x, botom.y - ImGui::GetFrameHeight()));
            if (ImGuiToolkit::IconButton(ICON_FA_EDIT, "Open in editor"))
                SystemToolkit::open(_contents.c_str());
            ImGui::SetCursorPos(
                ImVec2(top.x + 0.95 * ImGui::GetFrameHeight(), botom.y - ImGui::GetFrameHeight()));
            if (ImGuiToolkit::IconButton(ICON_FA_REDO_ALT, "Reload"))
                s.reload();
        }
        // general case of free text : text editor
        else {
            if (ImGuiToolkit::InputTextMultiline("Text", &_contents, fieldsize, &numlines)) {
                info.reset();
                tc->setText(_contents);
                Action::manager().store(s.name() + " Change text");
            }
            botom = ImGui::GetCursorPos();

            // Actions on the text
            ImGui::SetCursorPos(ImVec2(top.x, botom.y - ImGui::GetFrameHeight()));
            if (ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy"))
                ImGui::SetClipboardText(_contents.c_str());
            ImGui::SetCursorPos(
                ImVec2(top.x + 0.95 * ImGui::GetFrameHeight(), botom.y - ImGui::GetFrameHeight()));
            if (ImGuiToolkit::IconButton(ICON_FA_PASTE, "Paste")) {
                _contents = std::string(ImGui::GetClipboardText());
                info.reset();
                tc->setText(_contents);
                Action::manager().store(s.name() + " Change text");
            }
            ImGui::SetCursorPos(ImVec2(top.x, botom.y - 2.f * ImGui::GetFrameHeight()));
            if (ImGuiToolkit::IconButton(ICON_FA_CODE, "Pango syntax"))
                SystemToolkit::open("https://docs.gtk.org/Pango/pango_markup.html");
        }

        // Text Properties
        ImGui::SetCursorPos(botom);

        ImVec4 color = ImGuiToolkit::ColorConvertARGBToFloat4(tc->color());
        if (ImGui::ColorEdit4("Text Color", (float *) &color, ImGuiColorEditFlags_AlphaPreview |
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) ) {
            tc->setColor( ImGuiToolkit::ColorConvertFloat4ToARGB(color) );
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            Action::manager().store( s.name() + " Change font color" );
        std::string font = tc->fontDescriptor();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::InputText("##Font", &font, ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            tc->setFontDescriptor(font);
            Action::manager().store( s.name() + " Change font");
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Font")) {
            tc->setFontDescriptor("arial 24");
            tc->setColor(0xffffffff);
            Action::manager().store(s.name() + " Reset font");
        }
        botom = ImGui::GetCursorPos();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::IconButton(1, 13, "Pango font description"))
            SystemToolkit::open("https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html");
        ImGui::SetCursorPos(botom);

        color = ImGuiToolkit::ColorConvertARGBToFloat4(tc->outlineColor());
        if (ImGui::ColorEdit4("Text Outline Color", (float *) &color, ImGuiColorEditFlags_AlphaPreview |
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) ) {
            tc->setOutlineColor( ImGuiToolkit::ColorConvertFloat4ToARGB(color) );
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            Action::manager().store( s.name() + " Change outline color" );
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        int outline = tc->outline();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::Combo("##Outline", &outline, "None\0Border\0Border & shadow\0")) {
            tc->setOutline(outline);
            Action::manager().store(s.name() + " Change outline");
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Outline")) {
            tc->setOutline(2);
            tc->setOutlineColor(0xFF000000);
            Action::manager().store(s.name() + " Reset outline");
        }

        // HORIZONTAL alignment
        bool on = true;
        uint var = tc->horizontalAlignment();
        on = var == 0;
        float _x = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
        _x -= 4.f * (g.FontSize + g.Style.FramePadding.x + IMGUI_SAME_LINE);
        if (var != 1) {
            // not centered
            ImGui::SetNextItemWidth(_x);
            if (var > 2) {
                // Absolute position
                float align_x = tc->horizontalPadding();
                if (ImGui::SliderFloat("##Posx", &align_x, 0.f, 1.f, "%.2f")) {
                    tc->setHorizontalPadding(align_x);
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip( "Coordinates" );
            } else {
                // padding top or bottom
                int pad_x = (int) tc->horizontalPadding();
                if (ImGui::SliderInt("##Padx", &pad_x, 0, 1000)) {
                    tc->setHorizontalPadding((float) pad_x);
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip( "Padding" );
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
        } else {
            _x += IMGUI_SAME_LINE + g.Style.FramePadding.x;
            ImGui::SetCursorPosX(_x);
        }
        if (ImGuiToolkit::ButtonIconToggle(17, 16, &on, "Left"))
            tc->setHorizontalAlignment(0);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        on = var == 1;
        if (ImGuiToolkit::ButtonIconToggle(18, 16, &on, "Center"))
            tc->setHorizontalAlignment(1);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        on = var == 2;
        if (ImGuiToolkit::ButtonIconToggle(19, 16, &on, "Right"))
            tc->setHorizontalAlignment(2);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        on = var == 3;
        if (ImGuiToolkit::ButtonIconToggle(6, 10, &on, "Absolute"))
            tc->setHorizontalAlignment(3);
        if (var != tc->horizontalAlignment())
            Action::manager().store(s.name() + " Change h-align");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Horizontal")) {
            tc->setHorizontalAlignment(1);
            Action::manager().store(s.name() + " Reset h-align");
        }

        // VERTICAL alignment
        var = tc->verticalAlignment();
        on = var == 0;
        _x = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
        _x -= 4.f * (g.FontSize + g.Style.FramePadding.x + IMGUI_SAME_LINE);
        if (var != 2) {
            // not centered
            ImGui::SetNextItemWidth(_x);
            if (var > 2) {
                // Absolute position
                float align_y = tc->verticalPadding();
                if (ImGui::SliderFloat("##Posy", &align_y, 0.f, 1.f, "%.2f")) {
                    tc->setVerticalPadding(align_y);
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip( "Coordinates" );
            } else {
                // padding top or bottom
                int pad_y = (int) tc->verticalPadding();
                if (ImGui::SliderInt("##Pady", &pad_y, 0, 1000)) {
                    tc->setVerticalPadding((float) pad_y);
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip( "Padding" );
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
        } else {
            // no value slider for centered
            _x += IMGUI_SAME_LINE + g.Style.FramePadding.x;
             ImGui::SetCursorPosX(_x);
        }
        if (ImGuiToolkit::ButtonIconToggle(1, 17, &on, "Bottom"))
            tc->setVerticalAlignment(0);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        on = var == 2;
        if (ImGuiToolkit::ButtonIconToggle(3, 17, &on, "Center"))
            tc->setVerticalAlignment(2);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        on = var == 1;
        if (ImGuiToolkit::ButtonIconToggle(2, 17, &on, "Top"))
            tc->setVerticalAlignment(1);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        on = var == 3;
        if (ImGuiToolkit::ButtonIconToggle(3, 10, &on, "Absolute"))
            tc->setVerticalAlignment(3);
        if (var != tc->verticalAlignment())
            Action::manager().store(s.name() + " Change v-align");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Vertical")) {
            tc->setVerticalAlignment(2);
            Action::manager().store(s.name() + " Reset v-align");
        }

        botom = ImGui::GetCursorPos();
    }

    if ( !s.failed() ) {
        // icon (>) to open player
        if ( s.stream()->isOpen() ) {
            ImGui::SetCursorPos(top);
            std::string msg = s.playing() ? "Open Player\n(source is playing)" : "Open Player\n(source is paused)";
            if (ImGuiToolkit::IconButton( s.playing() ? ICON_FA_PLAY_CIRCLE : ICON_FA_PAUSE_CIRCLE, msg.c_str()))
                UserInterface::manager().showSourceEditor(&s);
        }
        else
            info.reset();
    }
    else
        info.reset();

    ImGui::SetCursorPos(botom);

}
