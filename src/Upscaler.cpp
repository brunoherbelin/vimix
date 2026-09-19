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

#include <algorithm>
#include <cmath>
#include <stdexcept>

#ifdef HAVE_NCNN
// Real-ESRGAN (ext/realesrgan-ncnn-vulkan/src) built against ext/ncnn
#include "realesrgan.h"
#include "gpu.h"
#include "Toolkit/NcnnToolkit.h"
#endif

#ifdef HAVE_ONNX
#include "Toolkit/OnnxToolkit.h"
#endif

#include "Log.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/SystemToolkit.h"

#include "Upscaler.h"

namespace {

// Model files are platform independent ncnn data (.param = text graph
// description, .bin = raw weights). The official Real-ESRGAN releases ship
// them only inside zip archives, but the actively maintained upscayl project
// hosts the same files raw -- pinned here to a fixed commit.
// The pass-through entry heads both catalogues and is what Upscaler::NONE
// names; spelled once so the three can never disagree.
const char *kNone = "None";
const char *kNcnnModelDir = "models/realesrgan";
const char *kNcnnModelBaseUrl =
    "https://raw.githubusercontent.com/upscayl/custom-models/"
    "4b6d2cfa59c7442af115dfc6e50fd8d7d40b96ef/models/";

// Curated from github.com/upscayl/custom-models (the pinned commit above).
// To add a model, list its exact .param/.bin filenames from the repository
// and its upscale factor, then measure the two capability flags -- neither
// can be guessed from the size or the architecture:
//
//   video  fast enough to run on every frame of a video stream. The 33MB and
//          66MB networks take seconds per frame, which only makes sense for
//          a single image.
//
//   alpha  produces a correct image from an RGBA source. Never offer a model with this
//          false for an image which may carry transparency.
const std::vector<UpscalerModel> kNcnnModels = {
    { kNone,
      "Transcode at the resolution of the source video",
      nullptr, nullptr, 1, true, true },
    { "ESRGAN Anime x2",
      "Real-ESRGAN anime video v3, x2 - fast, tuned for animation - 1.2MB",
      "realesr-animevideov3-x2.param", "realesr-animevideov3-x2.bin", 2, true, false },
    { "ESRGAN Anime x3",
      "Real-ESRGAN anime video v3, x3 - fast, tuned for animation - 1.2MB",
      "realesr-animevideov3-x3.param", "realesr-animevideov3-x3.bin", 3, true, false },
    { "ESRGAN Anime x4",
      "Real-ESRGAN anime video v3, x4 - fast, tuned for animation - 1.2MB",
      "realesr-animevideov3-x4.param", "realesr-animevideov3-x4.bin", 4, true, true },
    { "ESRGAN General x4",
      "Real-ESRGAN general v3, x4 - fast, tuned for photo & video - 2.4MB",
      "RealESRGAN_General_x4_v3.param", "RealESRGAN_General_x4_v3.bin", 4, true, false },
    { "ESRGAN Deep x4",
      "Real-ESRGAN Wide Deep Network v3, x4 - fast, tuned for synthetic images - 2.4MB",
      "RealESRGAN_General_WDN_x4_v3.param", "RealESRGAN_General_WDN_x4_v3.bin", 4, true, false },
    { "LSDIR Compact x4",
      "LSDIR Compact C3, x4 - fast, general purpose, keeps transparency - 1.2MB",
      "4xLSDIRCompactC3.param", "4xLSDIRCompactC3.bin", 4, true, true },
    { "Nomos8k x4",
      "Nomos8k SC, x4 - slow, tuned for sharp photographic detail - 33MB",
      "4xNomos8kSC.param", "4xNomos8kSC.bin", 4, false, true },
    { "NMKD Superscale x4",
      "NMKD Superscale SP, x4 - slow, tuned for clean real-world images - 66MB",
      "4x_NMKD-Superscale-SP_178000_G.param",
      "4x_NMKD-Superscale-SP_178000_G.bin", 4, false, true },
    { "NMKD Siax x4",
      "NMKD Siax, x4 - slow, general purpose for web images - 66MB",
      "4x_NMKD-Siax_200k.param", "4x_NMKD-Siax_200k.bin", 4, false, true }
};

// ONNX models run on the CPU through ONNX Runtime, which is what makes
// upscaling possible where ncnn cannot go -- macOS, or any machine without a
// Vulkan device. They are single .onnx files, all taking three channels, so
// the backend carries any alpha around the network itself and every one of
// them keeps transparency.
//
// Curated from huggingface.co/notaneimu/onnx-image-models, listed lightest
// first. Descriptions of the networks are at
// huggingface.co/huggingworld/onnx-image-models.
//
// A model is admitted only if its memory cost fits UpscalerONNX's single
// tiling rule: at most kBytesPerInputPixel of activations per pixel of input,
// measured on the CPU provider. 
//
//    model                            KB per input pixel
//    4xLSDIRCompactv2                 0.7
//    realesr-general-wdn-x4v3         0.7
//    4xPurePhoto-RealPLSKR            2.3
//    4x-ClearRealityV1-fp32-opset17   2.6
//    4xPurePhoto-Span                 2.6
//    2x-spanx2-ch48                   2.6
//
// A model exported with a fixed input size is tiled at that size, so its
// cost is a constant rather than a rate: 4xNomos2_realplksr_dysample_256
// takes exactly 256x256 and holds 0.7 GB per tile whatever the image.
//
const char *kOnnxModelDir = "models/upscale";
const char *kOnnxModelBaseUrl =
    "https://huggingface.co/notaneimu/onnx-image-models/resolve/main/";

const std::vector<UpscalerModel> kOnnxModels = {
    { kNone,
      "Transcode at the resolution of the source video",
      nullptr, nullptr, 1, true, true },
    { "SPAN x2",
      "SPAN ch48, x2 - fast, faithful general purpose upscaler - 1.7MB",
      "2x-spanx2-ch48.onnx", nullptr, 2, true, true },
    { "ClearReality x4",
      "ClearReality v1, x4 - fast, crisp general purpose upscaler - 1.7MB",
      "4x-ClearRealityV1-fp32-opset17.onnx", nullptr, 4, true, true },
    { "PurePhoto SPAN x4",
      "PurePhoto SPAN, x4 - fast, tuned for photographic detail - 1.7MB",
      "4xPurePhoto-Span.onnx", nullptr, 4, true, true },
    { "LSDIR Compact x4",
      "LSDIR Compact v2, x4 - fast, general purpose - 2.5MB",
      "4xLSDIRCompactv2.onnx", nullptr, 4, true, true },
    { "ESRGAN General x4",
      "Real-ESRGAN general WDN v3, x4 - fast, tuned for photo & video - 4.9MB",
      "realesr-general-wdn-x4v3.onnx", nullptr, 4, true, true },
    { "PurePhoto RealPLKSR x4",
      "PurePhoto RealPLKSR, x4 - slow, high quality photographic detail - 30MB",
      "4xPurePhoto-RealPLSKR.onnx", nullptr, 4, false, true },
    { "Nomos2 RealPLKSR x4",
      "Nomos2 RealPLKSR DySample, x4 - slow, sharpest detail for photos and illustrations - 30MB",
      "4xNomos2_realplksr_dysample_256_fp32_fullyoptimized.onnx", nullptr, 4, false, true }
};

// Offered when neither backend can run: the pass-through entry alone, so
// that a stored preference still resolves and the UI has something to show.
const std::vector<UpscalerModel> kNoModels = { kNcnnModels[0] };

// Which backend will actually run. Decided once and cached: ncnn needs a
// Vulkan device at runtime and not merely at build time, so a machine
// without one falls back to ONNX exactly as a build without ncnn does.
enum Backend { BACKEND_NONE, BACKEND_NCNN, BACKEND_ONNX };

Backend backend()
{
    static const Backend selected = [] {
#ifdef HAVE_NCNN
        try {
            NcnnToolkit::GpuInstance probe;
            return BACKEND_NCNN;
        }
        catch (const std::exception &) {
            Log::Info("Upscaler: no Vulkan device, falling back to the CPU backend");
        }
#endif
#ifdef HAVE_ONNX
        return OnnxToolkit::available() ? BACKEND_ONNX : BACKEND_NONE;
#endif
        return BACKEND_NONE;
    }();

    return selected;
}

// Local folder holding the downloaded model files, per backend
std::string modelPath()
{
    return SystemToolkit::full_filename(SystemToolkit::settings_path(),
                                        backend() == BACKEND_NCNN ? kNcnnModelDir : kOnnxModelDir);
}

// Fetch the model's file(s) on first use, from the repository of its family
void ensureModel(const UpscalerModel &model)
{
    const char *base = backend() == BACKEND_NCNN ? kNcnnModelBaseUrl : kOnnxModelBaseUrl;
    const std::string path = modelPath();

    for (const char *file : { model.file, model.weights }) {
        if (file == nullptr)
            continue;
        const std::string dest = SystemToolkit::full_filename(path, file);
        if (!SystemToolkit::file_exists(dest)) {
            Log::Info("Upscaler: downloading model file %s", file);
            GstToolkit::download_file(std::string(base) + file, dest);
        }
    }
}

}

