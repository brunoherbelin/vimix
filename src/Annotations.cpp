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

#include <clocale>
#include <cstring>

#include <tinyxml2.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "defines.h"
#include "IconsFontAwesome5.h"
#include "Log.h"
#include "Screenshot.h"
#include "RenderingManager.h"
#include "Toolkit/ImGuiToolkit.h"
#include "Toolkit/tinyxml2Toolkit.h"

#include "Annotations.h"

using namespace tinyxml2;

#define ANNOTATION_HANDLE_RADIUS 7.f
#define ANNOTATION_SHADOW_OFFSET 2.f
#define ANNOTATION_FILE_TYPE "vimix annotations (XML)"
#define ANNOTATION_FILE_PATTERN { "*.xml" }
#define SCREENSHOT_FILE_TYPE "PNG image"
#define SCREENSHOT_FILE_PATTERN { "*.png" }

static const char *type_names[] = { "Arrow", "Text", "Frame", "Circle" };
static const char *type_icons[] = { ICON_FA_LONG_ARROW_ALT_RIGHT, ICON_FA_FONT, ICON_FA_SQUARE, ICON_FA_CIRCLE };
static const char *font_names[] = { "Default", "Bold", "Italic", "Mono", "Large" };
static const char *text_icons[] = { ICON_FA_MOUSE_POINTER, ICON_FA_HAND_POINTER, ICON_FA_KEYBOARD,
                                    ICON_FA_PLUS, ICON_FA_ARROWS_ALT, ICON_FA_EXPAND_ALT, ICON_FA_CHECK, ICON_FA_TIMES, ICON_FA_EXCLAMATION_TRIANGLE };

static inline ImVec2 iv(glm::vec2 v) { return ImVec2(v.x, v.y); }

///
/// ANNOTATION
///
Annotation::Annotation(Type t, glm::vec2 position) : type(t), text(""), color(0.55f, 1.f, 0.25f, 1.f),
    thickness(3.f), size(30.f), font(ImGuiToolkit::FONT_ITALIC), shadow(false)
{
    p[0] = p[1] = p[2] = position;

    switch (type) {
    case ARROW:
        p[0] = position + glm::vec2(-140.f, 90.f);
        p[2] = (p[0] + p[1]) * 0.5f;
        break;
    case TEXT:
        text = "Text";
        break;
    case FRAME:
        p[0] = position - glm::vec2(80.f, 30.f);
        p[1] = position + glm::vec2(80.f, 30.f);
        break;
    case CIRCLE:
        p[1] = position + glm::vec2(40.f, 0.f);
        break;
    default:
        break;
    }
}

int Annotation::numPoints() const
{
    switch (type) {
    case ARROW:
        return 3;
    case FRAME:
    case CIRCLE:
        return 2;
    default:
        return 1;
    }
}

void Annotation::move(glm::vec2 delta)
{
    for (int k = 0; k < 3; ++k)
        p[k] += delta;
}

std::string Annotation::label() const
{
    std::string l = std::string(type_icons[type]) + "  " + type_names[type];
    if (!text.empty()) {
        std::string t = text.substr(0, text.find('\n'));
        if (t.size() > 24)
            t = t.substr(0, 24) + "...";
        l += " : " + t;
    }
    return l;
}

