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


// Combo box to select a still image format, disabling a format whose encoder
// is not installed on this system (webpenc lives in gst-plugins-bad and can
// be absent) and one which could not hold the given resolution.
// Returns true only if the user selected a format (as ImGui::Combo does).
bool ComboImageFormat(const char *label, int *format, int width, int height)
{
    bool ret = false;

    // make sure the current format is valid
    if ( *format < GstToolkit::IMAGE_PNG || *format >= GstToolkit::IMAGE_INVALID )
        *format = GstToolkit::IMAGE_PNG;

    if (ImGui::BeginCombo(label, GstToolkit::image_name[*format])) {
        for (int i = GstToolkit::IMAGE_PNG; i < GstToolkit::IMAGE_INVALID; ++i) {
            const GstToolkit::Image image = (GstToolkit::Image) i;

            // an absent encoder and a resolution the format cannot hold both
            // disable the entry; explain which, in the tooltip
            std::string unavailable;
            if (GstToolkit::getImageEncodingPipeline(image).empty())
                unavailable = std::string(GstToolkit::image_name[i]) +
                              " is not available: its encoder is not installed on this system.";
            else
                unavailable = GstToolkit::unsupportedImageResolution(image, width, height);

            if (ImGui::Selectable( GstToolkit::image_name[i], *format == i,
                                   unavailable.empty() ? ImGuiSelectableFlags_None
                                                       : ImGuiSelectableFlags_Disabled )) {
                *format = i;
                ret = true;
            }
            if (!unavailable.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGuiToolkit::ToolTip( unavailable.c_str() );
        }
        ImGui::EndCombo();
    }

    return ret;
}


// Which models are worth offering for what is being encoded. A video needs
// a network fast enough to run on every one of its frames, which rules the
// heavy ones out. An image instead needs a network which handles an alpha
// channel correctly, because a still can carry transparency and several of
// the models return noise rather than a flattened image when given one --
// the per-model measurements are recorded in Upscaler.cpp. Speed does not
// disqualify a model here: seconds of inference is a fine price for the one
// frame of an image.
static bool suitableUpscaler(const UpscalerModel &m, bool is_image)
{
    return is_image ? m.alpha : m.video;
}

// Combo box to select a Real-ESRGAN upscaling model, disabling those whose
// upscaled resolution the target codec could not accept: the factor comes
// with the model, so a x4 model on a HD source demands 8K from the encoder.
// The selection is a model name rather than an index, so that reordering the
// catalogue never silently changes a stored preference.
// Returns true only if the user selected a model (as ImGui::Combo does).
bool ComboUpscaler(const char *label, std::string *model, int width, int height,
                   const TranscoderCodec &codec)
{
    bool ret = false;

    const bool is_image = std::holds_alternative<GstToolkit::Image>(codec);

    // an unknown name (an older setting, a model since removed) reads as no
    // upscaling, which is also what an empty preference means. A model which
    // is valid but not suitable here -- the preference was last set on the
    // other kind of source -- reads the same way, so that it can never reach
    // the Transcoder just because it was left selected.
    const UpscalerModel &current = Upscaler::model(*model);
    *model = suitableUpscaler(current, is_image) ? current.name : Upscaler::NONE;

    if (ImGui::BeginCombo(label, model->c_str())) {
        for (const UpscalerModel &m : Upscaler::models()) {
            if (!suitableUpscaler(m, is_image))
                continue;

            const int w = width * m.factor;
            const int h = height * m.factor;
            const std::string unsupported =
                is_image
                    ? GstToolkit::unsupportedImageResolution(std::get<GstToolkit::Image>(codec), w, h)
                    : GstToolkit::unsupportedResolution(std::get<GstToolkit::Profile>(codec), w, h,
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
