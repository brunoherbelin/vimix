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


#include "NavigatorInternal.h"

#include "Settings.h"
#include "Upscaler.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/ImGuiToolkit.h"

#include "NavigatorCodec.h"


// If the encoder of the given profile cannot encode this resolution (e.g.
// GPU encoders limited to 4096 x 4096 for H264), select the closest profile
// which can instead.
// Silent (can be called every frame); returns true if the profile was changed.
bool ValidateCodecResolution(int *profile, int width, int height)
{
    if ( *profile < GstToolkit::H264_RT || *profile >= GstToolkit::DEFAULT )
        return false;

    const GstToolkit::Profile alternative =
        GstToolkit::alternativeProfile((GstToolkit::Profile) *profile, width, height,
                                       Settings::application.render.gpu_decoding);
    if ( alternative == *profile )
        return false;

    *profile = alternative;

    return true;
}

// Combo box to select an encoding profile, disabling the profiles whose
// encoder cannot encode the given resolution, and selecting an alternative
// profile instead of a profile which cannot.
// Returns true only if the user selected a profile (as ImGui::Combo does).
bool ComboCodec(const char *label, int *profile, int width, int height)
{
    bool ret = false;

    // make sure the current profile is valid for this resolution
    if ( *profile < GstToolkit::H264_RT || *profile >= GstToolkit::DEFAULT )
        *profile = GstToolkit::H264_RT;
    ValidateCodecResolution(profile, width, height);

    if (ImGui::BeginCombo(label, GstToolkit::profile_name[*profile])) {
        for (int i = GstToolkit::H264_RT; i < GstToolkit::DEFAULT; ++i) {
            const bool supported = GstToolkit::supportsResolution((GstToolkit::Profile) i, width, height,
                                                                 Settings::application.render.gpu_decoding);
            if (ImGui::Selectable( GstToolkit::profile_name[i], *profile == i,
                                   supported ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_Disabled )) {
                *profile = i;
                ret = true;
            }
            if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGuiToolkit::ToolTip( GstToolkit::unsupportedResolution((GstToolkit::Profile) i, width, height,
                                       Settings::application.render.gpu_decoding).c_str() );
        }
        ImGui::EndCombo();
    }

    return ret;
}


// Combo box to select a Real-ESRGAN upscaling model, showing only the models
// fast enough to run on every frame of a video, and disabling those whose
// upscaled resolution the encoder of the given profile could not accept: the
// factor comes with the model, so a x4 model on a HD source demands 8K from
// the encoder. The selection is a model name rather than an index, so that
// reordering the catalogue never silently changes a stored preference.
// Returns true only if the user selected a model (as ImGui::Combo does).
bool ComboUpscaler(const char *label, std::string *model, int width, int height, int profile)
{
    bool ret = false;

    // an unknown name (an older setting, a model since removed) reads as no
    // upscaling, which is also what an empty preference means
    const UpscalerModel &current = Upscaler::model(*model);
    *model = current.name;

    if (ImGui::BeginCombo(label, current.name)) {
        for (const UpscalerModel &m : Upscaler::models()) {
            // the heavy networks take seconds per frame: they belong to
            // single image upscaling, not to a video stream
            if (!m.video)
                continue;

            const std::string unsupported =
                GstToolkit::unsupportedResolution((GstToolkit::Profile) profile,
                                                  width * m.factor, height * m.factor,
                                                  Settings::application.render.gpu_decoding);
            if (ImGui::Selectable(m.name, *model == m.name,
                                  unsupported.empty() ? ImGuiSelectableFlags_None
                                                      : ImGuiSelectableFlags_Disabled)) {
                *model = m.name;
                ret = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGuiToolkit::ToolTip(unsupported.empty() ? m.description : unsupported.c_str());
        }
        ImGui::EndCombo();
    }

    return ret;
}
