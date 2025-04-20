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
#include "ShaderSource.h"
#include "DialogToolkit.h"
#include "UserInterfaceManager.h"

#include "ShaderEditWindow.h"

ImageFilter *getImageFilter(Source *s)
{
    ImageFilter *i = nullptr;

    // if s is a source
    if (s != nullptr) {
        CloneSource *c = dynamic_cast<CloneSource *>(s);
        // if s is a clone
        if (c != nullptr) {
            // it has a filter
            FrameBufferFilter *f = c->filter();
            // if the filter seems to be an Image Filter
            if (f != nullptr && f->type() == FrameBufferFilter::FILTER_IMAGE) {
                // cast to image filter
                i = dynamic_cast<ImageFilter *>(f);
            }
        }
        else {
            ShaderSource *sc = dynamic_cast<ShaderSource *>(s);
            // if s is a shader source
            if (sc != nullptr) {
                // it has an Image filter
                i = sc->filter();
            }
        }
    }

    return i;
}

void saveEditorText(const std::string &filename)
{
    std::ofstream file(filename);
    if (file.is_open()) {
        // get the editor text and remove trailing '\n'
        std::string code = _editor.GetText();
        code = code.substr(0, code.size() -1);
        file << code;
    }
    file.close();
}

///
/// SHADER EDITOR
///
///
ShaderEditWindow::ShaderEditWindow() : WorkspaceWindow("Shader"), _cs_id(0), current_(nullptr), show_shader_inputs_(false)
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

    // validate list for menu
    Settings::application.recentShaderCode.validate();

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

        // get the editor text and remove trailing '\n'
        std::string code = _editor.GetText();
        code = code.substr(0, code.size() -1);

        // set the code to the current content of editor
        filters_[current_].setCode({code, ""});

        // set parameters
        filters_[current_].setParameters(current_->program().parameters());

        // change the filter of the current image filter
        // => this triggers compilation of shader
        compilation_ = new std::promise<std::string>();
        current_->setProgram( filters_[current_], compilation_ );
        compilation_return_ = compilation_->get_future();

        // inform status
        status_ = "Building...";
    }
}