const char *Upscaler::NONE = kNone;

const std::vector<UpscalerModel> &Upscaler::models()
{
    switch (backend()) {
    case BACKEND_NCNN: return kNcnnModels;
    case BACKEND_ONNX: return kOnnxModels;
    default:           return kNoModels;
    }
}

const UpscalerModel &Upscaler::model(const std::string &name)
{
    const std::vector<UpscalerModel> &catalogue = models();

    for (const auto &m : catalogue)
        if (name == m.name)
            return m;

    // [0] is the pass-through model: the default for an unknown name, which
    // is also what a name from the other backend's catalogue reads as
    return catalogue[0];
}

bool Upscaler::available()
{
    return backend() != BACKEND_NONE;
}

// ============================================================ backends

namespace {

// One loaded network, whatever runs it. Frames cross this interface as
// tightly packed interleaved bytes, RGB or RGBA, which is what both the
// GStreamer buffers on either side and the two libraries underneath happen
// to want.
class UpscalerBackend {
public:
    virtual ~UpscalerBackend() = default;
    virtual const char *describe() const = 0;
    virtual void process(const unsigned char *in, int w, int h,
                         unsigned char *out, int channels) = 0;
};

#ifdef HAVE_NCNN
// ------------------------------------------ inference: ncnn / Vulkan GPU
//
// Real-ESRGAN from ext/realesrgan-ncnn-vulkan, which does its own tiling and
// its own alpha handling on the GPU -- see the note on UpscalerModel::alpha
// for which models can be trusted with the latter.
class UpscalerNCNN : public UpscalerBackend {
public:
    UpscalerNCNN(const UpscalerModel &model, int tilesize)
        : esrgan_(gpu_.index(), /*tta_mode=*/false)
    {
        const std::string path = modelPath();
        if (esrgan_.load(SystemToolkit::full_filename(path, model.file),
                         SystemToolkit::full_filename(path, model.weights)) != 0)
            throw std::runtime_error(std::string("failed to load upscaling model ") + model.name);

        esrgan_.scale = model.factor;
        esrgan_.prepadding = 10;

        if (tilesize >= 32)
            esrgan_.tilesize = tilesize;
        else {
            // Automatic tile size from the GPU memory budget (upstream
            // heuristic): tiles are processed independently and blended,
            // which is what makes a large frame fit in VRAM at all.
            const uint32_t heap_budget = ncnn::get_gpu_device(gpu_.index())->get_heap_budget();
            if (heap_budget > 1900)     esrgan_.tilesize = 200;
            else if (heap_budget > 550) esrgan_.tilesize = 100;
            else if (heap_budget > 190) esrgan_.tilesize = 64;
            else                        esrgan_.tilesize = 32;
        }

        description_ = std::string(model.name) + " on Vulkan GPU (" + gpu_.name() +
                       "), tiles of " + std::to_string(esrgan_.tilesize) + " pixels";
    }