void Annotation::draw(ImDrawList *dl, glm::vec2 offset, bool as_shadow) const
{
    const ImU32 col = ImGui::ColorConvertFloat4ToU32( as_shadow ? ImVec4(0.f, 0.f, 0.f, 0.4f * color.a)
                                                                : ImVec4(color.r, color.g, color.b, color.a) );
    switch (type) {
    case ARROW:
    {
        const glm::vec2 a = p[0] + offset;
        const glm::vec2 b = p[1] + offset;
        const glm::vec2 c = p[2] + offset;
        // direction of arrow head is the tangent at the end of the curve
        glm::vec2 dir = b - c;
        if (glm::length(dir) < 1.f)
            dir = b - a;
        if (glm::length(dir) < 1.f)
            break;
        dir = glm::normalize(dir);
        const glm::vec2 perp(-dir.y, dir.x);
        const float head = 4.f * thickness + 6.f;
        const glm::vec2 base = b - dir * head;
        // quadratic curve (tail, control, base of head) as a cubic bezier
        const glm::vec2 c1 = a + (c - a) * (2.f / 3.f);
        const glm::vec2 c2 = base + (c - base) * (2.f / 3.f);
        dl->AddBezierCurve(iv(a), iv(c1), iv(c2), iv(base), col, thickness);
        dl->AddTriangleFilled(iv(b), iv(base + perp * head * 0.5f), iv(base - perp * head * 0.5f), col);
    }
        break;
    case TEXT:
    {
        ImGuiToolkit::PushFont( (ImGuiToolkit::font_style) font );
        ImFont *f = ImGui::GetFont();
        ImGui::PopFont();
        dl->AddText(f, size, iv(p[0] + offset), col, text.c_str());
    }
        break;
    case FRAME:
    {
        const glm::vec2 a = glm::min(p[0], p[1]) + offset;
        const glm::vec2 b = glm::max(p[0], p[1]) + offset;
        dl->AddRect(iv(a), iv(b), col, 2.f * thickness, ImDrawCornerFlags_All, thickness);
    }
        break;
    case CIRCLE:
        dl->AddCircle(iv(p[0] + offset), glm::length(p[1] - p[0]), col, 64, thickness);
        break;
    default:
        break;
    }
}

///
/// ANNOTATIONS
///
Annotations::Annotations() : selected_(-1), active_(false), edit_(false)
{
    opendialog_ = new DialogToolkit::OpenFileDialog("Open Annotations",
                                                    ANNOTATION_FILE_TYPE, ANNOTATION_FILE_PATTERN);
    savedialog_ = new DialogToolkit::SaveFileDialog("Save Annotated Screenshot",
                                                    SCREENSHOT_FILE_TYPE, SCREENSHOT_FILE_PATTERN);
}

void Annotations::Menu()
{
    if (ImGui::MenuItem(ICON_FA_CHALKBOARD_TEACHER "  Active", nullptr, &active_) && !active_)
        edit_ = false;
    if (ImGui::MenuItem(ICON_FA_PEN "  Editor", nullptr, &edit_) && edit_)
        active_ = true;

    ImGui::Separator();
    for (int t = Annotation::ARROW; t < Annotation::INVALID; ++t) {
        std::string label = std::string(type_icons[t]) + "  New " + type_names[t];
        if (ImGui::MenuItem(label.c_str()))
            add( (Annotation::Type) t );
    }

    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open"))
        opendialog_->open();
    if (ImGui::MenuItem(ICON_FA_BROOM "  Clear", nullptr, false, !items_.empty()))
        clear();
}

void Annotations::add(Annotation::Type t)
{
    ImGuiIO &io = ImGui::GetIO();
    glm::vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    center += glm::vec2( 20.f * float(items_.size() % 8) );

    Annotation a(t, center);
    // new annotation inherits style of the selected one
    if (selected_ > -1 && selected_ < (int) items_.size()) {
        const Annotation &s = items_[selected_];
        a.color = s.color;
        a.thickness = s.thickness;
        a.shadow = s.shadow;
        if (s.type == Annotation::TEXT) {
            a.font = s.font;
            a.size = s.size;
        }
    }

    items_.push_back(a);
    selected_ = (int) items_.size() - 1;
    active_ = true;
    edit_ = true;
}

void Annotations::clear()
{
    items_.clear();
    selected_ = -1;
}

void Annotations::saveScreenshot()
{
    savedialog_->open();
}

void Annotations::Render(bool capturing)
{
    // file dialogs
    if (opendialog_->closed() && !opendialog_->path().empty())
        load(opendialog_->path());

    if (savedialog_->closed() && !savedialog_->path().empty()) {
        const std::string png = savedialog_->path();
        Screenshot *s = Rendering::manager().currentScreenshot();
        if (s->isFull()) {
            s->save(png);
            save( png.substr(0, png.find_last_of('.')) + ".xml" );
            Log::Notify("Screenshot saved %s", png.c_str());
        }
    }

    if (!active_)
        return;

    // draw annotations on top of everything
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    for (const auto &a : items_) {
        if (a.shadow)
            a.draw(dl, glm::vec2(ANNOTATION_SHADOW_OFFSET), true);
        a.draw(dl);
    }

    // no editing when capturing screenshot
    if (capturing || !edit_)
        return;

    RenderHandles();
    RenderEditor();
}

