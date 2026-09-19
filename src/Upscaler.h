/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2025 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#ifndef UPSCALER_H
#define UPSCALER_H

#include <memory>
#include <string>
#include <vector>

/**
 * @brief A selectable Real-ESRGAN upscaling model (ncnn / Vulkan)
 *
 * The upscale factor is a property of the model, not a separate setting:
 * each entry fixes the .param/.bin files to fetch and the resolution
 * multiplier the network applies. The first entry of Upscaler::models() is
 * the pass-through model (factor 1, no files): selecting it does nothing.
 */
struct UpscalerModel {
    const char *name;        ///< short name, used to select and to store the model
    const char *description; ///< human readable info, shown as a tooltip
    const char *param;       ///< ncnn .param filename in the model repository
    const char *bin;         ///< ncnn .bin filename in the model repository
    int factor;              ///< upscaling factor (1 = none, else 2, 3 or 4)
    bool video;              ///< fast enough to run on every frame of a video
    bool alpha;              ///< produces a correct image from an RGBA source
};

namespace Upscaler
{

/**
 * @brief Name of the pass-through model: no upscaling at all (the default)
 */
extern const char *NONE;

/**
 * @brief The models available for selection, models()[0] being NONE
 *
 * Enumerate this to build a UI; select one by its @c name. Models with
 * @c video false are far too slow to run on a video stream and should only
 * be offered for single images. Models with @c alpha false must never be
 * given a transparent image: see the note on that field in Upscaler.cpp.
 */
const std::vector<UpscalerModel> &models();

/**
 * @brief The model with this name, or the NONE model if unknown or empty
 */
const UpscalerModel &model(const std::string &name);

/**
 * @brief True when this build has the ncnn / Vulkan backend compiled in
 *
 * A false here means no upscaling can be performed: the selection should
 * not even be offered. It does not guarantee a usable Vulkan device; that
 * is only known when an Engine is constructed.
 */
bool available();

/**
 * @brief Real-ESRGAN inference engine for one model, on the Vulkan GPU
 *
 * Constructing it downloads the model files on first use (needs network
 * once), acquires the shared ncnn Vulkan instance and loads the network;
 * every step throws std::runtime_error on failure. Not thread safe: one
 * engine belongs to the thread which created it.
 */
class Engine
{
public:
    /**
     * @brief Load a model on the default Vulkan device
     * @param model One of Upscaler::models(), with a factor above 1
     * @param tilesize GPU tile size, 0 to derive it from the VRAM budget
     */
    explicit Engine(const UpscalerModel &model, int tilesize = 0);
    ~Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    /**
     * @brief Upscale one frame
     * @param in Tightly packed interleaved pixels, w x h
     * @param out Tightly packed interleaved pixels, (w x factor) x (h x factor)
     * @param channels 3 for RGB, 4 for RGBA
     *
     * With 4 channels the network still sees only the colour: the alpha is
     * split off and enlarged by a bicubic interpolation instead. A hard
     * transparency edge therefore comes out smoothly interpolated rather
     * than sharpened, which is the intended behaviour -- the model has no
     * business inventing detail in a matte.
     *
     * Throws std::runtime_error if the inference fails.
     */
    void process(const unsigned char *in, int w, int h, unsigned char *out, int channels = 3);

    /**
     * @brief Description of the device and tiling used, for logging
     */
    const std::string &describe() const;

private:
    // ncnn and Real-ESRGAN types are kept out of this header
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}

#endif // UPSCALER_H
