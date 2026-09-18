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

#include <string>

#include "NavigatorInternal.h"

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "IconsVimixImage.h"
#include "Settings.h"
#include "Log.h"
#include "Mixer.h"
#include "ActionManager.h"
#include "MediaPlayer.h"
#include "Transcoder.h"
#include "Upscaler.h"
#include "Source/MediaSource.h"
#include "Source/SourceCallback.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "Visitor/ImGuiVisitor.h"
#include "UserInterfaceManager.h"

#include "Navigator.h"
#include "NavigatorCodec.h"
#include "SourcePanel.h"

static bool renderTranscodingPanel(guint64 id, MediaPlayer *mp)
{
    static Transcoder *transcoder = nullptr;
    static guint64 transcode_id = 0;
    bool ret = false;

    if (mp == nullptr || mp->isImage())
        return ret;
        
    if (id != transcode_id && transcoder != nullptr) {
        // if source changed while transcoding;
        //    show a disabled transcoding panel
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));    
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.f,0.f,0.f,0.f));
        ImGui::CollapsingHeader("Transcoding", ImGuiTreeNodeFlags_Bullet);
        ImGui::PopStyleColor(3);
        return ret;
    }

    // Transcoding panel
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));
    Settings::application.pannel_source[2] = ImGui::CollapsingHeader("Transcoding",
                                                                      Settings::application.pannel_source[2] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor();

    if (Settings::application.pannel_source[2]) {

        // transcoding is done at the resolution of the source video: the user
        // preference is kept, but H265 is used locally if H264 cannot encode it
        const int source_width = (int) mp->width();
        const int source_height = (int) mp->height();
        int transcode_profile = Settings::application.transcode_options[1];

        // Transcoding options
        // Codec
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ComboCodec("##CodecTranscode", &transcode_profile, source_width, source_height))
            // the user selected a codec: change the preference
            Settings::application.transcode_options[1] = transcode_profile;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Codec")) {
            Settings::application.transcode_options[1] = GstToolkit::H264_RT;
            transcode_profile = GstToolkit::H264_RT;
            ValidateCodecResolution(&transcode_profile, source_width, source_height);
        }
        // Upscaling model, offered only when this build has the ncnn Vulkan
        // backend which performs the inference
        std::string transcode_upscaler = Settings::application.transcode_upscaler;
        if (Upscaler::available()) {
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ComboUpscaler("##UpscalerTranscode", &transcode_upscaler, source_width, source_height,
                              transcode_profile))
                // the user selected a model: change the preference
                Settings::application.transcode_upscaler = transcode_upscaler;
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton("Upscale"))
                Settings::application.transcode_upscaler = transcode_upscaler = Upscaler::NONE;
        }
        const int upscale_factor = Upscaler::model(transcode_upscaler).factor;

        ImGui::Spacing();
        // Keyframes
        bool force_keyframes = Settings::application.transcode_options[0] != 0;
        ImGuiToolkit::ButtonSwitch( "Backward playback", &force_keyframes,
        "Optimize for backward playback by adding keyframes", 
        transcoder == nullptr && transcode_profile != GstToolkit::JPEG_MULTI);
        Settings::application.transcode_options[0] = force_keyframes ? 1 : 0;
        // audio: upscaling splits the pipeline around the GPU inference,
        // which no audio stream can cross, so the switch is forced on
        bool force_no_audio = Settings::application.transcode_options[2] != 0 || upscale_factor > 1;
        ImGuiToolkit::ButtonSwitch( "Remove audio", &force_no_audio,
        upscale_factor > 1 ? "Audio tracks cannot be kept when upscaling"
                           : "Do not include audio tracks in produced video",
        transcoder == nullptr && transcode_profile != GstToolkit::JPEG_MULTI && upscale_factor < 2);
        if (upscale_factor < 2)
            Settings::application.transcode_options[2] = force_no_audio ? 1 : 0;

        // Start transcoding if not already started for current source
        if (transcoder == nullptr) {
            if (ImGui::Button(ICON_FA_COGS " Transcode", ImVec2(IMGUI_RIGHT_ALIGN,0))) {
                transcode_id = id;
                transcoder = new Transcoder(gst_uri_get_location(mp->uri().c_str()));
                TranscoderOptions transcode_options(
                    static_cast<GstToolkit::Profile>(transcode_profile),
                    force_keyframes,
                    force_no_audio,
                    transcode_upscaler
                );
                if (!transcoder->start(transcode_options)) {
                    Log::Warning("Failed to start transcoding: %s", transcoder->error().c_str());
                    delete transcoder;
                    transcoder = nullptr;
                    transcode_id = 0;
                }
            }
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Re-encode the source video using the specified codec and options.\n\n "
                    ICON_FA_FILM "  The new file will replace the one in the source "
                    "once transcoding is successfully completed. "
                    "The current file is left unchanged.\n\n "
                    ICON_FA_MAGIC "  An upscaling model enlarges every frame with a neural "
                    "network on the GPU. This is much slower than plain transcoding, and "
                    "the audio track is not kept.");
        }

        if (transcoder != nullptr) {
            if (transcoder->finished()) {
                if (transcoder->success()) {
                    Log::Notify("Transcoding successful : %s", transcoder->outputFilename().c_str());
                    // reload source with new file
                    Source *src = Mixer::manager().findSource(transcode_id);
                    if (src != nullptr) {
                        // create MultiFile source from generated jpeg
                        if (transcoder->isImageSequence()) {
                            std::list<std::string> files = SystemToolkit::list_directory(transcoder->outputFilename(), {"*.jpg", "*.jpeg", "*.png"});
                            Source *mfs = Mixer::manager().createSourceMultifile(files, 30);
                            // replace source in session by new multifile source
                            Mixer::manager().replaceSource(src, mfs);
                        }
                        // General case; change path of current media source
                        else 
                            static_cast<MediaSource*>(src)->setPath( transcoder->outputFilename() );
                    }
                    ret = true;
                }
                else
                    // upscaling reports its failures asynchronously: this is
                    // the only place the user would ever hear about them
                    Log::Warning("Transcoding failed : %s", transcoder->error().c_str());
                // all done in any case
                delete transcoder;
                transcoder = nullptr;
                transcode_id = 0;
            }
            else {
                float progress = transcoder->progress();
                // the status tells about the long preliminary steps of
                // upscaling (fetching the model, initializing the GPU)
                std::string status = transcoder->status();
                if (status.empty() && progress < EPSILON)
                    status = "working...";
                ImGui::ProgressBar(progress, ImVec2(IMGUI_RIGHT_ALIGN,0),
                                   status.empty() ? nullptr : status.c_str());
                ImGui::SameLine();
                if (ImGui::Button( ICON_FA_TIMES " Cancel", ImVec2(0,0)) ||
                    Mixer::manager().findSource(transcode_id) == nullptr ) {
                    // cancel transcoding by user or source removed
                    transcoder->stop();
                }
            }
        }
    }
    else {
        if (transcoder != nullptr && !transcoder->finished()) {
            ImVec2 pos_tmp = ImGui::GetCursorPos();
            ImVec2 space_size = ImGui::CalcTextSize(" Transcoding ", NULL);
            space_size.x += ImGui::GetTextLineHeightWithSpacing() * 2.f;
            space_size.y = -ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPos( pos_tmp + space_size );
            ImGui::Text("( %d %% )", (int)(100.0 * transcoder->progress()));
            ImGui::SetCursorPos( pos_tmp );
        }
    }

    return ret;
}