    const char *describe() const override { return description_.c_str(); }

    void process(const unsigned char *in, int w, int h, unsigned char *out, int channels) override
    {
        const int factor = esrgan_.scale;

        // ncnn::Mat views over the interleaved u8 buffers, elemsize =
        // elempack = the number of channels: the pre/postprocessing shaders
        // do the u8 <-> float conversion on the GPU (as in upstream
        // main.cpp). With 4 channels Real-ESRGAN routes the alpha around the
        // network and enlarges it with the bicubic layers built by load().
        ncnn::Mat in_mat(w, h, (void *) in, (size_t) channels, channels);
        ncnn::Mat out_mat(w * factor, h * factor, (void *) out, (size_t) channels, channels);

        // Real-ESRGAN prints a per-tile progress percentage to stderr, which
        // restarts on every frame; suppress it (Transcoder reports progress)
        SystemToolkit::StderrSilencer silence;
        if (esrgan_.process(in_mat, out_mat) != 0)
            throw std::runtime_error("Real-ESRGAN processing failed");
    }

private:
    // the Vulkan instance must outlive the network built on it, which member
    // declaration order guarantees (gpu_ destroyed after esrgan_)
    NcnnToolkit::GpuInstance gpu_;
    RealESRGAN esrgan_;
    std::string description_;
};
#endif // HAVE_NCNN

#ifdef HAVE_ONNX
// ----------------------------------------------- inference: ONNX Runtime
//
// The CPU backend, and the only one on a machine without Vulkan. Unlike
// Real-ESRGAN's ncnn implementation, ONNX Runtime does nothing for us: the
// tiling and the alpha channel are this class's job.
//
// Tiling is not an optimisation here but a necessity. These networks hold
// their activations for the whole input at once, so a 1080p frame through a
// x4 model would ask for several gigabytes; feeding it in tiles bounds that
// by the tile size whatever the frame size. Each tile is taken with a margin
// of context around it and that margin is cropped off the result, so the
// network never sees a tile edge where the frame has none and the seams do
// not show.
class UpscalerONNX : public UpscalerBackend {
public:
    UpscalerONNX(const UpscalerModel &model, int tilesize)
        : session_(SystemToolkit::full_filename(modelPath(), model.file))
        , factor_(model.factor)
    {
        if (session_.inputCount() != 1)
            throw std::runtime_error("unsupported ONNX model: it expects " +
                                     std::to_string(session_.inputCount()) +
                                     " inputs, but only a single image tensor is provided");

        // every upscaling model in the catalogue takes plain RGB; a model
        // wanting anything else is not one we know how to feed
        const std::vector<int64_t> shape = session_.inputShape();
        const int c = (shape.size() == 4 && shape[1] > 0) ? (int) shape[1] : 3;
        if (c != 3)
            throw std::runtime_error("unsupported ONNX model: its input takes " +
                                     std::to_string(c) + " channels, expected 3");

        // Most of these networks take any size, but some are exported with
        // a fixed one (4xNomos2...256 takes exactly 256x256): the model then
        // dictates the tiles, and every region handed to it must be exactly
        // that size -- see region().
        if (shape.size() == 4 && shape[2] > 0 && shape[3] > 0) {
            fixed_h_ = (int) shape[2];
            fixed_w_ = (int) shape[3];
            if (fixed_w_ < 2 * kTilePadding + 16 || fixed_h_ < 2 * kTilePadding + 16)
                throw std::runtime_error("unsupported ONNX model: its fixed input of " +
                                         std::to_string(fixed_w_) + "x" + std::to_string(fixed_h_) +
                                         " is too small to be tiled");
            tile_ = std::min(fixed_w_, fixed_h_) - 2 * kTilePadding;
        }
        else
            tile_ = tilesize >= 32 ? tilesize : automaticTileSize();

        description_ = std::string(model.name) + " on " + session_.describe() +
                       " (onnxruntime), tiles of " + std::to_string(tile_) + " pixels";
        if (fixed_w_ > 0)
            description_ += " (fixed " + std::to_string(fixed_w_) + "x" +
                            std::to_string(fixed_h_) + " input)";
    }

