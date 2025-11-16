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

#include "MediaSource.h"
#include <string>
#include <regex>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

// ImGui
#include "ImGuiToolkit.h"
#include "imgui_internal.h"

#include <GLFW/glfw3.h>

#include "defines.h"
#include "BaseToolkit.h"
#include "Settings.h"
#include "Source.h"
#include "Mixer.h"
#include "SourceCallback.h"
#include "ControlManager.h"
#include "Metronome.h"
#include "MediaPlayer.h"

#include "InputMappingWindow.h"

InputMappingWindow::InputMappingWindow() : WorkspaceWindow("InputMappingInterface")
{
    input_mode = { ICON_FA_KEYBOARD "  Keyboard",
                   ICON_FA_CALCULATOR "   Numpad" ,
                   ICON_FA_TABLET_ALT "   TouchOSC" ,
                   ICON_FA_GAMEPAD "  Gamepad",
                   ICON_FA_CLOCK "   Timer"  };
    current_input_for_mode = { INPUT_KEYBOARD_FIRST, INPUT_NUMPAD_FIRST, INPUT_MULTITOUCH_FIRST, INPUT_JOYSTICK_FIRST, INPUT_TIMER_FIRST };
    current_input_ = current_input_for_mode[Settings::application.mapping.mode];
}

void InputMappingWindow::setVisible(bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (Settings::application.widget.inputs_view > 0 && Settings::application.widget.inputs_view != Settings::application.current_view) {
        Settings::application.widget.inputs_view = -1;
        on = true;
    }

    Settings::application.widget.inputs = on;
}

bool InputMappingWindow::Visible() const
{
    return ( Settings::application.widget.inputs
             && (Settings::application.widget.inputs_view < 0 || Settings::application.widget.inputs_view == Settings::application.current_view )
             );
}

///
/// Draw a combo box listing all sources and all batch of the current session
/// Returns a Target variant : non-assigned by default (std::monostate), or a Source, or a Batch
/// If current element is indicated, it is displayed first
///
Target InputMappingWindow::ComboSelectTarget(const Target &current)
{
    Target selected;
    std::string label = "Select";

    // depending on variant, fill the label of combo
    if (current.index() > 0) {
        if (Source * const* v = std::get_if<Source *>(&current)) {
            label = std::string((*v)->initials()) + " - " + (*v)->name();
        }
        else if ( const size_t* v = std::get_if<size_t>(&current)) {
            label = std::string("Batch #") + std::to_string(*v);
        }
    }

    if (ImGui::BeginCombo("##ComboSelectSource", label.c_str()) )
    {
        Session *ses = Mixer::manager().session();
        for (auto sit = ses->begin(); sit != ses->end(); ++sit) {
            label = std::string((*sit)->initials()) + " - " + (*sit)->name();
            if (ImGui::Selectable( label.c_str() )) {
                selected = *sit;
            }
        }
        for (size_t b = 0; b < ses->numBatch(); ++b){
            label = std::string("Batch #") + std::to_string(b);
            if (ImGui::Selectable( label.c_str() )) {
                selected = b;
            }
        }

        ImGui::EndCombo();
    }

    return selected;
}

uint InputMappingWindow::ComboSelectCallback(uint current, bool imageprocessing, bool ismediaplayer)
{
    const char* callback_names[24] = { "Select",
                                       ICON_FA_BULLSEYE "  Alpha",
                                       ICON_FA_BULLSEYE "  Loom",
                                       ICON_FA_OBJECT_UNGROUP "  Geometry",
                                       ICON_FA_OBJECT_UNGROUP "  Grab",
                                       ICON_FA_OBJECT_UNGROUP "  Resize",
                                       ICON_FA_OBJECT_UNGROUP "  Turn",
                                       ICON_FA_LAYER_GROUP "  Depth",
                                       ICON_FA_PLAY_CIRCLE "  Play",
                                       ICON_FA_PLAY_CIRCLE "  Speed",
                                       ICON_FA_PLAY_CIRCLE "  Fast forward",
                                       ICON_FA_PLAY_CIRCLE "  Seek",
                                       ICON_FA_PLAY_CIRCLE "  Flag",
                                       "  None",
                                       "  None",
                                       "  None",
                                       ICON_FA_PALETTE "  Gamma",
                                       ICON_FA_PALETTE "  Brightness",
                                       ICON_FA_PALETTE "  Contrast",
                                       ICON_FA_PALETTE "  Saturation",
                                       ICON_FA_PALETTE "  Hue",
                                       ICON_FA_PALETTE "  Threshold",
                                       ICON_FA_PALETTE "  Invert",
                                       "  None"
    };

    uint selected = 0;
    if (ImGui::BeginCombo("##ComboSelectCallback", callback_names[current]) ) {
        for (uint i = SourceCallback::CALLBACK_ALPHA; 
            i <= (ismediaplayer ? SourceCallback::CALLBACK_FLAG : SourceCallback::CALLBACK_PLAY) ; ++i){
            if ( ImGui::Selectable( callback_names[i]) ) {
                selected = i;
            }
        }
        if (imageprocessing) {
            for (uint i = SourceCallback::CALLBACK_GAMMA; i <= SourceCallback::CALLBACK_INVERT; ++i){
                if ( ImGui::Selectable( callback_names[i]) ) {
                    selected = i;
                }
            }
        }
        ImGui::EndCombo();
    }

    return selected;
}

