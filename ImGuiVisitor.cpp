#include "ImGuiVisitor.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"

#include "imgui.h"
#include "ImGuiToolkit.h"

#define RIGHT_ALIGN -100

ImGuiVisitor::ImGuiVisitor()
{

}

void ImGuiVisitor::visit(Node &n)
{

}

void ImGuiVisitor::visit(Group &n)
{
    std::string id = std::to_string(n.id());
    if (ImGui::TreeNode(id.c_str(), "Group %d", n.id()))
    {
        // MODEL VIEW
        if (ImGuiToolkit::ButtonIcon(6, 15)) {
            n.translation_.x = 0.f;
            n.translation_.y = 0.f;
        }
        ImGui::SameLine(0, 10);
        float translation[2] = { n.translation_.x, n.translation_.y};
        if ( ImGui::SliderFloat2("position", translation, -5.0, 5.0) )
        {
            n.translation_.x = translation[0];
            n.translation_.y = translation[1];
        }

        if (ImGuiToolkit::ButtonIcon(4, 15))
            n.rotation_.z = 0.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderAngle("angle", &(n.rotation_.z), -180.f, 180.f) ;

        if (ImGuiToolkit::ButtonIcon(3, 15))  {
            n.scale_.x = 1.f;
            n.scale_.y = 1.f;
        }
        ImGui::SameLine(0, 10);
        float scale = n.scale_.y;
        if ( ImGui::SliderFloat("scale", &scale, 0.1f, 10.f, "%.3f", 3.f) )
        {
            n.scale_.x = scale;
            n.scale_.y = scale;
        }

        // loop over members of a group
        for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
            (*node)->accept(*this);
        }

        ImGui::TreePop();
    }
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
    ImGui::Text("%s", n.uri().c_str());

    if (n.mediaPlayer())
        n.mediaPlayer()->accept(*this);
}

void ImGuiVisitor::visit(MediaPlayer &n)
{
    ImGui::Text("Media Player");
}

void ImGuiVisitor::visit(Shader &n)
{
    float width = ImGui::GetContentRegionAvail().x;

    ImGui::PushID(n.id());

//    if (ImGuiToolkit::ButtonIcon(14, 8)) n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
//    ImGui::SameLine(0, 10);
//    ImGui::ColorEdit3("Color", glm::value_ptr(n.color) ) ;

    if (ImGuiToolkit::ButtonIcon(10, 2)) {
        n.blending = Shader::BLEND_OPACITY;
        n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Color", glm::value_ptr(n.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel ) ;
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    int mode = n.blending;
    if (ImGui::Combo("Blending", &mode, "Normal\0Screen\0Inverse\0Addition\0Subtract\0") )
        n.blending = Shader::BlendMode(mode);

    ImGui::PopID();
}

void ImGuiVisitor::visit(ImageShader &n)
{
    ImGui::PushID(n.id());

    if (ImGuiToolkit::ButtonIcon(4, 1)) {
        n.brightness = 0.f;
        n.contrast = 0.f;
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    float bc[2] = { n.brightness, n.contrast};
    if ( ImGui::SliderFloat2("B & C", bc, -1.0, 1.0) )
    {
        n.brightness = bc[0];
        n.contrast = bc[1];
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
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    float bc[2] = { n.brightness, n.contrast};
    if ( ImGui::SliderFloat2("B & C", bc, -1.0, 1.0) )
    {
        n.brightness = bc[0];
        n.contrast = bc[1];
    }

    if (ImGuiToolkit::ButtonIcon(2, 1)) n.saturation = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    ImGui::SliderFloat("Saturation", &n.saturation, -1.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(12, 4)) n.hueshift = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    ImGui::SliderFloat("Hue shift", &n.hueshift, 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(8, 1)) n.threshold = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    ImGui::SliderFloat("Threshold", &n.threshold, 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(3, 1)) n.lumakey = 0.f;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    ImGui::SliderFloat("Lumakey", &n.lumakey, 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(18, 1)) n.nbColors = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    ImGui::SliderInt("Posterize", &n.nbColors, 0, 16, "%d colors");

    if (ImGuiToolkit::ButtonIcon(1, 7)) n.filter = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
    ImGui::Combo("Filter", &n.filter, "None\0Blur\0Sharpen\0Edge\0Emboss\0Erode 3x3\0Erode 5x5\0Erode 7x7\0Dilate 3x3\0Dilate 5x5\0Dilate 7x7\0");

    if (ImGuiToolkit::ButtonIcon(7, 1)) n.invert = 0;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
//    static const char* invert_names[3] = { "None", "Color", "Luminance" };
//    const char* current_invert_name = invert_names[n.invert];
//    ImGui::SliderInt("Invert", &n.invert, 0, 2, current_invert_name);
    ImGui::Combo("Invert", &n.invert, "None\0Invert Color\0Invert Luminance\0");

    if (ImGuiToolkit::ButtonIcon(13, 4)) {
        n.chromakey = glm::vec4(0.f, 1.f, 0.f, 1.f);
        n.chromadelta = 0.f;
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Chroma color", glm::value_ptr(n.chromakey), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel  ) ;
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(RIGHT_ALIGN);
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