void Annotations::RenderHandles()
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    const float r = ANNOTATION_HANDLE_RADIUS;

    // guides of the curve for the selected arrow
    if (selected_ > -1 && selected_ < (int) items_.size() && items_[selected_].type == Annotation::ARROW) {
        const Annotation &a = items_[selected_];
        dl->AddLine(iv(a.p[0]), iv(a.p[2]), IM_COL32(255, 255, 255, 90), 1.f);
        dl->AddLine(iv(a.p[2]), iv(a.p[1]), IM_COL32(255, 255, 255, 90), 1.f);
    }

    // each handle is a tiny invisible window, so that ImGui captures the mouse
    // on top of other windows and vimix views do not react to the clic
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

    for (int i = 0; i < (int) items_.size(); ++i) {
        Annotation &a = items_[i];
        for (int k = 0; k < a.numPoints(); ++k) {
            char id[64];
            snprintf(id, 64, "##AnnotationHandle%d_%d", i, k);
            ImGui::SetNextWindowPos(ImVec2(a.p[k].x - r, a.p[k].y - r));
            ImGui::SetNextWindowSize(ImVec2(2.f * r, 2.f * r));
            if (ImGui::Begin(id, nullptr, flags)) {
                ImGui::InvisibleButton("##handle", ImVec2(2.f * r, 2.f * r));
                const bool hovered = ImGui::IsItemHovered();
                const bool active = ImGui::IsItemActive();
                if (ImGui::IsItemClicked())
                    selected_ = i;
                // drag point, or whole annotation with SHIFT
                if (active && (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f)) {
                    const glm::vec2 delta(io.MouseDelta.x, io.MouseDelta.y);
                    if (io.KeyShift)
                        a.move(delta);
                    else
                        a.p[k] += delta;
                }
                ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

                const float rr = (hovered || active) ? r : r * 0.7f;
                const ImU32 col = (i == selected_) ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 150);
                dl->AddCircleFilled(iv(a.p[k]), rr, IM_COL32(0, 0, 0, 150), 16);
                dl->AddCircle(iv(a.p[k]), rr, col, 16, 2.f);
            }
            ImGui::End();
        }
    }

    ImGui::PopStyleVar(3);
}

