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

#include <stdexcept>

#ifdef HAVE_NCNN
// Real-ESRGAN (ext/realesrgan-ncnn-vulkan/src) built against ext/ncnn
#include "realesrgan.h"
#include "gpu.h"
#endif

#include "Log.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/NcnnToolkit.h"
#include "Toolkit/SystemToolkit.h"

#include "Upscaler.h"

namespace {

// Model files are platform independent ncnn data (.param = text graph
// description, .bin = raw weights). The official Real-ESRGAN releases ship
// them only inside zip archives, but the actively maintained upscayl project
// hosts the same files raw -- pinned here to a fixed commit.
const char *kModelDir = "models/realesrgan";
const char *kModelBaseUrl =
    "https://raw.githubusercontent.com/upscayl/custom-models/"
    "4b6d2cfa59c7442af115dfc6e50fd8d7d40b96ef/models/";

// Curated from github.com/upscayl/custom-models (the pinned commit above).
// To add a model, list its exact .param/.bin filenames from the repository
// and its upscale factor; set video false unless it is fast enough to run
// on every frame of a video stream (the heavy networks below take seconds
// per frame, which only makes sense for a single image).
const std::vector<UpscalerModel> kModels = {
    { "No upscale",
      "Transcode at the resolution of the source video",
      nullptr, nullptr, 1, true },
    { "ESRGAN Anime x2",
      "Real-ESRGAN anime video v3, x2 - fast, tuned for animation - 1.2MB",
      "realesr-animevideov3-x2.param", "realesr-animevideov3-x2.bin", 2, true },
    { "ESRGAN Anime x3",
      "Real-ESRGAN anime video v3, x3 - fast, tuned for animation - 1.2MB",
      "realesr-animevideov3-x3.param", "realesr-animevideov3-x3.bin", 3, true },
    { "ESRGAN Anime x4",
      "Real-ESRGAN anime video v3, x4 - fast, tuned for animation - 1.2MB",
      "realesr-animevideov3-x4.param", "realesr-animevideov3-x4.bin", 4, true },
    { "ESRGAN General x4",
      "Real-ESRGAN general v3, x4 - fast, tuned for photo & video - 2.4MB",
      "RealESRGAN_General_x4_v3.param", "RealESRGAN_General_x4_v3.bin", 4, true },
    { "ESRGAN Deep x4",
      "Real-ESRGAN Wide Deep Network v3, x4 - fast, tuned for synthetic images - 2.4MB",
      "RealESRGAN_General_WDN_x4_v3.param", "RealESRGAN_General_WDN_x4_v3.bin", 4, true },
    { "Nomos8k x4",
      "Nomos8k SC, x4 - slow, tuned for sharp photographic detail - 33MB",
      "4xNomos8kSC.param", "4xNomos8kSC.bin", 4, false },
    { "NMKD Superscale x4",
      "NMKD Superscale SP, x4 - slow, tuned for clean real-world images - 66MB",
      "4x_NMKD-Superscale-SP_178000_G.param",
      "4x_NMKD-Superscale-SP_178000_G.bin", 4, false },
    { "NMKD Siax x4",
      "NMKD Siax, x4 - slow, general purpose for web images - 66MB",
      "4x_NMKD-Siax_200k.param", "4x_NMKD-Siax_200k.bin", 4, false }
};

// Local folder holding the downloaded model files
std::string modelPath()
{
    return SystemToolkit::full_filename(SystemToolkit::settings_path(), kModelDir);
}

}

const char *Upscaler::NONE = "No upscale";

const std::vector<UpscalerModel> &Upscaler::models()
{
    return kModels;
}

const UpscalerModel &Upscaler::model(const std::string &name)
{
    for (const auto &m : kModels)
        if (name == m.name)
            return m;

    // kModels[0] is the pass-through model: the default for an unknown name
    return kModels[0];
}

bool Upscaler::available()
{
#ifdef HAVE_NCNN
    return true;
#else
    return false;
#endif
}

// ============================================================ Engine

#ifdef HAVE_NCNN