    const char *describe() const override { return description_.c_str(); }

    void process(const unsigned char *in, int w, int h, unsigned char *out, int channels) override
    {
        const int ow = w * factor_;

        // ---- colour, tile by tile
        const int step_x = fixed_w_ > 0 ? fixed_w_ - 2 * kTilePadding : tile_;
        const int step_y = fixed_h_ > 0 ? fixed_h_ - 2 * kTilePadding : tile_;

        for (int y0 = 0; y0 < h; y0 += step_y) {
            for (int x0 = 0; x0 < w; x0 += step_x) {
                const int x1 = std::min(x0 + step_x, w);
                const int y1 = std::min(y0 + step_y, h);

                // the part of the frame handed to the network for this tile
                int px0, tw, py0, th;
                region(x0, x1, w, fixed_w_, px0, tw);
                region(y0, y1, h, fixed_h_, py0, th);

                // interleaved bytes -> planar float in 0..1, the layout every
                // one of these networks expects. A region can only overhang
                // the frame (to the right or bottom) for a fixed size model
                // given a frame smaller than its input; the overhang is then
                // filled by mirroring the frame -- see mirror().
                const size_t plane = (size_t) tw * th;
                std::vector<float> input(plane * 3);
                for (int y = 0; y < th; ++y) {
                    const unsigned char *row = in + (size_t) mirror(py0 + y, h) * w * channels;
                    for (int x = 0; x < tw; ++x) {
                        const unsigned char *px = row + (size_t) mirror(px0 + x, w) * channels;
                        const size_t i = (size_t) y * tw + x;
                        input[0 * plane + i] = px[0] / 255.f;
                        input[1 * plane + i] = px[1] / 255.f;
                        input[2 * plane + i] = px[2] / 255.f;
                    }
                }

                std::vector<int64_t> out_dims;
                const std::vector<float> result =
                    session_.run(input.data(), { 1, 3, th, tw }, &out_dims);

                if (out_dims.size() != 4 || out_dims[2] != (int64_t) th * factor_ ||
                    out_dims[3] != (int64_t) tw * factor_)
                    throw std::runtime_error("ONNX model returned an unexpected output size");

                // crop the margin back off and write the tile into place
                const int rw = tw * factor_;
                const size_t rplane = (size_t) rw * th * factor_;
                const int sx = (x0 - px0) * factor_;
                const int sy = (y0 - py0) * factor_;
                for (int y = 0; y < (y1 - y0) * factor_; ++y) {
                    unsigned char *orow = out + ((size_t)(y0 * factor_ + y) * ow + x0 * factor_) * channels;
                    const size_t r = (size_t)(sy + y) * rw + sx;
                    for (int x = 0; x < (x1 - x0) * factor_; ++x) {
                        orow[x * channels + 0] = to_u8(result[0 * rplane + r + x]);
                        orow[x * channels + 1] = to_u8(result[1 * rplane + r + x]);
                        orow[x * channels + 2] = to_u8(result[2 * rplane + r + x]);
                    }
                }
            }
        }

        // ---- transparency, which never went through the network
        if (channels == 4)
            enlargeAlpha(in, w, h, out);
    }

private:
    // Region of one axis handed to the network for the tile [a0, a1) of a
    // frame n pixels long: its start r0 and length rl.
    //
    // For a model taking any size, that is the tile grown by kTilePadding of
    // context on each side and clipped to the frame -- where the frame ends
    // there is nothing to add, and the network sees a real edge, which is
    // correct.
    //
    // For a model with a fixed input size, the region is exactly that size.
    // It is centred on the tile, and slid back inside the frame where it
    // would stick out, so that it overhangs only when the frame is smaller
    // than the model's input altogether.
    static void region(int a0, int a1, int n, int fixed, int &r0, int &rl)
    {
        if (fixed > 0) {
            r0 = std::clamp(a0 - kTilePadding, 0, std::max(0, n - fixed));
            rl = fixed;
        }
        else {
            r0 = std::max(a0 - kTilePadding, 0);
            rl = std::min(a1 + kTilePadding, n) - r0;
        }
    }