void Annotations::RenderEditor()
{
    ImGui::SetNextWindowPos(ImVec2(500, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 480), ImGuiCond_FirstUseEver);
    if ( !ImGui::Begin(ICON_FA_PEN "  Annotations", &edit_) ) {
        ImGui::End();
        return;
    }

    // add new annotations
    ImGui::AlignTextToFramePadding();
    ImGui::Text("New");
    for (int t = Annotation::ARROW; t < Annotation::INVALID; ++t) {
        ImGui::SameLine();
        if (ImGui::Button(type_icons[t], ImVec2(2.f * ImGui::GetFrameHeight(), 0.f)))
            add( (Annotation::Type) t );
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(type_names[t]);
    }

    // list of annotations
    ImGui::BeginChild("##AnnotationList", ImVec2(0, 120), true);
    for (int i = 0; i < (int) items_.size(); ++i) {
        ImGui::PushID(i);
        if (ImGui::Selectable(items_[i].label().c_str(), selected_ == i))
            selected_ = i;
        ImGui::PopID();
    }
    ImGui::EndChild();

    // properties of selected annotation
    bool duplicate = false;
    bool remove = false;
    if (selected_ > -1 && selected_ < (int) items_.size()) {
        Annotation &a = items_[selected_];

        if (a.type == Annotation::TEXT) {
            ImGuiToolkit::InputTextMultiline("##AnnotationText", &a.text, ImVec2(-1.f, 3.f * ImGui::GetTextLineHeightWithSpacing()));
            for (size_t k = 0; k < IM_ARRAYSIZE(text_icons); ++k) {
                if (k > 0)
                    ImGui::SameLine();
                ImGui::PushID((int) k);
                if (ImGui::SmallButton(text_icons[k]))
                    a.text += text_icons[k];
                ImGui::PopID();
            }
            ImGui::Combo("Font", &a.font, font_names, IM_ARRAYSIZE(font_names));
            ImGui::SliderFloat("Size", &a.size, 8.f, 150.f, "%.0f px");
        }
        else
            ImGui::SliderFloat("Thickness", &a.thickness, 1.f, 12.f, "%.1f px");

        ImGui::ColorEdit4("Color", &a.color[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::Checkbox("Shadow", &a.shadow);

        duplicate = ImGui::Button(ICON_FA_CLONE "  Duplicate");
        ImGui::SameLine();
        remove = ImGui::Button(ICON_FA_TRASH "  Delete");
    }

    if (duplicate) {
        Annotation a = items_[selected_];
        a.move(glm::vec2(20.f));
        items_.push_back(a);
        selected_ = (int) items_.size() - 1;
    }
    else if (remove) {
        items_.erase(items_.begin() + selected_);
        selected_ = -1;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Drag handles to edit; SHIFT + drag to move. "
                       "[F9] saves screenshot (PNG) and annotations (XML) with the same name.");
    ImGui::PopStyleColor();

    ImGui::End();
}

bool Annotations::save(const std::string &filename) const
{
    setlocale(LC_ALL, "C");
    XMLDocument xmlDoc;

    XMLElement *pRoot = xmlDoc.NewElement(APP_NAME);
    xmlDoc.InsertEndChild(pRoot);

    XMLElement *node = xmlDoc.NewElement("Annotations");
    ImGuiIO &io = ImGui::GetIO();
    node->SetAttribute("width", (int) io.DisplaySize.x);
    node->SetAttribute("height", (int) io.DisplaySize.y);
    pRoot->InsertEndChild(node);

    for (const auto &a : items_) {
        XMLElement *e = xmlDoc.NewElement("Annotation");
        e->SetAttribute("type", type_names[a.type]);
        e->SetAttribute("thickness", a.thickness);
        e->SetAttribute("size", a.size);
        e->SetAttribute("font", a.font);
        e->SetAttribute("shadow", a.shadow);
        e->InsertEndChild( XMLElementFromGLM(&xmlDoc, a.color) );
        for (int k = 0; k < a.numPoints(); ++k)
            e->InsertEndChild( XMLElementFromGLM(&xmlDoc, a.p[k]) );
        if (!a.text.empty()) {
            XMLElement *t = xmlDoc.NewElement("text");
            t->SetText(a.text.c_str());
            e->InsertEndChild(t);
        }
        node->InsertEndChild(e);
    }

    return XMLSaveDoc(&xmlDoc, filename);
}

bool Annotations::load(const std::string &filename)
{
    setlocale(LC_ALL, "C");
    XMLDocument xmlDoc;
    if ( XMLResultError(xmlDoc.LoadFile(filename.c_str())) ) {
        Log::Warning("Cannot open annotations file %s", filename.c_str());
        return false;
    }

    XMLElement *pRoot = xmlDoc.FirstChildElement(APP_NAME);
    XMLElement *node = pRoot ? pRoot->FirstChildElement("Annotations") : nullptr;
    if (!node) {
        Log::Warning("No annotations in file %s", filename.c_str());
        return false;
    }

    clear();
    for (XMLElement *e = node->FirstChildElement("Annotation"); e; e = e->NextSiblingElement("Annotation")) {
        const char *type = e->Attribute("type");
        int t = Annotation::ARROW;
        while (t < Annotation::INVALID && (!type || strcmp(type, type_names[t]) != 0))
            ++t;
        if (t == Annotation::INVALID)
            continue;

        Annotation a( (Annotation::Type) t );
        e->QueryFloatAttribute("thickness", &a.thickness);
        e->QueryFloatAttribute("size", &a.size);
        e->QueryIntAttribute("font", &a.font);
        e->QueryBoolAttribute("shadow", &a.shadow);
        XMLElementToGLM(e->FirstChildElement("vec4"), a.color);
        int k = 0;
        for (XMLElement *v = e->FirstChildElement("vec2"); v && k < 3; v = v->NextSiblingElement("vec2"), ++k)
            XMLElementToGLM(v, a.p[k]);
        XMLElement *text = e->FirstChildElement("text");
        a.text = (text && text->GetText()) ? text->GetText() : "";
        items_.push_back(a);
    }

    int w = 0, h = 0;
    node->QueryIntAttribute("width", &w);
    node->QueryIntAttribute("height", &h);
    ImGuiIO &io = ImGui::GetIO();
    if (w != (int) io.DisplaySize.x || h != (int) io.DisplaySize.y)
        Log::Info("Annotations were made for a %d x %d window.", w, h);

    // next annotated screenshot goes to the same folder
    savedialog_->setFolder(filename);
    active_ = true;
    Log::Notify("Annotations loaded from %s", filename.c_str());
    return true;
}