struct ClosestIndex
{
    int index;
    float val;
    ClosestIndex (float v) { val = v; index = 0; }
    void operator()(float v) { if (v < val) ++index; }
};

void InputMappingWindow::SliderParametersCallback(SourceCallback *callback, const Target &target)
{
    const float right_align = -1.05f * ImGui::GetTextLineHeightWithSpacing();
    static const char *press_tooltip[3] = {"Key Press\nApply value on key press",
                              "Key Down\nApply value on key down,\nrevert on key up",
                              "Repeat\nMaintain key down to repeat and iterate" };
    static std::vector< std::pair<int,int> > speed_icon = { {18,15}, {17,15}, {16,15}, {15,15}, {14,15} };
    static std::vector< std::string > speed_tooltip = { "Fastest\n0 ms", "Fast\n60 ms", "Smooth\n120 ms", "Slow\n240 ms", "Slowest\n500 ms" };
    static std::vector< float > speed_values = { 0.f, 60.f, 120.f, 240.f, 500.f };

    switch ( callback->type() ) {
    case SourceCallback::CALLBACK_ALPHA:
    {
        SetAlpha *edited = static_cast<SetAlpha*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat("##CALLBACK_ALPHA", &val, -1.f, 1.f, "%.2f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Alpha value to set if the source is\nvisible (1.0), transparent (0.0),\nor innactive (-1.0)", 18, 12);

    }
        break;
    case SourceCallback::CALLBACK_LOOM:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Loom *edited = static_cast<Loom*>(callback);
        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat("##CALLBACK_LOOM", &val, -1.f, 1.f, "%.2f", 2.f))
            edited->setValue(val);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Increment alpha to make the source more visible (>0) or more transparent (<0)", 19, 12);
    }
        break;
    case SourceCallback::CALLBACK_GEOMETRY:
    {
        SetGeometry *edited = static_cast<SetGeometry*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        if (target.index() > 0) {
            // 1. Case of variant as Source pointer
            if (Source * const* v = std::get_if<Source *>(&target)) {
                // Button to capture the source current geometry
                ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
                if (ImGui::Button("Capture", ImVec2(right_align, 0))) {
                    edited->setTarget( (*v)->group(View::GEOMETRY) );
                }
            }
            // 2. Case of variant as index of batch
            else if ( const size_t* v = std::get_if<size_t>(&target)) {

                std::vector<SourceIdList> _batch = Mixer::manager().session()->getAllBatch();

                std::string label = "Capture";
                // Combo box to set which source to capture
                if (ImGui::BeginCombo("##ComboSelectGeometryCapture", label.c_str()) )
                {
                    if ( *v < _batch.size() )
                    {
                        for (auto sid = _batch[*v].begin(); sid != _batch[*v].end(); ++sid){
                            SourceList::iterator sit = Mixer::manager().session()->find(*sid);
                            if ( sit != Mixer::manager().session()->end() ) {

                                label = std::string((*sit)->initials()) + " - " + (*sit)->name();
                                // C to capture the source current geometry
                                if (ImGui::Selectable( label.c_str() )) {
                                    edited->setTarget( (*sit)->group(View::GEOMETRY) );
                                }
                            }
                        }
                    }

                    ImGui::EndCombo();
                }
            }

            ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
            ImGuiToolkit::Indication("Capture source geometry to restore it later (position, scale and rotation).", 1, 16);
        }
        else {

            ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
            ImGui::TextDisabled("Invalid");
        }
    }
        break;

    case SourceCallback::CALLBACK_GRAB:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Grab *edited = static_cast<Grab*>(callback);
        float val[2] = {edited->value().x, edited->value().y};
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat2("##CALLBACK_GRAB", val, -2.f, 2.f, "%.2f"))
            edited->setValue( glm::vec2(val[0], val[1]));
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Increment vector (x,y) to move the source horizontally and vertically.", 6, 15);
    }
        break;

    case SourceCallback::CALLBACK_RESIZE:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Resize *edited = static_cast<Resize*>(callback);
        float val[2] = {edited->value().x, edited->value().y};
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat2("##CALLBACK_RESIZE", val, -2.f, 2.f, "%.2f"))
            edited->setValue( glm::vec2(val[0], val[1]));
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Increment vector (x,y) to scale the source horizontally and vertically.", 2, 15);

    }
        break;

    case SourceCallback::CALLBACK_TURN:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Turn *edited = static_cast<Turn*>(callback);
        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        if ( ImGui::SliderAngle("##CALLBACK_TURN", &val, -180.f, 180.f) )
            edited->setValue(val );

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Rotation speed (\u00B0/s) to turn the source clockwise (>0) or counterclockwise (<0)", 18, 9);
    }
        break;

    case SourceCallback::CALLBACK_DEPTH:
    {
        SetDepth *edited = static_cast<SetDepth*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_DEPTH", &val, 11.9f, 0.1f, "%.1f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Depth value to place the source front (12) or back (0) in the scene.", 11, 16);
    }
        break;

    case SourceCallback::CALLBACK_PLAY:
    {
        Play *edited = static_cast<Play*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int val = edited->value() ? 1 : 0;
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderInt("##CALLBACK_PLAY", &val, 0, 1, "Pause  |   Play "))
            edited->setValue(val>0);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Play or pause the source.", 12, 7);
    }
        break;

    case SourceCallback::CALLBACK_PLAYSPEED:
    {
        PlaySpeed *edited = static_cast<PlaySpeed*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_PLAYSPEED", &val, -10.f, 10.f, UNICODE_MULTIPLY " %.2f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Factor to multiply the playback speed of a video source.", 0, 12);
    }
        break;

    case SourceCallback::CALLBACK_PLAYFFWD:
    {
        PlayFastForward *edited = static_cast<PlayFastForward*>(callback);

        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int val = (int) edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderInt("##CALLBACK_PLAYFFWD", &val, 30, 1000, "%d ms"))
            edited->setValue( MAX(1, val) );

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Step increment (in miliseconds) to jump fast-forward in a video source.", 13, 7);
    }
        break;

    case SourceCallback::CALLBACK_SEEK:
    {
        Seek *edited = static_cast<Seek*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);
        
        // Text input field for MM:SS:MS seek target time
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        guint64 duration = GST_SECOND * 1000;
        if (Source * const* v = std::get_if<Source *>(&target)) {
            MediaSource *ms = dynamic_cast<MediaSource*>(*v);
            if (ms) 
                duration = ms->mediaplayer()->timeline()->duration();
        }

        static bool valid = false;
        guint64 target_time = edited->value();
        if ( ImGuiToolkit::InputTime("##CALLBACK_SEEK", &target_time, duration, &valid) ){
            if (valid)
                edited->setValue( target_time );
        }

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Target time (HH:MM:SS.MS) to set where to jump to in a video source.", 15, 7);
    }
        break;

    case SourceCallback::CALLBACK_FLAG:
    {
        Flag *edited = static_cast<Flag*>(callback);

        ImGuiToolkit::Indication(press_tooltip[0], 2, 13);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int max = -1;
        if (Source * const* v = std::get_if<Source *>(&target)) {
            MediaSource *ms = dynamic_cast<MediaSource*>(*v);
            if (ms) 
                max = ms->mediaplayer()->timeline()->numFlags() - 1;
        }
        int val = MIN( (int) edited->value(), max);

        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderInt("##CALLBACK_PLAY_FLAG", &val, -1, max, val < 0 ? "Next Flag" : "Flag <%d>"))
            edited->setValue(val );

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Flag to jump to in a video source.", 12, 6);

    }
        break;

    case SourceCallback::CALLBACK_BRIGHTNESS:
    {
        SetBrightness *edited = static_cast<SetBrightness*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_BRIGHTNESS", &val, -1.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Brightness for color correction.", 5, 16);
    }
        break;

    case SourceCallback::CALLBACK_CONTRAST:
    {
        SetContrast *edited = static_cast<SetContrast*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_CONTRAST", &val, -1.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Contrast for color correction.", 5, 16);
    }
        break;

    case SourceCallback::CALLBACK_SATURATION:
    {
        SetSaturation *edited = static_cast<SetSaturation*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_SATURATION", &val, -1.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Saturation for color correction.", 9, 16);
    }
        break;

    case SourceCallback::CALLBACK_HUE:
    {
        SetHue *edited = static_cast<SetHue*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_HUE", &val, 0.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Hue shift for color correction.", 3, 4);
    }
        break;

    case SourceCallback::CALLBACK_THRESHOLD:
    {
        SetThreshold *edited = static_cast<SetThreshold*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_THRESHOLD", &val, 0.0, 1.0, val < 0.001 ? "None" : "%.2f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Threshold for color correction.", 5, 4);
    }
        break;

    case SourceCallback::CALLBACK_GAMMA:
    {
        SetGamma *edited = static_cast<SetGamma*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        glm::vec4 val = edited->value();
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::ColorEdit3("##CALLBACK_GAMMA Color", glm::value_ptr(val), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) )
            edited->setValue(val);
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_GAMMA Gamma", &val.w, 0.5f, 10.f, "%.2f", 2.f) )
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Set Gamma color correction.", 6, 4);
    }
        break;

    case SourceCallback::CALLBACK_INVERT:
    {
        SetInvert *edited = static_cast<SetInvert*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int val = (int) edited->value();
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::Combo("##CALLBACK_INVERT", &val, "None\0Color RGB\0Luminance\0"))
            edited->setValue( (float) val);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Invert mode for color correction.", 4, 4);
    }
        break;

    default:
        break;
    }
}


