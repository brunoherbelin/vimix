#include "ImGuiVisitor.h"

#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "defines.h"
#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "Settings.h"
#include "Mixer.h"

#include "imgui.h"
#include "ImGuiToolkit.h"
#include "UserInterfaceManager.h"
#include "SystemToolkit.h"


ImGuiVisitor::ImGuiVisitor()
{

}

void ImGuiVisitor::visit(Node &n)
{

}

void ImGuiVisitor::visit(Group &n)
{
//    std::string id = std::to_string(n.id());
//    if (ImGui::TreeNode(id.c_str(), "Group %d", n.id()))
//    {
        // MODEL VIEW

    ImGui::PushID(n.id());

    if (ImGuiToolkit::ButtonIcon(1, 16)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        n.rotation_.z = 0.f;
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
    }
    ImGui::SameLine(0, 10);
    ImGui::Text("Geometry");

    if (ImGuiToolkit::ButtonIcon(6, 15)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
    }
    ImGui::SameLine(0, 10);
    float translation[2] = { n.translation_.x, n.translation_.y};
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("Position", translation, -5.0, 5.0) )
    {
        n.translation_.x = translation[0];
        n.translation_.y = translation[1];
    }

    if (ImGuiToolkit::ButtonIcon(18, 9))
        n.rotation_.z = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderAngle("Angle", &(n.rotation_.z), -180.f, 180.f) ;

    if (ImGuiToolkit::ButtonIcon(3, 15))  {
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
    }
    ImGui::SameLine(0, 10);
    float scale[2] = { n.scale_.x, n.scale_.y} ;
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("Scale", scale, -MAX_SCALE, MAX_SCALE, "%.2f") )
    {
        n.scale_.x = CLAMP_SCALE(scale[0]);
        n.scale_.y = CLAMP_SCALE(scale[1]);
    }

    ImGui::PopID();

//        // loop over members of a group
//        for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
//            (*node)->accept(*this);
//        }

//        ImGui::TreePop();
//    }

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
    ImGui::PushID(n.id());
    ImGui::Text("Primitive %d", n.id());

    n.shader()->accept(*this);

    ImGui::PopID();
}

void ImGuiVisitor::visit(FrameBufferSurface &n)
{
    ImGui::Text("Framebuffer");
}

void ImGuiVisitor::visit(MediaSurface &n)
{
    ImGui::Text("%s", n.path().c_str());

    if (n.mediaPlayer())
        n.mediaPlayer()->accept(*this);
}

void ImGuiVisitor::visit(MediaPlayer &n)
{
    ImGui::Text("Media Player");
}

void ImGuiVisitor::visit(Shader &n)
{
    ImGui::PushID(n.id());

//    if (ImGuiToolkit::ButtonIcon(10, 2)) {
//        n.blending = Shader::BLEND_OPACITY;
//        n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
//    }
//    ImGui::SameLine(0, 10);
//    ImGui::ColorEdit3("Color", glm::value_ptr(n.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel ) ;
//    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int mode = n.blending;
    if (ImGui::Combo("Blending", &mode, "Normal\0Screen\0Inverse\0Addition\0Subtract\0") )
        n.blending = Shader::BlendMode(mode);

    ImGui::PopID();
}

void ImGuiVisitor::visit(ImageShader &n)
{
    ImGui::PushID(n.id());

    // get index of the mask used in this ImageShader
    int item_current = n.mask;

//    if (ImGuiToolkit::ButtonIcon(10, 3)) n.mask = 0;
//    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    // combo list of masks
    if ( ImGui::Combo("Mask", &item_current, ImageShader::mask_names, IM_ARRAYSIZE(ImageShader::mask_names) ) )
    {
        if (item_current < (int) ImageShader::mask_presets.size())
            n.mask = item_current;
        else {
            // TODO ask for custom mask
        }
    }

    ImGui::PopID();
}

void ImGuiVisitor::visit(ImageProcessingShader &n)
{
    ImGui::PushID(n.id());

    if (ImGuiToolkit::ButtonIcon(6, 2)) {
        ImageProcessingShader defaultvalues;
        n = defaultvalues;
    }
    ImGui::SameLine(0, 10);
    ImGui::Text("Filters");

    if (ImGuiToolkit::ButtonIcon(6, 4)) n.gamma = glm::vec4(1.f, 1.f, 1.f, 1.f);
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("GammaColor", glm::value_ptr(n.gamma), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) ;
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Gamma", &n.gamma.w, 0.5f, 10.f, "%.2f", 2.f);

//    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//    ImGui::SliderFloat4("Levels", glm::value_ptr(n.levels), 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(4, 1)) {
        n.brightness = 0.f;
        n.contrast = 0.f;
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float bc[2] = { n.brightness, n.contrast};
    if ( ImGui::SliderFloat2("B & C", bc, -1.0, 1.0) )
    {
        n.brightness = bc[0];
        n.contrast = bc[1];
    }

    if (ImGuiToolkit::ButtonIcon(2, 1)) n.saturation = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Saturation", &n.saturation, -1.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(12, 4)) n.hueshift = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Hue shift", &n.hueshift, 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(18, 1)) n.nbColors = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderInt("Posterize", &n.nbColors, 0, 16, n.nbColors == 0 ? "None" : "%d colors");

    if (ImGuiToolkit::ButtonIcon(8, 1)) n.threshold = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Threshold", &n.threshold, 0.0, 1.0, n.threshold < 0.001 ? "None" : "%.2f");

    if (ImGuiToolkit::ButtonIcon(3, 1)) n.lumakey = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Lumakey", &n.lumakey, 0.0, 1.0);
    if (ImGuiToolkit::ButtonIcon(13, 4)) {
        n.chromakey = glm::vec4(0.f, 0.8f, 0.f, 1.f);
        n.chromadelta = 0.f;
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Chroma color", glm::value_ptr(n.chromakey), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel  ) ;
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Chromakey", &n.chromadelta, 0.0, 1.0, n.chromadelta < 0.001 ? "None" : "Tolerance %.2f");

    if (ImGuiToolkit::ButtonIcon(7, 1)) n.invert = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("Invert", &n.invert, "None\0Invert Color\0Invert Luminance\0");

    if (ImGuiToolkit::ButtonIcon(1, 7)) n.filterid = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("Filter", &n.filterid, ImageProcessingShader::filter_names, IM_ARRAYSIZE(ImageProcessingShader::filter_names) );

    ImGui::PopID();

    ImGui::Spacing();
}


