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

#include "imgui.h"
#include "ImGuiToolkit.h"


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
    if ( ImGui::SliderFloat2("position", translation, -5.0, 5.0) )
    {
        n.translation_.x = translation[0];
        n.translation_.y = translation[1];
    }

    if (ImGuiToolkit::ButtonIcon(18, 9))
        n.rotation_.z = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderAngle("angle", &(n.rotation_.z), -180.f, 180.f) ;

    if (ImGuiToolkit::ButtonIcon(3, 15))  {
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
    }
    ImGui::SameLine(0, 10);
    float scale[2] = { n.scale_.x, n.scale_.y} ;
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("scale", scale, -5.0, 5.0, "%.2f") )
    {
        n.scale_.x = scale[0];
        n.scale_.y = scale[1];
    }

//        // loop over members of a group
//        for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
//            (*node)->accept(*this);
//        }

//        ImGui::TreePop();
//    }
}

void ImGuiVisitor::visit(Switch &n)
{
    // TODO : display selection of active child
    (*n.activeChild())->accept(*this);
}

void ImGuiVisitor::visit(Animation &n)
{
    // TODO : display group and animation parameters

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

    if (ImGuiToolkit::ButtonIcon(10, 2)) {
        n.blending = Shader::BLEND_OPACITY;
        n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Color", glm::value_ptr(n.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel ) ;
    ImGui::SameLine(0, 5);
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

    if (ImGuiToolkit::ButtonIcon(10, 3)) n.mask = 0;
    ImGui::SameLine(0, 10);
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

    if (ImGuiToolkit::ButtonIcon(8, 1)) n.threshold = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Threshold", &n.threshold, 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(3, 1)) n.lumakey = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Lumakey", &n.lumakey, 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(18, 1)) n.nbColors = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderInt("Posterize", &n.nbColors, 0, 16, "%d colors");

    if (ImGuiToolkit::ButtonIcon(1, 7)) n.filterid = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("Filter", &n.filterid, ImageProcessingShader::filter_names, IM_ARRAYSIZE(ImageProcessingShader::filter_names) );

    if (ImGuiToolkit::ButtonIcon(7, 1)) n.invert = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("Invert", &n.invert, "None\0Invert Color\0Invert Luminance\0");

    if (ImGuiToolkit::ButtonIcon(13, 4)) {
        n.chromakey = glm::vec4(0.f, 1.f, 0.f, 1.f);
        n.chromadelta = 0.f;
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Chroma color", glm::value_ptr(n.chromakey), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel  ) ;
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Chromakey", &n.chromadelta, 0.0, 1.0, "%.2f tolerance");

    ImGui::PopID();
}

void ImGuiVisitor::visit(Scene &n)
{
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Scene Property Tree"))
    {
        n.root()->accept(*this);
    }
}