struct Upscaler::Engine::Impl {
    // the Vulkan instance must outlive the network built on it, which member
    // declaration order guarantees (gpu_ destroyed after esrgan_)
    NcnnToolkit::GpuInstance gpu_;
    RealESRGAN esrgan_;
    std::string description_;

    explicit Impl(bool tta) : esrgan_(gpu_.index(), tta) {}
};

Upscaler::Engine::Engine(const UpscalerModel &model, int tilesize)
{
    if (model.factor < 2 || model.param == nullptr || model.bin == nullptr)
        throw std::runtime_error("no upscaling model selected");

    // fetch the two model files on first use, from the pinned repository
    const std::string path = modelPath();
    for (const char *file : { model.param, model.bin }) {
        const std::string dest = SystemToolkit::full_filename(path, file);
        if (!SystemToolkit::file_exists(dest)) {
            Log::Info("Upscaler: downloading model file %s", file);
            GstToolkit::download_file(std::string(kModelBaseUrl) + file, dest);
        }
    }

    impl_ = std::make_unique<Impl>(/*tta_mode=*/false);

    if (impl_->esrgan_.load(SystemToolkit::full_filename(path, model.param),
                            SystemToolkit::full_filename(path, model.bin)) != 0)
        throw std::runtime_error(std::string("failed to load upscaling model ") + model.name);

    impl_->esrgan_.scale = model.factor;
    impl_->esrgan_.prepadding = 10;

    if (tilesize >= 32)
        impl_->esrgan_.tilesize = tilesize;
    else {
        // Automatic tile size from the GPU memory budget (upstream
        // heuristic): tiles are processed independently and blended, which
        // is what makes a large frame fit in VRAM at all.
        const uint32_t heap_budget = ncnn::get_gpu_device(impl_->gpu_.index())->get_heap_budget();
        if (heap_budget > 1900)     impl_->esrgan_.tilesize = 200;
        else if (heap_budget > 550) impl_->esrgan_.tilesize = 100;
        else if (heap_budget > 190) impl_->esrgan_.tilesize = 64;
        else                        impl_->esrgan_.tilesize = 32;
    }

    impl_->description_ = std::string(model.name) + " on Vulkan GPU (" + impl_->gpu_.name() +
                          "), tiles of " + std::to_string(impl_->esrgan_.tilesize) + " pixels";
}

Upscaler::Engine::~Engine()
{
}

void Upscaler::Engine::process(const unsigned char *in, int w, int h, unsigned char *out)
{
    const int factor = impl_->esrgan_.scale;

    // ncnn::Mat views over the interleaved RGB u8 buffers, elemsize =
    // elempack = 3: the pre/postprocessing shaders do the u8 <-> float
    // conversion on the GPU (as in upstream main.cpp)
    ncnn::Mat in_mat(w, h, (void *) in, (size_t) 3, 3);
    ncnn::Mat out_mat(w * factor, h * factor, (void *) out, (size_t) 3, 3);

    // Real-ESRGAN prints a per-tile progress percentage to stderr, which
    // restarts on every frame; suppress it (Transcoder reports progress)
    SystemToolkit::StderrSilencer silence;
    if (impl_->esrgan_.process(in_mat, out_mat) != 0)
        throw std::runtime_error("Real-ESRGAN processing failed");
}

const std::string &Upscaler::Engine::describe() const
{
    return impl_->description_;
}

#else // HAVE_NCNN

// Without the ncnn backend there is nothing to run: the constructor always
// throws, so no Engine ever exists and the other methods are unreachable.
struct Upscaler::Engine::Impl { };

Upscaler::Engine::Engine(const UpscalerModel &, int)
{
    throw std::runtime_error("this build has no ncnn Vulkan backend");
}

Upscaler::Engine::~Engine()
{
}

void Upscaler::Engine::process(const unsigned char *, int, int, unsigned char *)
{
}

const std::string &Upscaler::Engine::describe() const
{
    static const std::string none;
    return none;
}

#endif // HAVE_NCNN