// Source pannel : *s was checked before
void SourcePanel::Render(Navigator *navigator, Source *s, const ImVec2 &iconsize, bool reset)
{
    // index of this source in the Navigator, given at every frame
    const int selected_index = navigator->selectedPannelSource();
    if (s == nullptr || Settings::application.current_view == View::TRANSITION)
        return;

    // Next window is a side pannel
    const ImGuiStyle& style = ImGui::GetStyle();
    if (beginPannelWindow("##navigatorSource"))
    {
        ///
        /// TITLE
        ///
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetCursorPosY(0.5f * (iconsize.y - ImGui::GetTextLineHeight()));
        ImGui::Text("Source");

        // index indicator
        ImGui::SetCursorPos(ImVec2(pannel_width_ - 2.8f * ImGui::GetTextLineHeightWithSpacing(), IMGUI_TOP_ALIGN));

        if (Mixer::manager().indexCurrentSource() < 0)
            Mixer::manager().setCurrentIndex(selected_index);
        ImGui::TextDisabled("#%d", Mixer::manager().indexCurrentSource());

        ImGui::PopFont();

        // name
        std::string sname = s->name();
        ImGui::SetCursorPosY(width_ - style.WindowPadding.x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::InputText("Name", &sname) ){
            Mixer::manager().renameSource(s, sname);
        }

        ///
        /// Source pannel
        ///
        static ImGuiVisitor v;
        if (reset)
            v.reset();
        s->accept(v);

        ///
        /// Transcoding panel for media player
        ///
        if (!s->failed()) {
            MediaSource* ms = dynamic_cast<MediaSource*>(s);
            if (ms != nullptr) {
                if (renderTranscodingPanel(ms->id(), ms->mediaplayer())) 
                    v.reset();
            }
        }

        ///
        /// ACTION BUTTONS PANEL if not loading
        ///
        ImGui::Text(" ");
        if (s->ready() || s->failed()) {

            ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 0);

            // clone button
            if ( s->failed() )
                ImGuiToolkit::ButtonDisabled( ICON_FA_SHARE_SQUARE " Clone & Filter", size);
            else if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone & Filter", size) ) {
                Mixer::manager().addSource ( (Source *) Mixer::manager().createSourceClone() );
                UserInterface::manager().showPannel(  Mixer::manager().numSource() );
            }

            // replace button
            if ( ImGui::Button( ICON_FA_PLUS_SQUARE " Replace", ImVec2((size.x - IMGUI_SAME_LINE)/2.f, 0)) ) {

                // prepare panel for new source of same type
                glm::ivec2 i = s->icon();
                if ( i == glm::ivec2(ICON_VI_SOURCE_VIDEO) || i == glm::ivec2(ICON_VI_SOURCE_IMAGE)
                     || i == glm::ivec2(ICON_VI_SOURCE_CLONE) )
                    Settings::application.source.new_type = Navigator::SOURCE_FILE;
                else if ( i == glm::ivec2(ICON_VI_SOURCE_SEQUENCE) )
                    Settings::application.source.new_type = Navigator::SOURCE_SEQUENCE;
                else if ( i == glm::ivec2(ICON_VI_SOURCE_PATTERN) || i == glm::ivec2(ICON_VI_SOURCE_TEXT)
                          || i == glm::ivec2(ICON_VI_SOURCE_GSTREAMER) || i == glm::ivec2(ICON_VI_SOURCE_SHADER)  )
                    Settings::application.source.new_type = Navigator::SOURCE_GENERATED;
                else
                    Settings::application.source.new_type = Navigator::SOURCE_CONNECTED;

                // switch to panel new source
                navigator->showPannelSource(NAV_NEW);
                // set source to be replaced
                navigator->setSourceToReplace(s);
            }

            // delete button
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if ( ImGui::Button( ACTION_DELETE, ImVec2((size.x - IMGUI_SAME_LINE)/2.f, 0)) ) {
                Mixer::manager().deleteSource(s);
                Action::manager().store(sname + std::string(": Deleted"));
            }
            // delete all button
            if ( Mixer::manager().session()->failedSources().size() > 1 && 
                Mixer::manager().session()->find(s) != Mixer::manager().session()->end() && s->failed()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 1.));
                if ( ImGui::Button( ICON_FA_REDO_ALT " Retry all", ImVec2((size.x - IMGUI_SAME_LINE)/2.f, 0)) ) {
                    auto failedsources = Mixer::manager().session()->failedSources();
                    for (auto sit = failedsources.cbegin(); sit != failedsources.cend(); ++sit) {
                        Source *s = Mixer::manager().findSource( (*sit)->id() );
                        if (s)
                            Mixer::manager().recreateSource( s );
                    }
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if ( ImGui::Button( ICON_FA_BACKSPACE " Delete all", ImVec2((size.x - IMGUI_SAME_LINE)/2.f, 0)) ) {
                    auto failedsources = Mixer::manager().session()->failedSources();
                    for (auto sit = failedsources.cbegin(); sit != failedsources.cend(); ++sit) {
                        Mixer::manager().deleteSource( Mixer::manager().findSource( (*sit)->id() ) );
                    }
                }
                ImGui::PopStyleColor(1);
            }
        }
        ImGui::End();
    }
}