    // Index i of an axis n pixels long, mirrored back into it when i runs
    // past the end (i is never negative here). Mirroring continues the
    // texture of the frame, where repeating its last row or column would
    // hand the network a flat plateau it never met in training. Measured on
    // 4xNomos2 with a frame smaller than its input, the border of the result
    // came out 21 levels off with repeating and 12 with mirroring, against 4
    // for the same model on a real frame edge: better, not perfect, and only
    // ever for a frame smaller than the model's fixed input.
    static int mirror(int i, int n)
    {
        if (i < n)
            return i;
        if (n == 1)
            return 0;
        const int period = 2 * (n - 1);
        i %= period;
        return i < n ? i : period - i;
    }

    static unsigned char to_u8(float v)
    {
        return (unsigned char) std::lround(std::clamp(v, 0.f, 1.f) * 255.f);
    }

    // Tile side which keeps one tile's working set within kTileBudgetBytes.
    // The dominant term is the network's activations, which scale with the
    // pixels of the *input* region actually processed -- the tile together
    // with its context margin, which is what the budget has to cover.
    static int automaticTileSize()
    {
        const double padded = std::sqrt((double) kTileBudgetBytes / (double) kBytesPerInputPixel);
        const int tile = (int) padded - 2 * kTilePadding;
        return std::clamp(tile / 8 * 8, 64, 512);
    }

