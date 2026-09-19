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
 * @brief A selectable upscaling model
 *
 * The upscale factor is a property of the model, not a separate setting:
 * each entry fixes the file(s) to fetch and the resolution multiplier the
 * network applies. The first entry of Upscaler::models() is the pass-through
 * model (factor 1, no files): selecting it does nothing.
 *
 * Which models are listed depends on the backend that will run them, so an
 * entry never has to say which backend it belongs to -- see
 * Upscaler::models().
 */
struct UpscalerModel {
    const char *name;        ///< short name, used to select and to store the model
    const char *description; ///< human readable info, shown as a tooltip
    const char *file;        ///< ncnn .param graph, or the .onnx model
    const char *weights;     ///< ncnn .bin weights; nullptr for a single file ONNX model
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
 *
 * The list depends on which backend this machine can run: the ncnn networks
 * when a Vulkan device is there, otherwise the ONNX ones, which need only a
 * CPU. The two families are different files with different names, so the
 * selection offered legitimately differs from one machine to another.
 */
const std::vector<UpscalerModel> &models();

/**
 * @brief The model with this name, or the NONE model if unknown or empty
 */
const UpscalerModel &model(const std::string &name);

/**
 * @brief True when this machine can run one of the upscaling backends
 *
 * A false here means no upscaling can be performed at all and the selection
 * should not be offered. Unlike a build-time check this accounts for a
 * Vulkan device being absent at runtime, in which case the ONNX backend
 * takes over.
 */
bool available();

/**
 * @brief Inference engine for one upscaling model
 *
 * Runs the model on whichever backend this machine offers: Real-ESRGAN on
 * the Vulkan GPU through ncnn, or the ONNX Runtime on the CPU. Constructing
 * it downloads the model files on first use (needs network once) and loads
 * the network; every step throws std::runtime_error on failure. Not thread
 * safe: one engine belongs to the thread which created it.
 */
class Engine
{
public:
    /**
     * @brief Load a model on the backend this machine offers
     * @param model One of Upscaler::models(), with a factor above 1
     * @param tilesize Tile size in pixels, 0 to derive it from the memory
     *        budget of the backend (VRAM for ncnn, RAM for ONNX)
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
     * split off and enlarged by interpolation instead (bicubic on ncnn,
     * bilinear on ONNX). A hard
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