void ImGuiVisitor::visit (Source& s)
{
    ImGui::PushID(s.id());
    // blending
    s.blendingShader()->accept(*this);

    // preview
    float preview_width = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
    ImVec2 imagesize ( preview_width, preview_width / s.frame()->aspectRatio());
    ImGui::Image((void*)(uintptr_t) s.frame()->texture(), imagesize);

    ImVec2 pos = ImGui::GetCursorPos(); // remember where we were...

    // toggle enable/disable image processing
    bool on = s.imageProcessingEnabled();
    ImGui::SetCursorPos( ImVec2(preview_width + 15, pos.y -ImGui::GetFrameHeight() ) );
    ImGuiToolkit::ButtonToggle(ICON_FA_MAGIC, &on);
    s.setImageProcessingEnabled(on);

    ImGui::SetCursorPos(pos); // ...come back

    // image processing pannel
    if (s.imageProcessingEnabled())
        s.processingShader()->accept(*this);

    // geometry direct control
    s.groupNode(View::GEOMETRY)->accept(*this);

    ImGui::PopID();
}

void ImGuiVisitor::visit (MediaSource& s)
{
    if ( s.mediaplayer()->isImage() ) {
        ImGuiToolkit::Icon(2,9);
        ImGui::SameLine(0, 10);
        ImGui::Text("Image File");
    }
    else {
        ImGuiToolkit::Icon(18,13);
        ImGui::SameLine(0, 10);
        ImGui::Text("Video File");
        if ( ImGui::Button(IMGUI_TITLE_MEDIAPLAYER, ImVec2(IMGUI_RIGHT_ALIGN, 0)) ) {
            UserInterface::manager().showMediaPlayer( s.mediaplayer());
        }
    }
    ImGuiToolkit::ButtonOpenUrl( SystemToolkit::path_filename(s.path()).c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
}

void ImGuiVisitor::visit (SessionSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Session File");

    if (ImGuiToolkit::ButtonIcon(3, 2)) s.session()->setFading(0.f);
    float f = s.session()->fading();
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::SliderFloat("Fading", &f, 0.0, 1.0, f < 0.001 ? "None" : "%.2f") )
        s.session()->setFading(f);

    if ( ImGui::Button( ICON_FA_FILE_UPLOAD " Make Current", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().set( s.detach() );
    if ( ImGui::Button( ICON_FA_FILE_EXPORT " Import", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().merge( s.detach() );

    ImGuiToolkit::ButtonOpenUrl( SystemToolkit::path_filename(s.path()).c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
}

void ImGuiVisitor::visit (RenderSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Rendering Output");
    if ( ImGui::Button(IMGUI_TITLE_PREVIEW, ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Settings::application.widget.preview = true;
}

void ImGuiVisitor::visit (CloneSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Clone");
    if ( ImGui::Button(s.origin()->name().c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().setCurrentSource(s.origin());
}

void ImGuiVisitor::visit (PatternSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Pattern");

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##Patterns", Pattern::pattern_types[s.pattern()->type()].c_str()) )
    {
        for (int p = 0; p < Pattern::pattern_types.size(); ++p){
            if (ImGui::Selectable( Pattern::pattern_types[p].c_str() )) {
                s.setPattern(p, s.pattern()->resolution());
            }
        }
        ImGui::EndCombo();
    }
}

void ImGuiVisitor::visit (DeviceSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Device");

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##Hardware", s.device().c_str()))
    {
        for (int d = 0; d < Device::manager().numDevices(); ++d){
            std::string namedev = Device::manager().name(d);
            if (ImGui::Selectable( namedev.c_str() )) {
                s.setDevice(namedev);
            }
        }
        ImGui::EndCombo();
    }
    DeviceConfigSet confs = Device::manager().config( Device::manager().index(s.device().c_str()));
    if ( !confs.empty()) {
        DeviceConfig best = *confs.rbegin();
        float fps = static_cast<float>(best.fps_numerator) / static_cast<float>(best.fps_denominator);
        ImGui::Text("%s %s %dx%d@%.1ffps", best.stream.c_str(), best.format.c_str(), best.width, best.height, fps);
    }
}

