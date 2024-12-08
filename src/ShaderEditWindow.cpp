#include <fstream>

// ImGui
#include "ImGuiToolkit.h"
#include "imgui_internal.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GLFW/glfw3.h>

#include "TextEditor.h"
TextEditor _editor;

#include "defines.h"
#include "Settings.h"
#include "Mixer.h"
#include "SystemToolkit.h"
#include "CloneSource.h"
#include "DialogToolkit.h"
#include "UserInterfaceManager.h"

#include "ShaderEditWindow.h"

///
/// SHADER EDITOR
///
///
ShaderEditWindow::ShaderEditWindow() : WorkspaceWindow("Shader"), current_(nullptr), show_shader_inputs_(false)
{
    auto lang = TextEditor::LanguageDefinition::GLSL();

    static const char* const keywords[] = {
        "discard", "attribute", "varying", "uniform", "in", "out", "inout", "bvec2", "bvec3", "bvec4", "dvec2",
        "dvec3", "dvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4", "vec2", "vec3", "vec4", "mat2",
        "mat3", "mat4", "dmat2", "dmat3", "dmat4", "sampler1D", "sampler2D", "sampler3D", "samplerCUBE", "samplerbuffer",
        "sampler1DArray", "sampler2DArray", "sampler1DShadow", "sampler2DShadow", "vec4", "vec4", "smooth", "flat",
        "precise", "coherent", "uint", "struct", "switch", "unsigned", "void", "volatile", "while", "readonly"
    };
    for (auto& k : keywords)
        lang.mKeywords.insert(k);

    static const char* const identifiers[] = {
        "radians",  "degrees", "sin",  "cos", "tan", "pow", "exp2", "log2", "sqrt", "inversesqrt",
        "sign", "floor", "ceil", "fract", "mod", "min", "max", "clamp", "mix", "step", "smoothstep", "length", "distance",
        "dot", "cross", "normalize", "ftransform", "faceforward", "reflect", "matrixcompmult", "lessThan", "lessThanEqual",
        "greaterThan", "greaterThanEqual", "equal", "notEqual", "any", "all", "not", "texture1D", "texture1DProj", "texture1DLod",
        "texture1DProjLod", "texture", "texture2D", "texture2DProj", "texture2DLod", "texture2DProjLod", "texture3D",
        "texture3DProj", "texture3DLod", "texture3DProjLod", "textureCube", "textureCubeLod", "shadow1D", "shadow1DProj",
        "shadow1DLod", "shadow1DProjLod", "shadow2D", "shadow2DProj", "shadow2DLod", "shadow2DProjLod",
        "dFdx", "dFdy", "fwidth", "noise1", "noise2", "noise3", "noise4", "refract", "exp", "log", "mainImage",
    };
    for (auto& k : identifiers)
    {
        TextEditor::Identifier id;
        id.mDeclaration = "GLSL function";
        lang.mIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    static const char* const filter_keyword[] = {
        "iResolution", "iTime", "iTimeDelta", "iFrame", "iChannelResolution", "iDate", "iMouse",
        "iChannel0", "iChannel1", "iTransform"
    };
    for (auto& k : filter_keyword)
    {
        TextEditor::Identifier id;
        id.mDeclaration = "Shader input";
        lang.mPreprocIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    // init editor
    _editor.SetLanguageDefinition(lang);
    _editor.SetHandleKeyboardInputs(true);
    _editor.SetShowWhitespaces(false);
    _editor.SetText("");
    _editor.SetReadOnly(true);
    _editor.SetColorizerEnable(false);

    // status
    status_ = "-";
}

void ShaderEditWindow::setVisible(bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (on && Settings::application.widget.shader_editor_view != Settings::application.current_view)
        Settings::application.widget.shader_editor_view = -1;

    Settings::application.widget.shader_editor = on;
}

bool ShaderEditWindow::Visible() const
{
    return ( Settings::application.widget.shader_editor
             && (Settings::application.widget.shader_editor_view < 0 || Settings::application.widget.shader_editor_view == Settings::application.current_view )
             );
}

void ShaderEditWindow::BuildShader()
{
    // if the UI has a current clone, and ref to code for current clone is valid
    if (current_ != nullptr &&  filters_.find(current_) != filters_.end()) {

        // set the code of the current filter
        if (Settings::application.recentShaderCode.path.empty()
            || !SystemToolkit::file_exists(Settings::application.recentShaderCode.path)) {
            filters_[current_].setCode({_editor.GetText(), ""});
            filters_[current_].setName("Custom"); // TODO SET BY FILENAME
        }
        else {
            filters_[current_].setCode({Settings::application.recentShaderCode.path, ""});
            filters_[current_].setName(SystemToolkit::base_filename(Settings::application.recentShaderCode.path));
        }

        // set parameters TODO VERIFY IT WORKS FOR LOADED FILE
        filters_[current_].setParameters(current_->program().parameters());

        // change the filter of the current image filter
        // => this triggers compilation of shader
        compilation_ = new std::promise<std::string>();
        current_->setProgram( filters_[current_], compilation_ );
        compilation_return_ = compilation_->get_future();

        // inform status
        status_ = "Building...";
        Refresh();
    }
}

void ShaderEditWindow::Render()
{
    static DialogToolkit::OpenFileDialog selectcodedialog("Open GLSL shader code",
                                                          "Text files",
                                                          {"*.glsl", "*.fs", "*.txt"} );
    static DialogToolkit::SaveFileDialog exportcodedialog("Save GLSL shader code",
                                                          "Text files",
                                                          {"*.glsl", "*.fs", "*.txt"} );

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin(name_, &Settings::application.widget.shader_editor,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    std::string file_to_open_ = "";
    Source *cs = Mixer::manager().currentSource();

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.shader_editor = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_SHADEREDITOR))
        {
            // Menu entry to allow creating a custom filter
            if (ImGui::MenuItem(ICON_FA_SHARE_SQUARE "  Clone & Custom shader",
                                nullptr, nullptr, cs != nullptr)) {
                CloneSource *filteredclone = Mixer::manager().createSourceClone();
                filteredclone->setFilter(FrameBufferFilter::FILTER_IMAGE);
                Mixer::manager().addSource ( filteredclone );
                UserInterface::manager().showPannel(  Mixer::manager().numSource() );
            }
            ImGui::Separator();

            // Enable/Disable editor options
            ImGui::MenuItem( ICON_FA_UNDERLINE "  Show Shader Inputs", nullptr, &show_shader_inputs_);
            bool ws = _editor.IsShowingWhitespaces();
            if (ImGui::MenuItem( ICON_FA_ELLIPSIS_H "  Show whitespace", nullptr, &ws))
                _editor.SetShowWhitespaces(ws);

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.shader_editor_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.shader_editor_view = Settings::application.current_view;
                else
                    Settings::application.widget.shader_editor_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_SHADEREDITOR) )
                Settings::application.widget.shader_editor = false;

            ImGui::EndMenu();
        }

        std::string active_code_ = LABEL_SHADER_EMBEDDED;
        if (!Settings::application.recentShaderCode.path.empty())
            active_code_ = ICON_FA_FILE_CODE "  " + SystemToolkit::filename(Settings::application.recentShaderCode.path);

        // Code and shader file menu
        if (ImGui::BeginMenu(active_code_.c_str(), current_ != nullptr)) {

            // Selection of embedded shader code
            if (ImGui::MenuItem(LABEL_SHADER_EMBEDDED, NULL,
                                Settings::application.recentShaderCode.path.empty())) {
                // cancel path of recent shader
                Settings::application.recentShaderCode.assign("");
                // build with code
                BuildShader();
            }

            // Selection of an external shader file
            if (!Settings::application.recentShaderCode.filenames.empty()) {
                for (auto filename = Settings::application.recentShaderCode.filenames.begin();
                     filename != Settings::application.recentShaderCode.filenames.end();
                     filename++) {
                    const std::string label = ICON_FA_FILE_CODE "  "
                                              + SystemToolkit::filename(*filename);
                    const bool selected = filename->compare(
                                              Settings::application.recentShaderCode.path)
                                          == 0;
                    if (ImGui::MenuItem(label.c_str(), NULL, selected)) {
                        file_to_open_ = *filename;
                    }
                }
            }

            ImGui::Separator();
            // Open dialog to select an external file to be added to the list
            if (ImGui::MenuItem(LABEL_SHADER_ADD))
                selectcodedialog.open();
            // Open dialog to save the current code as a file, added to the list
            if (ImGui::MenuItem(LABEL_SHADER_SAVE))
                exportcodedialog.open();

            ImGui::EndMenu();
        }

        // Edit menu
        bool ro = _editor.IsReadOnly();
        if (ImGui::BeginMenu(ICON_FA_LAPTOP_CODE "  Edit", current_ != nullptr)) {

            // Menu section for presets and examples
            if (ImGui::BeginMenu(ICON_FA_SCROLL " Examples", current_ != nullptr)) {
                for (auto p = FilteringProgram::presets.begin();
                     p != FilteringProgram::presets.end();
                     ++p) {
                    if (ImGui::MenuItem(p->name().c_str())) {
                        // change the filter of the current image filter
                        // => this triggers compilation of shader
                        compilation_ = new std::promise<std::string>();
                        current_->setProgram(*p, compilation_);
                        compilation_return_ = compilation_->get_future();
                        // inform status
                        status_ = "Building...";
                        Refresh();
                        // cancel path of recent shader
                        Settings::application.recentShaderCode.assign("");
                    }
                }

                ImGui::Separator();
                // Open browser to vimix wiki doc
                if (ImGui::MenuItem( ICON_FA_EXTERNAL_LINK_ALT " Documentation") )
                    SystemToolkit::open(
                        "https://github.com/brunoherbelin/vimix/wiki/"
                        "Filters-and-ShaderToy#custom-filter-with-shadertoy-glsl-coding");
                // Open browser to shadertoy website
                if (ImGui::MenuItem( ICON_FA_EXTERNAL_LINK_ALT " Shadertoy.com"))
                    SystemToolkit::open("https://www.shadertoy.com/");

                ImGui::EndMenu();
            }

            // Menu item to synch code with GPU
            if (ImGui::MenuItem( ICON_FA_SYNC "  Sync", nullptr, nullptr, current_ != nullptr))
                Refresh();

            // standard Edit menu actions
            ImGui::Separator();
            if (ImGui::MenuItem( MENU_UNDO, SHORTCUT_UNDO, nullptr, !ro && _editor.CanUndo()))
                _editor.Undo();
            if (ImGui::MenuItem( MENU_REDO, CTRL_MOD "Y", nullptr, !ro && _editor.CanRedo()))
                _editor.Redo();
            if (ImGui::MenuItem( MENU_DELETE, SHORTCUT_DELETE, nullptr, !ro && _editor.HasSelection()))
                _editor.Delete();
            if (ImGui::MenuItem( MENU_CUT, SHORTCUT_CUT, nullptr, !ro && _editor.HasSelection()))
                _editor.Cut();
            if (ImGui::MenuItem( MENU_COPY, SHORTCUT_COPY, nullptr, _editor.HasSelection()))
                _editor.Copy();
            if (ImGui::MenuItem( MENU_PASTE, SHORTCUT_PASTE, nullptr, !ro && ImGui::GetClipboardText() != nullptr))
                _editor.Paste();
            if (ImGui::MenuItem( MENU_SELECTALL, SHORTCUT_SELECTALL, nullptr, _editor.GetText().size() > 1 ))
                _editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(_editor.GetTotalLines(), 0));

            ImGui::EndMenu();
        }

        // Build action menu
        if (ImGui::MenuItem( ICON_FA_HAMMER " Build", CTRL_MOD "B", nullptr, current_ != nullptr ))
            BuildShader();

        ImGui::EndMenuBar();
    }

    // garbage collection
    if ( Mixer::manager().session()->numSources() < 1 )
    {
        filters_.clear();
        current_ = nullptr;
        _editor.SetText("");
    }


    // File dialog Export code gives a filename to save to
    if (exportcodedialog.closed() && !exportcodedialog.path().empty()) {
        // Open the file
        std::ofstream file(exportcodedialog.path());

        // Save content to file
        if (file.is_open())
            file << _editor.GetText();

        // Close the file
        file.close();

        // set shader code by file
        file_to_open_ = exportcodedialog.path();
    }

    // File dialog select code gives a filename to open
    if (selectcodedialog.closed() && !selectcodedialog.path().empty()) {
        // set shader code by file
        file_to_open_ = selectcodedialog.path();
    }

    if ( !file_to_open_.empty() ) {

        // link to file
        Settings::application.recentShaderCode.push(file_to_open_);
        Settings::application.recentShaderCode.assign(file_to_open_);

        // read and display content
        std::string filecontent = SystemToolkit::get_text_content(file_to_open_);

        // TODO = ACTIONS BUILD should save the file
        if (!filecontent.empty()) {
            // replace text of editor
            _editor.SetText(filecontent);
            _editor.SetReadOnly(false);
            _editor.SetColorizerEnable(true);
        }

        // build with new code
        BuildShader();
    }


    // if compiling, cannot change source nor do anything else
    static std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
    if (compilation_ != nullptr && compilation_return_.wait_for(timeout) == std::future_status::ready )
    {
        // get message returned from compilation
        status_ = compilation_return_.get();

        // end compilation promise
        delete compilation_;
        compilation_ = nullptr;
    }
    // not compiling
    else {

        ImageFilter *i = nullptr;
        // if there is a current source
        if (cs != nullptr) {
            CloneSource *c = dynamic_cast<CloneSource *>(cs);
            // if the current source is a clone
            if ( c != nullptr ) {
                FrameBufferFilter *f = c->filter();
                // if the filter seems to be an Image Filter
                if (f != nullptr && f->type() == FrameBufferFilter::FILTER_IMAGE ) {
                    i = dynamic_cast<ImageFilter *>(f);
                    // if we can access the code of the filter
                    if (i != nullptr) {
                        // if the current clone was not already registered
                        if ( filters_.find(i) == filters_.end() )
                            // remember code for this clone
                            filters_[i] = i->program();
                    }
                }
            }
            // there is a current source, and it is not a filter
            if (i == nullptr) {
                status_ = "-";
                _editor.SetText("");
                _editor.SetReadOnly(true);
                current_ = nullptr;
            }
        }
        else
            status_ = "-";

        // change editor text only if current changed
        if ( current_ != i )
        {
            // get the editor text and remove trailing '\n'
            std::string code = _editor.GetText();
            code = code.substr(0, code.size() -1);

            // remember this code as buffered for the filter of this source
            filters_[current_].setCode( { code, "" } );

            // if switch to another shader code
            if ( i != nullptr ) {
                std::string c = filters_[i].code().first;
                if (SystemToolkit::file_exists(c)) {
                    Settings::application.recentShaderCode.push(c);
                    Settings::application.recentShaderCode.assign(c);
                    c = SystemToolkit::get_text_content(c);
                }
                else
                    Settings::application.recentShaderCode.assign("");

                // change editor
                _editor.SetText( c );
                _editor.SetReadOnly(false);
                _editor.SetColorizerEnable(true);
                status_ = "Ready";
            }
            // cancel edit clone
            else {
                // possibility that source was removed
                // disable editor
                _editor.SetReadOnly(true);
                _editor.SetColorizerEnable(false);
                status_ = "-";
            }
            // current changed
            current_ = i;
        }

    }

    // render status message
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
    ImGui::Text("Status: %s", status_.c_str());

    // render filename and file close button
    if (!Settings::application.recentShaderCode.path.empty()) { // if current is linked file
        const float w = ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight();

        // right-aligned filename, in italics and greyed out
        ImVec2 txtsize = ImGui::CalcTextSize(Settings::application.recentShaderCode.path.c_str(), NULL);
        ImGui::SameLine(w - txtsize.x - IMGUI_SAME_LINE, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 0.9f));
        ImGui::Text("%s", Settings::application.recentShaderCode.path.c_str());
        ImGui::PopStyleColor(1);

        // top right X icon to close the file
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
        ImGui::SameLine(w, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton(ICON_FA_TIMES, "Close file")) {
            // remove the filename from list of menu
            Settings::application.recentShaderCode.remove(
                Settings::application.recentShaderCode.path);
            // assign a non-filename to path and rebuild from existing code as embeded
            Settings::application.recentShaderCode.assign("");
            BuildShader();
        }
        ImGui::PopStyleVar(1);
    }

    ImGui::PopFont();

    // render the window content in mono font
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);

    // render shader input
    if (show_shader_inputs_) {
        ImGuiTextBuffer info;
        info.append(FilteringProgram::getFilterCodeInputs().c_str());

        // Show info text bloc (multi line, dark background)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::InputTextMultiline("##Info", (char *)info.c_str(), info.size(), ImVec2(-1, 8*ImGui::GetTextLineHeightWithSpacing()), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(2);

        // sliders iMouse
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##iMouse.x",
                           &FilteringProgram::iMouse.x, 0.f,
                           Mixer::manager().session()->frame()->width(), "iMouse.x %.f");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##iMouse.y",
                           &FilteringProgram::iMouse.y, 0.f,
                           Mixer::manager().session()->frame()->height(), "iMouse.y %.f");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##iMouse.z",
                           &FilteringProgram::iMouse.z, 0.f, 1.f, "iMouse.z %.2f");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##iMouse.w",
                           &FilteringProgram::iMouse.w, 0.f, 1.f, "iMouse.w %.2f");

    }
    else
        ImGui::Spacing();

    // the TextEditor captures keyboard focus from the main imgui context
    // so UserInterface::handleKeyboard cannot capture this event:
    // reading key press before render bypasses this problem
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl) {
        // special case for 'CTRL + B' keyboard shortcut
        if (ImGui::IsKeyPressed(io.KeyMap[ImGuiKey_A] + 1))
            BuildShader();
        // special case for 'CTRL + S' keyboard shortcut
        if (ImGui::IsKeyPressed(io.KeyMap[ImGuiKey_V] - 3)) {
            BuildShader();
            Mixer::manager().save();
        }
    }

    // render main editor
    _editor.Render("Shader Editor");

    ImGui::PopFont();

    ImGui::End();
}

void ShaderEditWindow::Refresh()
{
    // force reload
    if ( current_ != nullptr )
        filters_.erase(current_);
    current_ = nullptr;
}
