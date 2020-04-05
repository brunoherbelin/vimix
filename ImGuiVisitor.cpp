#include "ImGuiVisitor.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
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
    if (ImGui::TreeNode(id.c_str(), "Group"))
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
        ImGui::SliderAngle("slider angle", &(n.rotation_.z), -180.f, 180.f) ;

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

}

void ImGuiVisitor::visit(Primitive &n)
{
    ImGui::PushID(n.id());
    ImGui::Text("Primitive");

    n.getShader()->accept(*this);

    ImGui::PopID();
}

void ImGuiVisitor::visit(ImageSurface &n)
{

}

void ImGuiVisitor::visit(MediaSurface &n)
{
    ImGui::Text("%s", n.getFilename().c_str());

    if (n.getMediaPlayer())
        n.getMediaPlayer()->accept(*this);
}

void ImGuiVisitor::visit(MediaPlayer &n)
{

    ImGui::Text("Media Player");
}

void ImGuiVisitor::visit(Shader &n)
{
    ImGui::ColorEdit3("color", glm::value_ptr(n.color) ) ;
}

void ImGuiVisitor::visit(ImageShader &n)
{
}

void ImGuiVisitor::visit(LineStrip &n)
{
    ImGui::Text("Lines");
}

void ImGuiVisitor::visit(LineSquare &n)
{
    ImGui::Text("Square");
}

void ImGuiVisitor::visit(LineCircle &n)
{
    ImGui::Text("Circle");
}

void ImGuiVisitor::visit(Scene &n)
{
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Scene Property Tree"))
    {
        n.getRoot()->accept(*this);
    }
}


