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

        //    n.transform_
            // MODEL VIEW
            float translation[2] = { n.transform_[3][0], n.transform_[3][1]};
            if (ImGuiToolkit::ButtonIcon(6, 15)) {
                translation[0] = 0.f;
                translation[1] = 0.f;
            }
            ImGui::SameLine(0, 10);
            ImGui::SliderFloat2("position", translation, -5.0, 5.0);

            float rotation = asin(n.transform_[0][1]);
            if (ImGuiToolkit::ButtonIcon(4, 15)) rotation = 0.f;
            ImGui::SameLine(0, 10);
            ImGui::SliderFloat("rotation", &rotation, 0, glm::two_pi<float>());

            float scale = n.transform_[2][2];
            if (ImGuiToolkit::ButtonIcon(3, 15))  scale = 1.f;
            ImGui::SameLine(0, 10);
            ImGui::SliderFloat("scale", &scale, 0.1f, 10.f, "%.3f", 3.f);

            glm::mat4 View = glm::translate(glm::identity<glm::mat4>(), glm::vec3(translation[0], translation[1], 0.f));
            View = glm::rotate(View, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 Model = glm::scale(glm::identity<glm::mat4>(), glm::vec3(scale, scale, scale));
            n.transform_ = View * Model;

        // loop over members of a group
        for (int i = 0; i < n.numChildren(); ++i)
        {
            n.getChild(i)->accept(*this);
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


