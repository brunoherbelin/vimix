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
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "MultiFileSource.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "Settings.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "ActionManager.h"

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

//void ImGuiVisitor::visit(ImageShader &n)
//{
//    ImGui::PushID(std::to_string(n.id()).c_str());
//    // get index of the mask used in this ImageShader
//    int item_current = n.mask;
////    if (ImGuiToolkit::ButtonIcon(10, 3)) n.mask = 0;
////    ImGui::SameLine(0, IMGUI_SAME_LINE);
//    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//    // combo list of masks
//    if ( ImGui::Combo("Mask", &item_current, ImageShader::mask_names, IM_ARRAYSIZE(ImageShader::mask_names) ) )
//    {
//        if (item_current < (int) ImageShader::mask_presets.size())
//            n.mask = item_current;
//        else {
//            // TODO ask for custom mask
//        }
//        Action::manager().store("Mask "+ std::string(ImageShader::mask_names[n.mask]));
//    }
//    ImGui::PopID();
//}

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
        float height = s.frame()->projectionArea().y * width / ( s.frame()->projectionArea().x * s.frame()->aspectRatio());
        if (height > preview_height - space) {
            height = preview_height - space;
            width = height * s.frame()->aspectRatio() * ( s.frame()->projectionArea().x / s.frame()->projectionArea().y);
        }
        // centered image
        ImGui::SetCursorPos( ImVec2(pos.x + 0.5f * (preview_width-width), pos.y + 0.5f * (preview_height-height-space)) );
        ImGui::Image((void*)(uintptr_t) s.frame()->texture(), ImVec2(width, height));

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

        // Inform on workspace
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + ImGui::GetFrameHeightWithSpacing()) );
        if (s.workspace() == Source::BACKGROUND)
            ImGuiToolkit::Indication("in Background",10, 16);
        else if (s.workspace() == Source::FOREGROUND)
            ImGuiToolkit::Indication("in Foreground",12, 16);
        else
            ImGuiToolkit::Indication("in Workspace",11, 16);

        // Inform on link
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + 2.1f * ImGui::GetFrameHeightWithSpacing()) );
        if (s.mixingGroup() != nullptr) {
            if (ImGuiToolkit::IconButton(ICON_FA_LINK, "Linked")){
                Mixer::selection().clear();
                Mixer::selection().add( s.mixingGroup()->getCopy() );
            }
        }
        else
            ImGuiToolkit::Indication("not Linked", ICON_FA_UNLINK);

        // locking
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + 3.f * ImGui::GetFrameHeightWithSpacing()) );
        const char *tooltip[2] = {"Unlocked", "Locked"};
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
        float height = s.frame()->projectionArea().y * width / ( s.frame()->projectionArea().x * s.frame()->aspectRatio());
        if (height > preview_height - space) {
            height = preview_height - space;
            width = height * s.frame()->aspectRatio() * ( s.frame()->projectionArea().x / s.frame()->projectionArea().y);
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
        if (ImGuiToolkit::IconButton(ICON_FA_FOLDER_OPEN, "Show in finder"))
            SystemToolkit::open(SystemToolkit::path_filename(s.path()));

    }
    else {
        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy message"))
            ImGui::SetClipboardText(info.str().c_str());
        info.reset();
    }

    ImGui::SetCursorPos(botom);
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

    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip( s.path().c_str() );

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() && s.session() != nullptr && s.session()->ready()) {
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
        if (ImGui::SliderInt("##Fading", &f, 0, 100, f > 99 ? "None" : "%d %%") )
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

        ImGui::SetCursorPos(top);
        if (ImGuiToolkit::IconButton(ICON_FA_FOLDER_OPEN, "Show in finder"))
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
        if (ImGuiToolkit::IconButton(ICON_FA_LAPTOP, "Show Output"))
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

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##Patterns", Pattern::get(s.pattern()->type()).label.c_str(), ImGuiComboFlags_HeightLarge) )
    {
        for (uint p = 0; p < Pattern::count(); ++p){
            pattern_descriptor pattern = Pattern::get(p);
            std::string label = pattern.label + (pattern.animated ? " " ICON_FA_CARET_RIGHT : " ");
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
        }
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


    if ( !s.failed() ) {

        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("Device", s.device().c_str()))
        {
            for (int d = 0; d < Device::manager().numDevices(); ++d){
                std::string namedev = Device::manager().name(d);
                if (ImGui::Selectable( namedev.c_str() )) {
                    s.setDevice(namedev);
                    info.reset();
                    std::ostringstream oss;
                    oss << s.name() << " Device " << namedev;
                    Action::manager().store(oss.str());
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
    else
        info.reset();

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
    if (ImGuiToolkit::IconButton(ICON_FA_FOLDER_OPEN, "Show in finder"))
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
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    ImVec2 botom = ImGui::GetCursorPos();

    if ( !s.failed() ) {

        // Prepare display pipeline text
        static int numlines = 0;
        const ImGuiContext& g = *GImGui;
        ImVec2 fieldsize(w,  MAX(3, numlines) * g.FontSize + g.Style.ItemSpacing.y + g.Style.FramePadding.y);

        // Editor
        std::string _description = s.description();
        if ( ImGuiToolkit::InputCodeMultiline("Pipeline", &_description, fieldsize, &numlines) ) {
            s.setDescription(_description);
            Action::manager().store( s.name() + ": Change pipeline");
        }

        botom = ImGui::GetCursorPos();

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


void ImGuiVisitor::visit (SrtReceiverSource& s)
{
    ImVec2 top = ImGui::GetCursorPos();
    top.x = 0.5f * ImGui::GetFrameHeight() + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;

    // network info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

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

//    if ( ImGui::Button( ICON_FA_REPLY " Reconnect", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
//    {
//        s.setConnection(s.connection());
//        info.reset();
//    }
}