void InputMappingWindow::Render()
{
    const ImGuiContext& g = *GImGui;
    const ImVec2 keyItemSpacing = ImVec2(g.FontSize * 0.2f, g.FontSize * 0.2f);
    const ImVec2 keyLetterIconSize = ImVec2(g.FontSize * 1.9f, g.FontSize * 1.9f);
    const ImVec2 keyLetterItemSize = keyLetterIconSize + keyItemSpacing;
    const ImVec2 keyNumpadIconSize = ImVec2(g.FontSize * 2.4f, g.FontSize * 2.4f);
    const ImVec2 keyNumpadItemSize = keyNumpadIconSize + keyItemSpacing;
    const float  fixed_height = keyLetterItemSize.y * 5.f + g.Style.WindowBorderSize + g.FontSize + g.Style.FramePadding.y * 2.0f + keyItemSpacing.y;
    const float  inputarea_width = keyLetterItemSize.x * 5.f;

    ImGui::SetNextWindowPos(ImVec2(530, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1000, fixed_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(900, fixed_height), ImVec2(FLT_MAX, fixed_height));

    if ( !ImGui::Begin(name_, &Settings::application.widget.inputs,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // short variable to refer to session
    Session *S = Mixer::manager().session();

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.inputs = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_INPUT_MAPPING))
        {
            // Disable
            ImGui::MenuItem( ICON_FA_BAN "  Disable", nullptr, &Settings::application.mapping.disabled);

            // Clear all
            if ( ImGui::MenuItem( ICON_FA_BACKSPACE " Clear all" ) )
                S->clearInputCallbacks();

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.inputs_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.inputs_view = Settings::application.current_view;
                else
                    Settings::application.widget.inputs_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_INPUTS) )
                Settings::application.widget.inputs = false;

            ImGui::EndMenu();
        }

        // Selection of the keyboard mode
        if (ImGui::BeginMenu( input_mode[Settings::application.mapping.mode].c_str() ))
        {
            for (size_t i = 0; i < input_mode.size(); ++i) {
                if (ImGui::MenuItem(input_mode[i].c_str())) {
                    current_input_for_mode[Settings::application.mapping.mode] = current_input_;
                    Settings::application.mapping.mode = i;
                    current_input_ = current_input_for_mode[i];
                }
            }
            ImGui::EndMenu();
        }

        // Options for current key
        const std::string key = (current_input_ < INPUT_NUMPAD_LAST) ? "  Key " : "  ";
        const std::string keymenu = ICON_FA_ARROW_RIGHT + key + Control::manager().inputLabel(current_input_);
        if (ImGui::BeginMenu(keymenu.c_str()) )
        {
            if ( ImGui::MenuItem(ICON_FA_TIMES "  Reset", NULL, false, S->inputAssigned(current_input_) ) )
                // remove all source callback of this input
                S->deleteInputCallbacks(current_input_);

            if (ImGuiToolkit::BeginMenuIcon(4, 13, "Metronome",
                                 S->inputAssigned(current_input_) && Settings::application.mapping.mode < 4 ))
            {
                Metronome::Synchronicity sync = S->inputSynchrony(current_input_);
                bool active = sync == Metronome::SYNC_NONE;
                if (ImGuiToolkit::MenuItemIcon(5, 13, " Not synchronized", NULL, active )){
                    S->setInputSynchrony(current_input_, Metronome::SYNC_NONE);
                }
                active = sync == Metronome::SYNC_BEAT;
                if (ImGuiToolkit::MenuItemIcon(6, 13, " Sync to beat", NULL, active )){
                    S->setInputSynchrony(current_input_, Metronome::SYNC_BEAT);
                }
                active = sync == Metronome::SYNC_PHASE;
                if (ImGuiToolkit::MenuItemIcon(7, 13, " Sync to phase", NULL, active )){
                    S->setInputSynchrony(current_input_, Metronome::SYNC_PHASE);
                }
                ImGui::EndMenu();
            }

            std::list<uint> models = S->assignedInputs();
            if (models.empty())
                ImGui::TextDisabled(ICON_FA_COPY "  Copy from");
            else {
                if (ImGui::BeginMenu(ICON_FA_COPY "  Copy from", models.size() > 0) )
                {
                    for (auto m = models.cbegin(); m != models.cend(); ++m) {
                        if ( *m != current_input_ )   {
                            if ( ImGui::MenuItem( Control::inputLabel( *m ).c_str() ) ){
                                S->copyInputCallback( *m, current_input_);
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // current window draw parameters
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = window->DrawList;
    ImVec2 frame_top = ImGui::GetCursorScreenPos();

    // change mode if a key is pressed
    for (uint k = INPUT_KEYBOARD_FIRST; k < INPUT_TIMER_FIRST; ++k) {
        if (Control::manager().inputActive(k)) {
            if (k < INPUT_NUMPAD_FIRST)
                Settings::application.mapping.mode = 0;
            else if (k < INPUT_JOYSTICK_FIRST)
                Settings::application.mapping.mode = 1;
            else if (k > INPUT_JOYSTICK_LAST_AXIS)
                Settings::application.mapping.mode = 2;
            else if (k < INPUT_JOYSTICK_FIRST_AXIS)
                Settings::application.mapping.mode = 3;
        }
    }

    //
    // KEYBOARD
    //
    if ( Settings::application.mapping.mode == 0 ) {

        // Draw table of letter keys [A] to [Y]
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Header, 0.4f));

        for (uint ik = INPUT_KEYBOARD_FIRST; ik < INPUT_KEYBOARD_LAST; ++ik){
            int i = ik - INPUT_KEYBOARD_FIRST;
            // draw overlay on active keys
            if ( Control::manager().inputActive(ik) ) {
                ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( i % 5, i / 5);
                draw_list->AddRectFilled(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = ik;
            }
            // draw key button
            ImGui::PushID(ik);
            if (ImGui::Selectable(Control::manager().inputLabel(ik).c_str(), S->inputAssigned(ik), 0, keyLetterIconSize)) {
                current_input_ = ik;
                // TODO SET VAR current input assigned??
            }
            ImGui::PopID();

            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(ik) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_KEYBOARD", &ik, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(ik).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_KEYBOARD")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, ik);
                        // switch to this key
                        current_input_ = ik;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 5 elements in a row
            if ((i % 5) < 4) ImGui::SameLine();

            // Draw frame
            ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( i % 5, i / 5);
            if (ik == current_input_)
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

    }
    //
    // NUMPAD
    //
    else if ( Settings::application.mapping.mode == 1 ) {

        // custom layout of numerical keypad
        std::vector<uint> numpad_inputs = { INPUT_NUMPAD_FIRST+7, INPUT_NUMPAD_FIRST+8, INPUT_NUMPAD_FIRST+9, INPUT_NUMPAD_FIRST+11,
                                            INPUT_NUMPAD_FIRST+4, INPUT_NUMPAD_FIRST+5, INPUT_NUMPAD_FIRST+6, INPUT_NUMPAD_FIRST+12,
                                            INPUT_NUMPAD_FIRST+1, INPUT_NUMPAD_FIRST+2, INPUT_NUMPAD_FIRST+3, INPUT_NUMPAD_FIRST+13,
                                            INPUT_NUMPAD_FIRST+0, INPUT_NUMPAD_FIRST+10, INPUT_NUMPAD_FIRST+14 };

        // Draw table of keypad keys
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Header, 0.4f));

        for (size_t p = 0; p < numpad_inputs.size(); ++p){
            uint ik = numpad_inputs[p];
            ImVec2 iconsize = p == 12 ? keyNumpadIconSize + ImVec2(keyNumpadIconSize.x+ g.Style.ItemSpacing.x, 0.f) : keyNumpadIconSize;
            ImVec2 itemsize = p == 12 ? keyNumpadItemSize + ImVec2(keyNumpadItemSize.x+ g.Style.ItemSpacing.x, 0.f) : keyNumpadItemSize;
            // draw overlay on active keys
            if ( Control::manager().inputActive(ik) ) {
                ImVec2 pos = frame_top + itemsize * ImVec2( p % 4, p / 4);
                pos += p > 12 ? ImVec2(keyNumpadIconSize.x+ g.Style.ItemSpacing.x, 0.f) : ImVec2(0,0);
                draw_list->AddRectFilled(pos, pos + iconsize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = ik;
            }
            // draw key button
            ImGui::PushID(ik);
            if (ImGui::Selectable(Control::manager().inputLabel(ik).c_str(), S->inputAssigned(ik), 0, iconsize)) {
                current_input_ = ik;
            }
            ImGui::PopID();
            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(ik) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_NUMPAD", &ik, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(ik).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_NUMPAD")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, ik);
                        // switch to this key
                        current_input_ = ik;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 4 elements in a row
            if ((p % 4) < 3) ImGui::SameLine();

            // Draw frame
            ImVec2 pos = frame_top + itemsize * ImVec2( p % 4, p / 4);
            pos += p > 12 ? ImVec2(keyNumpadIconSize.x + g.Style.ItemSpacing.x, 0.f) : ImVec2(0,0);
            if (ik == current_input_)
                draw_list->AddRect(pos, pos + iconsize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else
                draw_list->AddRect(pos, pos + iconsize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

    }
    //
    // MULTITOUCH OSC
    //
    else if ( Settings::application.mapping.mode == 2 ) {

        // Draw table of TouchOSC buttons
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Header, 0.4f));

        const ImVec2 touch_bar_size = keyNumpadItemSize * ImVec2(0.65f, 0.2f);
        const ImVec2 touch_bar_pos  = keyNumpadItemSize * ImVec2(0.125f, 0.6f);

        for (size_t t = 0; t < INPUT_MULTITOUCH_COUNT; ++t){
            uint it = INPUT_MULTITOUCH_FIRST + t;
            ImVec2 pos = frame_top + keyNumpadItemSize * ImVec2( t % 4, t / 4);

            // draw overlay on active keys
            if ( Control::manager().inputActive(it) ) {
                draw_list->AddRectFilled(pos, pos + keyNumpadIconSize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = it;
            }

            // draw key button
            ImGui::PushID(it);
            if (ImGui::Selectable(" ", S->inputAssigned(it), 0, keyNumpadIconSize))
                current_input_ = it;
            ImGui::PopID();

            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(it) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_MULTITOUCH", &it, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(it).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MULTITOUCH")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, it);
                        // switch to this key
                        current_input_ = it;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 4 elements in a row
            if ((t % 4) < 3) ImGui::SameLine();

            // Draw frame
            if (it == current_input_)
                draw_list->AddRect(pos, pos + keyNumpadIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else
                draw_list->AddRect(pos, pos + keyNumpadIconSize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

            // Draw value bar
            ImVec2 prev = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos( pos + touch_bar_pos);
            ImGui::ProgressBar(Control::manager().inputValue(it), touch_bar_size, "");
            ImGui::SetCursorScreenPos( prev );

        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

    }
    //
    // JOYSTICK
    //
    else if ( Settings::application.mapping.mode == 3 ) {

        // custom layout of gamepad buttons
        std::vector<uint> gamepad_inputs = { INPUT_JOYSTICK_FIRST_BUTTON+11, INPUT_JOYSTICK_FIRST_BUTTON+13,
                                             INPUT_JOYSTICK_FIRST_BUTTON+6,
                                             INPUT_JOYSTICK_FIRST_BUTTON+2, INPUT_JOYSTICK_FIRST_BUTTON+3,

                                             INPUT_JOYSTICK_FIRST_BUTTON+14, INPUT_JOYSTICK_FIRST_BUTTON+12,
                                             INPUT_JOYSTICK_FIRST_BUTTON+7,
                                             INPUT_JOYSTICK_FIRST_BUTTON+0, INPUT_JOYSTICK_FIRST_BUTTON+1,

                                             INPUT_JOYSTICK_FIRST_BUTTON+4, INPUT_JOYSTICK_FIRST_BUTTON+9,
                                             INPUT_JOYSTICK_FIRST_BUTTON+8,
                                             INPUT_JOYSTICK_FIRST_BUTTON+10, INPUT_JOYSTICK_FIRST_BUTTON+5  };

        std::vector< std::string > gamepad_labels = {  ICON_FA_ARROW_UP,  ICON_FA_ARROW_DOWN,
                                                       ICON_FA_CHEVRON_CIRCLE_LEFT, "X", "Y",
                                                       ICON_FA_ARROW_LEFT, ICON_FA_ARROW_RIGHT,
                                                       ICON_FA_CHEVRON_CIRCLE_RIGHT, "A", "B",
                                                       "L1", "LT", ICON_FA_DOT_CIRCLE, "RT", "R1" };

        // Draw table of Gamepad Buttons
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Header, 0.4f));

        // CENTER text for button
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        for (size_t b = 0; b < gamepad_inputs.size(); ++b ){
            uint ig = gamepad_inputs[b];
            // draw overlay on active keys
            if ( Control::manager().inputActive(ig) ) {
                ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( b % 5, b / 5);
                draw_list->AddRectFilled(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = ig;
            }
            // draw key button
            ImGui::PushID(ig);
            if (ImGui::Selectable(gamepad_labels[b].c_str(), S->inputAssigned(ig), 0, keyLetterIconSize))
                current_input_ = ig;
            ImGui::PopID();

            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(ig) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_GAMEPAD", &ig, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(ig).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEPAD")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, ig);
                        // switch to this key
                        current_input_ = ig;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if ((b % 5) < 4) ImGui::SameLine();

            // Draw frame
            ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( b % 5, b / 5);
            if (ig == current_input_)
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else if ( b!= 2 && b!= 7 && b!=12 )
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

        }
        ImGui::PopStyleVar();
        ImGui::PopFont();

        // Table of Gamepad Axis
        const ImVec2 axis_top = frame_top + ImVec2(0.f, 3.f * keyLetterItemSize.y);
        const ImVec2 axis_item_size(inputarea_width / 2.f, (2.f * keyLetterItemSize.y) / 3.f);
        const ImVec2 axis_icon_size = axis_item_size - g.Style.ItemSpacing;
        const ImVec2 axis_bar_size = axis_icon_size * ImVec2(0.4f, 0.4f);
        ImVec2 axis_bar_pos(axis_icon_size.x * 0.5f, axis_icon_size.y *0.3f );

        // LEFT align for 3 axis on the left
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.09f, 0.5f));

        // define top left screen cursor position
        ImVec2 pos = axis_top;
        // Draw a little bar showing activity on the gamepad axis
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS), axis_bar_size);
        // Draw button to assign the axis to an action
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("LX", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS;
        // Draw frame around current gamepad axis
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( 0, axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+1), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("LY", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+1), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+1;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+1)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( 0, 2.f * axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+2), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("L2", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+2), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+2;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+2)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        ImGui::PopStyleVar();

        // RIGHT align for 3 axis on the right
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.91f, 0.5f));
        axis_bar_pos.x = g.Style.ItemSpacing.x;

        pos = axis_top + ImVec2( axis_item_size.x, 0.f);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+3), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("RX", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+3), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+3;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+3)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( axis_item_size.x, axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+4), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("RY", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+4), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+4;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+4)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( axis_item_size.x, 2.f * axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+5), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("R2", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+5), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+5;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+5)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        ImGui::PopStyleVar(2);

        // Done with color change
        ImGui::PopStyleColor(3);

    }
    //
    // TIMER
    //
    else if ( Settings::application.mapping.mode == 4 ) {

        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 circle_center = frame_top + (ImVec2(inputarea_width, inputarea_width) )/ 2.f;
        const float circle_radius = (inputarea_width - IMGUI_SAME_LINE) / 2.f;
        const glm::vec2 mpo = glm::vec2 (io.MousePos.x - circle_center.x, io.MousePos.y - circle_center.y);
        const float angle = - glm::orientedAngle( glm::normalize(mpo), glm::vec2(1.f,0.f));
        const float lenght = glm::length(mpo);
        const float cm = 0.03f ; // circle margin

        // color palette
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Header, 0.4f));

        // draw background ring
        static ImU32 colorbg = ImGui::GetColorU32(ImGuiCol_FrameBgActive, 0.6f);
        draw_list->AddCircleFilled(circle_center, circle_radius, colorbg, PLOT_CIRCLE_SEGMENTS);

        // metronome timer info
        char text_buf[24] = {0};
        const double q = Metronome::manager().quantum();
        static const float resolution = PLOT_CIRCLE_SEGMENTS / (2.f * M_PI);
        static ImVec2 buffer[PLOT_CIRCLE_SEGMENTS];

        // draw all slices of metronome ring
        for (uint ip = 0 ; ip < (uint) floor(q) ; ++ip) {
            float a0 = cm - M_PI_2 + (float(ip) * (2.f * M_PI)) / floor(q);
            float a1 = (-2.f * cm) + a0 + (2.f * M_PI) / floor(q);
            int n = ImMax(3, (int)((a1 - a0) * resolution));
            double da = (a1 - a0) / (n - 1);
            int index = 0;
            // start drawing at center point of slice
            float a01 =  (a0 + a1) / 2.f;
            buffer[index++] = ImVec2(circle_center.x + cm * circle_radius * cos(a01), circle_center.y + cm * circle_radius * sin(a01));
            // draw round external border of slice
            for (int i = 0; i < n; ++i) {
                double a = a0 + i * da;
                buffer[index++] = ImVec2(circle_center.x + circle_radius * cos(a), circle_center.y + circle_radius * sin(a));
            }
            // Test mouse over in slices of the circle
            // 1) test if mouse is inside area
            if (ImGui::IsMouseHoveringRect(frame_top, frame_top + ImVec2(inputarea_width, inputarea_width), true))
            {
                // 2) test angle of mouse coordinate in circle
                if (lenght < circle_radius && ( ( angle > a0 && angle < a1) || (angle + (2.f * M_PI) > a0 && angle + (2.f * M_PI) < a1) ) )
                {
                    // draw the mouse-over slice
                    draw_list->AddConvexPolyFilled(buffer, index, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
                    // indicate tempo of mouse-over slice
                    snprintf(text_buf, 24, "%d/%d", ip + 1, (int) floor(q) );

                    // mouse clic
                    if (ImGui::IsMouseClicked(0)) {
                        current_input_ = ip + INPUT_TIMER_FIRST;
                    }
                }
            }

            // draw the slice showing its assigned in this session
            if (S->inputAssigned(ip + INPUT_TIMER_FIRST))
                draw_list->AddConvexPolyFilled(buffer, index, ImGui::GetColorU32(ImGuiCol_Header));

            // draw the border of the slice
            if (ip + INPUT_TIMER_FIRST == current_input_)
                // current active slice has bold white border
                draw_list->AddPolyline(buffer, index, ImGui::GetColorU32(ImGuiCol_Text), true, 3.f);
            else
                draw_list->AddPolyline(buffer, index, ImGui::GetColorU32(ImGuiCol_Button), true, 0.5f);

        }

        // draw clock hand
        float a = -M_PI_2 + (Metronome::manager().phase()/q) * (2.f * M_PI);
        draw_list->AddLine(ImVec2(circle_center.x + cos(a), circle_center.y + sin(a)),
                           ImVec2(circle_center.x + circle_radius * cos(a), circle_center.y + circle_radius * sin(a)),
                           ImGui::GetColorU32(ImGuiCol_PlotHistogram), 2.f);

        // display text indication 'x / N' on mouse over in a central circle
        draw_list->AddCircleFilled(circle_center, circle_radius * 0.25f, ImGui::GetColorU32(ImGuiCol_Button, 10.f), PLOT_CIRCLE_SEGMENTS);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();

        ImGui::PopStyleColor(3);
    }

    // Draw child Window (right) to list reactions to input
    ImGui::SetCursorScreenPos(frame_top + g.Style.FramePadding + ImVec2(inputarea_width,0.f));
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, g.Style.ItemSpacing.y * 2.f) );
        ImGui::BeginChild("InputsMappingInterfacePanel", ImVec2(0, 0), false);
        float w = ImGui::GetWindowWidth() *0.25f;

        // selection of device for joystick mode
        if ( Settings::application.mapping.mode == 3 ){
            char text_buf[512];
            if ( glfwJoystickPresent( Settings::application.gamepad_id ) )
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", glfwGetJoystickName(Settings::application.gamepad_id));
            else
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "Joystick %d", Settings::application.gamepad_id);

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("Device", text_buf, ImGuiComboFlags_None)) {
                for( int g = GLFW_JOYSTICK_1; g < GLFW_JOYSTICK_LAST; ++g) {
                    if ( glfwJoystickPresent( g ) ) {
                        ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", glfwGetJoystickName(g));
                        if (ImGui::Selectable(text_buf, Settings::application.gamepad_id == g) ) {
                            Settings::application.gamepad_id = g;
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        // adding actions is possible only if there are sources in the session
        if (!Mixer::manager().session()->empty()) {

            ///
            /// list of input callbacks for the current input
            ///
            if (S->inputAssigned(current_input_)) {

                auto result = S->getSourceCallbacks(current_input_);
                for (auto kit = result.cbegin(); kit != result.cend(); ++kit) {

                    Target target = kit->first;
                    SourceCallback *callback = kit->second;

                    // push ID because we repeat the same UI
                    ImGui::PushID( (void*) callback);

                    // Delete interface
                    if (ImGuiToolkit::IconButton(ICON_FA_MINUS, "Remove") ){
                        S->deleteInputCallback(callback);
                        // reload
                        ImGui::PopID();
                        break;
                    }

                    // Select Target
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::SetNextItemWidth(w);
                    Target selected_target = ComboSelectTarget(target);
                    // if the selected target variant was filled with a value
                    if (selected_target.index() > 0) {
                        // reassign the callback to newly selected source
                        S->assignInputCallback(current_input_, selected_target, callback);
                        // reload
                        ImGui::PopID();
                        break;
                    }

                    // check if target is a Source with image processing enabled
                    bool ismediaplayer = false;
                    bool withimageprocessing = false;
                    if ( target.index() == 1 ) {
                        if (Source * const* v = std::get_if<Source *>(&target)) {
                            withimageprocessing = (*v)->imageProcessingEnabled();
                            ismediaplayer = dynamic_cast<MediaSource*>(*v) != nullptr;
                        }
                    }

                    // Select Reaction
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::SetNextItemWidth(w);
                    uint type = ComboSelectCallback( callback->type(), withimageprocessing, ismediaplayer );
                    if (type > 0) {
                        // remove previous callback
                        S->deleteInputCallback(callback);
                        // assign callback
                        S->assignInputCallback(current_input_, target, SourceCallback::create((SourceCallback::CallbackType)type) );
                        // reload
                        ImGui::PopID();
                        break;
                    }

                    // Adjust parameters
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (callback)
                        SliderParametersCallback( callback, target );

                    ImGui::PopID();

                }

            }
            else {
                ImGui::Text("No action mapped to this input. Add one with +.");
            }

            ///
            /// Add a new interface
            ///
            ///

            static bool temp_new_input = false;
            static Target temp_new_target;
            static uint temp_new_callback = 0;

            // step 1 : press '+'
            if (temp_new_input) {
                if (ImGuiToolkit::IconButton(ICON_FA_TIMES, "Cancel") ){
                    temp_new_target = std::monostate();
                    temp_new_callback = 0;
                    temp_new_input = false;
                }
            }
            else if (ImGuiToolkit::IconButton(ICON_FA_PLUS, "Add mapping") )
                temp_new_input = true;

            if (temp_new_input) {
                // step 2 : Get input for source
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth(w);

                Target selected_target = ComboSelectTarget(temp_new_target);
                // if the selected target variant was filled with a value
                if (selected_target.index() > 0) {
                    temp_new_target = selected_target;
                    temp_new_callback = 0;
                }
                // possible new target
                if (temp_new_target.index() > 0) {
                    // check if target is a Source with image processing enabled
                    bool mediaplayer = false;
                    bool withimageprocessing = false;
                    if ( temp_new_target.index() == 1 ) {
                        if (Source * const* v = std::get_if<Source *>(&temp_new_target)) {
                            withimageprocessing = (*v)->imageProcessingEnabled();
                            mediaplayer = dynamic_cast<MediaSource*>(*v) != nullptr;
                        }
                    }
                    // step 3: Get input for callback type
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::SetNextItemWidth(w);
                    temp_new_callback = ComboSelectCallback( temp_new_callback, withimageprocessing, mediaplayer );
                    // user selected a callback type
                    if (temp_new_callback > 0) {
                        // step 4 : create new callback and add it to source
                        S->assignInputCallback(current_input_, temp_new_target, SourceCallback::create((SourceCallback::CallbackType)temp_new_callback) );
                        // done
                        temp_new_target = std::monostate();
                        temp_new_callback = 0;
                        temp_new_input = false;
                    }
                }
            }
        }
        else {
            ImGui::Text("No source to perform an action.");
        }

        ///
        /// Sync info lower right corner
        ///
        Metronome::Synchronicity sync = S->inputSynchrony(current_input_);
        if ( sync > Metronome::SYNC_NONE) {
            ImGui::SetCursorPos(ImGui::GetWindowSize() - ImVec2(50, 50));
            ImGuiToolkit::Icon( sync > Metronome::SYNC_BEAT ? 7 : 6, 13);
        }

        // close child window
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }


    ImGui::End();
}