void ShaderEditWindow::BuildAll()
{
    // Loop over all sources
    for (auto s = Mixer::manager().session()->begin(); s != Mixer::manager().session()->end(); ++s) {
        // if image filter can be extracted from source
        ImageFilter *imf = getImageFilter(*s);
        if (imf != nullptr) {
            // rebuild by re-injecting same program
            imf->setProgram(imf->program());
        }
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

    std::string file_to_build_ = "";
    Source *cs = Mixer::manager().currentSource();

    // garbage collection
    if ( !filters_.empty() && Mixer::manager().session()->numSources() < 1 )
    {
        // empty list of edited filter when session empty
        filters_.clear();
        current_ = nullptr;

        // clear editor text and recent file
        _editor.SetText("");
        Settings::application.recentShaderCode.assign("");
    }

    // File dialog Export code gives a filename to save to
    if (exportcodedialog.closed() && !exportcodedialog.path().empty()) {

        // set shader program to be a file
        file_to_build_ = exportcodedialog.path();

        // save the current content of editor into given file
        saveEditorText(file_to_build_);
    }

    // File dialog select code gives a filename to open
    if (selectcodedialog.closed() && !selectcodedialog.path().empty()) {

        // set shader program to be a file
        file_to_build_ = selectcodedialog.path();

        // read file and display content in editor
        _editor.SetText(SystemToolkit::get_text_content(file_to_build_));
    }

    // Render window
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if ( !ImGui::Begin(name_, &Settings::application.widget.shader_editor,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
                          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    // AUTO COLLAPSE
    // setCollapsed(!ImGui::IsWindowFocused() || current_ == nullptr);
    setCollapsed(current_ == nullptr);

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.shader_editor = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_SHADEREDITOR))
        {
            // Menu entry to allow creating a custom filter
            if (ImGui::MenuItem(ICON_FA_SHARE_SQUARE "  Clone source & add shader",
                                nullptr, nullptr, cs != nullptr)) {
                CloneSource *filteredclone = Mixer::manager().createSourceClone();
                filteredclone->setFilter(FrameBufferFilter::FILTER_IMAGE);
                Mixer::manager().addSource ( filteredclone );
                UserInterface::manager().showPannel(  Mixer::manager().numSource() );
            }

            // Menu entry to rebuild all
            if (ImGui::MenuItem(ICON_FA_HAMMER "  Rebuild all"))
                BuildAll();

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
                filters_[current_].resetFilename();
                Settings::application.recentShaderCode.assign("");
                // build code
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
                        // set shader program to be a file
                        file_to_build_ = *filename;
                        // read file and display content in editor
                        _editor.SetText(SystemToolkit::get_text_content(file_to_build_));
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

                if (ImGui::BeginMenu("Filters", current_ != nullptr)) {
                    for (auto p = FilteringProgram::example_filters.begin();
                         p != FilteringProgram::example_filters.end();
                         ++p) {
                        if (ImGui::MenuItem(p->name().c_str())) {
                            // copy text code into editor
                            _editor.SetText( p->code().first );
                            // cancel path of recent shader
                            filters_[current_].resetFilename();
                            Settings::application.recentShaderCode.assign("");
                            // build code
                            BuildShader();
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Patterns", current_ != nullptr)) {
                    for (auto p = FilteringProgram::example_patterns.begin();
                         p != FilteringProgram::example_patterns.end();
                         ++p) {
                        if (ImGui::MenuItem(p->name().c_str())) {
                            // copy text code into editor
                            _editor.SetText( p->code().first );
                            // cancel path of recent shader
                            filters_[current_].resetFilename();
                            Settings::application.recentShaderCode.assign("");
                            // build code
                            BuildShader();
                        }
                    }
                    ImGui::EndMenu();
                }

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
            if (ImGui::MenuItem( ICON_FA_SYNC "  Sync", nullptr, nullptr, current_ != nullptr)) {
                if (filters_[current_].filename().empty())
                    Refresh();
                else
                    BuildShader();
            }

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
        if (ImGui::MenuItem( ICON_FA_HAMMER " Build", CTRL_MOD "B", nullptr, current_ != nullptr )) {

            // if present, save the program file with current content of editor
            if (!filters_[current_].filename().empty())
                saveEditorText(filters_[current_].filename());

            // build
            BuildShader();
        }

        ImGui::EndMenuBar();
    }

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
            // if present, save the program file with current content of editor
            if (!filters_[current_].filename().empty())
                saveEditorText(filters_[current_].filename());
            // save session
            Mixer::manager().save();
        }
    }

    // compile
    if (current_ != nullptr && !file_to_build_.empty()) {

        // ok editor
        _editor.SetReadOnly(false);
        _editor.SetColorizerEnable(true);

        // link to file
        filters_[current_].setFilename(file_to_build_);
        Settings::application.recentShaderCode.push(file_to_build_);
        Settings::application.recentShaderCode.assign(file_to_build_);

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
        // if there is a current source
        if (cs != nullptr) {
            // if current source is different from previous one
            if (cs->id() != _cs_id) {
                // if we can access the shader of that source
                ImageFilter *i = getImageFilter(cs);
                if (i != nullptr) {

                    // if the current clone was not already registered
                    if (filters_.find(i) == filters_.end()) {
                        // remember program for this image filter
                        filters_[i] = i->program();
                        // set a name to the filter
                        filters_[i].setName(cs->name());
                    }

                    // keep unsaved code of current editor
                    if (current_ != nullptr) {
                        // get the editor text and remove trailing '\n'
                        std::string code = _editor.GetText();
                        code = code.substr(0, code.size() -1);
                        // remember this code as buffered for this filter
                        filters_[current_].setCode( { code, "" } );
                    }

                    // set current shader code menu (can be empty for embedded)
                    Settings::application.recentShaderCode.push(filters_[i].filename());
                    Settings::application.recentShaderCode.assign(filters_[i].filename());

                    // change editor
                    _editor.SetText( filters_[i].code().first );
                    _editor.SetReadOnly(false);
                    _editor.SetColorizerEnable(true);
                    status_ = "Ready";

                    // change current
                    current_ = i;
                }
                // there is a current source, and it is not a shader
                else {
                    _editor.SetText("");
                    _editor.SetReadOnly(true);
                    Settings::application.recentShaderCode.assign("");
                    status_ = "-";
                    // reset current
                    current_ = nullptr;
                }

                // keep track of source currently shown to do that only on change
                _cs_id = cs->id();
            }
        }
        // no current source
        else if (_cs_id > 0) {
            _editor.SetText("");
            _editor.SetReadOnly(true);
            Settings::application.recentShaderCode.assign("");
            // reset current
            current_ = nullptr;

            // keep track that no source currently shown to do this once only
            _cs_id = 0;
            status_ = "-";
        }
    }

    // render status message
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
    ImGui::Text("Status: %s", status_.c_str());

    // Right-align on same line than status
    const float w = ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight();

    // Display name of program for embedded code
    if (current_ != nullptr) {

        ImVec2 txtsize = ImGui::CalcTextSize(Settings::application.recentShaderCode.path.c_str(), NULL);
        ImGui::SameLine(w - txtsize.x - IMGUI_SAME_LINE, 0);

        if (filters_[current_].filename().empty()) {
            // right-aligned in italics and greyed out
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 0.9f));
            ImGui::Text("%s", Settings::application.recentShaderCode.path.c_str());
            ImGui::PopStyleColor(1);
        }
        // or Display filename and close button for shaders from file
        else {
            // right-aligned in italics
            ImGui::Text("%s", Settings::application.recentShaderCode.path.c_str());

            // top right X icon to close the file
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
            ImGui::SameLine(w, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton(ICON_FA_TIMES, "Close file")) {
                // Test if there are other filters refering to this filename
                uint count_file = 0;
                for (auto const &fil : filters_) {
                    if (fil.first != nullptr &&
                        filters_[current_].filename().compare(fil.first->program().filename()) == 0)
                        count_file++;
                }
                // remove the filename from list of menu if was used by other filters
                if (count_file > 1)
                    Settings::application.recentShaderCode.remove(Settings::application.recentShaderCode.path);
                // unset filename for program
                filters_[current_].resetFilename();
                // assign a non-filename to path
                Settings::application.recentShaderCode.assign("");
                // rebuild from existing code as embeded
                BuildShader();
            }
            ImGui::PopStyleVar(1);
        }
    }
    else {
        ImVec2 txtsize = ImGui::CalcTextSize("No shader", NULL);
        ImGui::SameLine(w - txtsize.x - IMGUI_SAME_LINE, 0);
        // right-aligned in italics and greyed out
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8, 0.8, 0.8, 0.9f));
        ImGui::Text("No Shader");
        ImGui::PopStyleColor(1);
    }

    ImGui::PopFont();

    if (current_ != nullptr) {

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

        // render main editor
        _editor.Render("Shader Editor");

        ImGui::PopFont();
    }

    ImGui::End();
}

void ShaderEditWindow::Refresh()
{
    // force reload
    if ( current_ != nullptr )
        filters_.erase(current_);
    current_ = nullptr;
    _cs_id = 0;
}
