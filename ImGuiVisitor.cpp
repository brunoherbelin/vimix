#include "ImGuiVisitor.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
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
    ImGui::Text("%s", n.getUri().c_str());

    if (n.getMediaPlayer())
        n.getMediaPlayer()->accept(*this);
}

void ImGuiVisitor::visit(MediaPlayer &n)
{
    ImGui::Text("Media Player");
}

void ImGuiVisitor::visit(Shader &n)
{
    ImGui::PushID(n.id());

    ImGui::ColorEdit3("color", glm::value_ptr(n.color) ) ;

    ImGui::PopID();
}

void ImGuiVisitor::visit(ImageShader &n)
{
    ImGui::PushID(n.id());

    if (ImGuiToolkit::ButtonIcon(2, 1)) {
        n.brightness = 0.f;
        n.contrast = 0.f;
    }
    ImGui::SameLine(0, 10);
    float bc[2] = { n.brightness, n.contrast};
    if ( ImGui::SliderFloat2("filter", bc, -1.0, 1.0) )
    {
        n.brightness = bc[0];
        n.contrast = bc[1];
    }
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