    // Bilinear enlargement of the alpha plane. The networks are colour only,
    // so transparency is carried around them -- which is also why every ONNX
    // model keeps it, with no per-model exceptions. A matte interpolates
    // perfectly well; there is no detail in it for a network to invent.
    void enlargeAlpha(const unsigned char *in, int w, int h, unsigned char *out)
    {
        const int ow = w * factor_;
        const int oh = h * factor_;

        for (int y = 0; y < oh; ++y) {
            const float sy = (y + 0.5f) / factor_ - 0.5f;
            const int y0 = std::max(0, (int) std::floor(sy));
            const int y1 = std::min(h - 1, y0 + 1);
            const float fy = std::max(0.f, sy - (float) y0);

            for (int x = 0; x < ow; ++x) {
                const float sx = (x + 0.5f) / factor_ - 0.5f;
                const int x0 = std::max(0, (int) std::floor(sx));
                const int x1 = std::min(w - 1, x0 + 1);
                const float fx = std::max(0.f, sx - (float) x0);

                const float a00 = in[((size_t) y0 * w + x0) * 4 + 3];
                const float a01 = in[((size_t) y0 * w + x1) * 4 + 3];
                const float a10 = in[((size_t) y1 * w + x0) * 4 + 3];
                const float a11 = in[((size_t) y1 * w + x1) * 4 + 3];
                const float top = a00 * (1.f - fx) + a01 * fx;
                const float bot = a10 * (1.f - fx) + a11 * fx;

                out[((size_t) y * ow + x) * 4 + 3] =
                    (unsigned char) std::lround(top * (1.f - fy) + bot * fy);
            }
        }
    }

    // Context kept around each tile, in source pixels, then cropped off the
    // result. Ten is what Real-ESRGAN uses for the same job; a little more
    // costs a little compute and leaves no room for doubt.
    static constexpr int kTilePadding = 16;

    // Activations allowed for one tile, and the most one pixel of input may
    // cost any model in kOnnxModels -- measured, with some headroom over the
    // dearest of them (2.6 KB). The whole catalogue is held to this figure,
    // which is what makes one tile size right for every model.
    static constexpr size_t kTileBudgetBytes = 512u << 20;
    static constexpr size_t kBytesPerInputPixel = 3u << 10;

    OnnxToolkit::Session session_;
    int factor_;
    int tile_ = 256;
    int fixed_w_ = 0;   // input size the model insists on, 0 when it takes any
    int fixed_h_ = 0;
    std::string description_;
};
#endif // HAVE_ONNX

}

// ============================================================ Engine

struct Upscaler::Engine::Impl {
    std::unique_ptr<UpscalerBackend> backend;
    std::string description;
};

Upscaler::Engine::Engine(const UpscalerModel &model, int tilesize)
    : impl_(new Impl)
{
    if (model.factor < 2 || model.file == nullptr)
        throw std::runtime_error("no upscaling model selected");

    // fetch the model on first use, from the repository of its family
    ensureModel(model);

    switch (backend()) {
#ifdef HAVE_NCNN
    case BACKEND_NCNN:
        impl_->backend = std::make_unique<UpscalerNCNN>(model, tilesize);
        break;
#endif
#ifdef HAVE_ONNX
    case BACKEND_ONNX:
        impl_->backend = std::make_unique<UpscalerONNX>(model, tilesize);
        break;
#endif
    default:
        throw std::runtime_error("no upscaling backend available on this machine");
    }

    impl_->description = impl_->backend->describe();
}

Upscaler::Engine::~Engine()
{
}

void Upscaler::Engine::process(const unsigned char *in, int w, int h, unsigned char *out,
                               int channels)
{
    if (channels != 3 && channels != 4)
        throw std::runtime_error("unsupported number of channels: " + std::to_string(channels));

    impl_->backend->process(in, w, h, out, channels);
}

const std::string &Upscaler::Engine::describe() const
{
    return impl_->description;
}
