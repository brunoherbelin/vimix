#include "ImGuiVisitor.h"

#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>

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
#include "NetworkSource.h"
#include "Settings.h"
#include "Mixer.h"
#include "ActionManager.h"

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
    // MODEL VIEW
    ImGui::PushID(std::to_string(n.id()).c_str());

    if (ImGuiToolkit::ButtonIcon(1, 16)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        n.rotation_.z = 0.f;
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
        Action::manager().store("Geometry Reset", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::Text("Geometry");

    if (ImGuiToolkit::ButtonIcon(6, 15)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        Action::manager().store("Position 0.0, 0.0", n.id());
    }
    ImGui::SameLine(0, 10);
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
        Action::manager().store(oss.str(), n.id());
    }
    if (ImGuiToolkit::ButtonIcon(3, 15))  {
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
        Action::manager().store("Scale 1.0 x 1.0", n.id());
    }
    ImGui::SameLine(0, 10);
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
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(18, 9)){
        n.rotation_.z = 0.f;
        Action::manager().store("Angle 0.0", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderAngle("Angle", &(n.rotation_.z), -180.f, 180.f) ;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::ostringstream oss;
        oss << "Angle " << std::setprecision(3) << n.rotation_.z * 180.f / M_PI;
        Action::manager().store(oss.str(), n.id());
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
    ImGui::PushID(std::to_string(n.id()).c_str());

    // Base color
//    if (ImGuiToolkit::ButtonIcon(10, 2)) {
//        n.blending = Shader::BLEND_OPACITY;
//        n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
//    }
//    ImGui::SameLine(0, 10);
//    ImGui::ColorEdit3("Color", glm::value_ptr(n.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel ) ;
//    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int mode = n.blending;
    if (ImGui::Combo("Blending", &mode, "Normal\0Screen\0Inverse\0Addition\0Subtract\0") ) {
        n.blending = Shader::BlendMode(mode);

        std::ostringstream oss;
        oss << "Blending ";
        switch(n.blending) {
        case Shader::BLEND_OPACITY:
            oss<<"Normal";
            break;
        case Shader::BLEND_ADD:
            oss<<"Screen";
            break;
        case Shader::BLEND_SUBSTRACT:
            oss<<"Inverse";
            break;
        case Shader::BLEND_LAYER_ADD:
            oss<<"Addition";
            break;
        case Shader::BLEND_LAYER_SUBSTRACT:
            oss<<"Subtract";
            break;
        case Shader::BLEND_CUSTOM:
            oss<<"Custom";
            break;
        }
        Action::manager().store(oss.str(), n.id());
    }

    ImGui::PopID();
}

//void ImGuiVisitor::visit(ImageShader &n)
//{
//    ImGui::PushID(std::to_string(n.id()).c_str());
//    // get index of the mask used in this ImageShader
//    int item_current = n.mask;
////    if (ImGuiToolkit::ButtonIcon(10, 3)) n.mask = 0;
////    ImGui::SameLine(0, 10);
//    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//    // combo list of masks
//    if ( ImGui::Combo("Mask", &item_current, ImageShader::mask_names, IM_ARRAYSIZE(ImageShader::mask_names) ) )
//    {
//        if (item_current < (int) ImageShader::mask_presets.size())
//            n.mask = item_current;
//        else {
//            // TODO ask for custom mask
//        }
//        Action::manager().store("Mask "+ std::string(ImageShader::mask_names[n.mask]), n.id());
//    }
//    ImGui::PopID();
//}

void ImGuiVisitor::visit(ImageProcessingShader &n)
{
    ImGui::PushID(std::to_string(n.id()).c_str());

    if (ImGuiToolkit::ButtonIcon(6, 2)) {
        ImageProcessingShader defaultvalues;
        n = defaultvalues;
        Action::manager().store("Reset Filters", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::Text("Filters");

    if (ImGuiToolkit::ButtonIcon(6, 4)) {
        n.gamma = glm::vec4(1.f, 1.f, 1.f, 1.f);
        Action::manager().store("Gamma & Color", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Gamma Color", glm::value_ptr(n.gamma), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) ;
    if (ImGui::IsItemDeactivatedAfterEdit())
        Action::manager().store("Gamma Color changed", n.id());

    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Gamma", &n.gamma.w, 0.5f, 10.f, "%.2f", 2.f);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Gamma " << std::setprecision(2) << n.gamma.w;
        Action::manager().store(oss.str(), n.id());
    }

//    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//    ImGui::SliderFloat4("Levels", glm::value_ptr(n.levels), 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(5, 16)) {
        n.brightness = 0.f;
        n.contrast = 0.f;
        Action::manager().store("B & C  0.0 0.0", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float bc[2] = { n.brightness, n.contrast};
    if ( ImGui::SliderFloat2("B & C", bc, -1.0, 1.0) )
    {
        n.brightness = bc[0];
        n.contrast = bc[1];
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "B & C  " << std::setprecision(2) << n.brightness << " " << n.contrast;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(9, 16)) {
        n.saturation = 0.f;
        Action::manager().store("Saturation 0.0", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Saturation", &n.saturation, -1.0, 1.0);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Saturation " << std::setprecision(2) << n.saturation;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(12, 4)) {
        n.hueshift = 0.f;
        Action::manager().store("Hue shift 0.0", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Hue shift", &n.hueshift, 0.0, 1.0);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Hue shift " << std::setprecision(2) << n.hueshift;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(18, 1)) {
        n.nbColors = 0;
        Action::manager().store("Posterize None", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderInt("Posterize", &n.nbColors, 0, 16, n.nbColors == 0 ? "None" : "%d colors");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Posterize ";
        if (n.nbColors == 0) oss << "None"; else oss << n.nbColors;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(8, 1)) {
        n.threshold = 0.f;
        Action::manager().store("Threshold None", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Threshold", &n.threshold, 0.0, 1.0, n.threshold < 0.001 ? "None" : "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Threshold ";
        if (n.threshold < 0.001) oss << "None"; else oss << std::setprecision(2) << n.threshold;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(3, 1)) {
        n.lumakey = 0.f;
        Action::manager().store("Lumakey 0.0", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Lumakey", &n.lumakey, 0.0, 1.0);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Lumakey " << std::setprecision(2) << n.lumakey;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(13, 4)) {
        n.chromakey = glm::vec4(0.f, 0.8f, 0.f, 1.f);
        n.chromadelta = 0.f;
        Action::manager().store("Chromakey & Color Reset", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Chroma color", glm::value_ptr(n.chromakey), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel  ) ;    
    if (ImGui::IsItemDeactivatedAfterEdit())
        Action::manager().store("Chroma color changed", n.id());
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Chromakey", &n.chromadelta, 0.0, 1.0, n.chromadelta < 0.001 ? "None" : "Tolerance %.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Chromakey ";
        if (n.chromadelta < 0.001) oss << "None"; else oss << std::setprecision(2) << n.chromadelta;
        Action::manager().store(oss.str(), n.id());
    }

    if (ImGuiToolkit::ButtonIcon(6, 16)) {
        n.invert = 0;
        Action::manager().store("Invert None", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::Combo("Invert", &n.invert, "None\0Invert Color\0Invert Luminance\0"))
        Action::manager().store("Invert " + std::string(n.invert<1 ? "None": (n.invert>1 ? "Luminance" : "Color")), n.id());

    if (ImGuiToolkit::ButtonIcon(1, 7)) {
        n.filterid = 0;
        Action::manager().store("Filter None", n.id());
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::Combo("Filter", &n.filterid, ImageProcessingShader::filter_names, IM_ARRAYSIZE(ImageProcessingShader::filter_names) ) )
        Action::manager().store("Filter " + std::string(ImageProcessingShader::filter_names[n.filterid]), n.id());

    ImGui::PopID();

    ImGui::Spacing();
}


void ImGuiVisitor::visit (Source& s)
{
    ImGui::PushID(std::to_string(s.id()).c_str());
    // blending
    s.blendingShader()->accept(*this);

    // preview
    float preview_width = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
    float preview_height = 4.5f * ImGui::GetFrameHeightWithSpacing();
    ImVec2 pos = ImGui::GetCursorPos(); // remember where we were...

    float space = ImGui::GetStyle().ItemSpacing.y;
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
            ImGuiToolkit::HelpMarker("Visible", ICON_FA_EYE);
        else
            ImGuiToolkit::HelpMarker("Not visible", ICON_FA_EYE_SLASH);
    }
    else
        ImGuiToolkit::HelpMarker("Inactive", ICON_FA_SNOWFLAKE);

    // Inform on workspace
    ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + ImGui::GetFrameHeightWithSpacing()) );
    if (s.workspace() == Source::BACKGROUND)
        ImGuiToolkit::HelpIcon("Background",10, 16);
    else if (s.workspace() == Source::FOREGROUND)
        ImGuiToolkit::HelpIcon("Foreground",12, 16);
    else
        ImGuiToolkit::HelpIcon("Stage",11, 16);

    // locking
    ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + 2.f * ImGui::GetFrameHeightWithSpacing()) );
    const char *tooltip[2] = {"Unlocked", "Locked"};
    bool l = s.locked();
    if (ImGuiToolkit::IconToggle(15,6,17,6, &l, tooltip ) )
        s.setLocked(l);

    // toggle enable/disable image processing
    bool on = s.imageProcessingEnabled();
    ImGui::SetCursorPos( ImVec2(preview_width + 15, pos.y + 3.5f * ImGui::GetFrameHeightWithSpacing()) );
    if ( ImGuiToolkit::ButtonToggle(ICON_FA_MAGIC, &on) ){
        std::ostringstream oss;
        oss << s.name() << ": " << ( on ? "Enable Filter" : "Disable Filter");
        Action::manager().store(oss.str(), s.id());
    }
    s.setImageProcessingEnabled(on);

    ImGui::SetCursorPosY(pos.y + preview_height); // ...come back

    // image processing pannel
    if (s.imageProcessingEnabled())
        s.processingShader()->accept(*this);

    // geometry direct control
//    s.groupNode(View::GEOMETRY)->accept(*this);
//    s.groupNode((View::Mode) Settings::application.current_view)->accept(*this);

    ImGui::PopID();
}

void ImGuiVisitor::visit (MediaSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    if ( s.mediaplayer()->isImage() )
        ImGui::Text("Image File");
    else
        ImGui::Text("Video File");

    if ( ImGui::Button(IMGUI_TITLE_MEDIAPLAYER, ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        UserInterface::manager().showMediaPlayer( s.mediaplayer());
    ImGuiToolkit::ButtonOpenUrl( SystemToolkit::path_filename(s.path()).c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
}

void ImGuiVisitor::visit (SessionSource& s)
{
    if (s.session() == nullptr)
        return;

    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Session File");
//    ImGui::Text("%s", SystemToolkit::base_filename(s.path()).c_str());

    if (ImGuiToolkit::ButtonIcon(3, 2)) s.session()->setFading(0.f);
    float f = s.session()->fading();
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::SliderFloat("Fading", &f, 0.0, 1.0, f < 0.001 ? "None" : "%.2f") )
        s.session()->setFading(f);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << s.name() << ": Fading " << std::setprecision(2) << f;
        Action::manager().store(oss.str(), s.id());
    }

    if ( ImGui::Button( ICON_FA_FILE_UPLOAD " Open Session", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().set( s.detach() );
    if ( ImGui::Button( ICON_FA_FILE_EXPORT " Import Session", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().import( &s );

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
        for (uint p = 0; p < Pattern::pattern_types.size(); ++p){
            if (ImGui::Selectable( Pattern::pattern_types[p].c_str() )) {
                s.setPattern(p, s.pattern()->resolution());
                std::ostringstream oss;
                oss << s.name() << ": Pattern " << Pattern::pattern_types[p];
                Action::manager().store(oss.str(), s.id());
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
                std::ostringstream oss;
                oss << s.name() << " Device " << namedev;
                Action::manager().store(oss.str(), s.id());
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

void ImGuiVisitor::visit (NetworkSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Network stream");

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
    ImGui::Text("%s", s.connection().c_str());
    ImGui::PopStyleColor(1);
    NetworkStream *ns = s.networkStream();
    ImGui::Text(" - %s (%dx%d)\n - Server address %s", NetworkToolkit::protocol_name[ns->protocol()],
            ns->resolution().x, ns->resolution().y, ns->serverAddress().c_str());

    if ( ImGui::Button( ICON_FA_REPLY " Reconnect", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
    {
        // TODO : reload ?
        s.setConnection(s.connection());
    }


}

