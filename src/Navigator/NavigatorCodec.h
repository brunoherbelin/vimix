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
#ifndef NAVIGATORCODEC_H
#define NAVIGATORCODEC_H

#include <string>

// Selection of a video encoding profile in the Navigator pannels:
// the encoders have different limitations (e.g. GPU H264 is limited to 4K),
// tested with GstToolkit to disable or replace a profile.

// If the encoder of the given profile cannot encode this resolution, select
// the closest profile which can instead; true if the profile was changed.
bool ValidateCodecResolution(int *profile, int width, int height);

// Combo box to select an encoding profile, disabling the profiles whose
// encoder cannot encode the given resolution.
// Returns true only if the user selected a profile (as ImGui::Combo does).
bool ComboCodec(const char *label, int *profile, int width, int height);

// Combo box to select a Real-ESRGAN upscaling model by name, listing only
// the models fast enough for video and disabling those whose upscaled
// resolution the given profile could not encode (the model fixes the
// factor, so a x4 model on a HD source asks for 8K out of the encoder).
// Returns true only if the user selected a model (as ImGui::Combo does).
bool ComboUpscaler(const char *label, std::string *model, int width, int height, int profile);

#endif /* NAVIGATORCODEC_H */
