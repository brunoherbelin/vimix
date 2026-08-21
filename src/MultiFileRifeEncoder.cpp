#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>

#ifdef HAVE_NCNN
// rife-ncnn-vulkan in ext/rife-ncnn-vulkan/src
#include "rife.h"
#include "gpu.h"
#endif

#ifdef HAVE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>

#include "Log.h"
#include "Settings.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"

#include "MultiFileRifeEncoder.h"


namespace fs = std::filesystem;

// Everything below is internal to the encoder
namespace {

// ---------------------------------------------------------------- frames

// Planar float RGB, values 0..1, layout [3][h][w] (matches ONNX tensors).
struct Frame {
    int w = 0, h = 0;
    std::vector<float> chw;
};

// Frame -> tightly packed interleaved RGB u8 (the layout ncnn's RIFE
// pre/postprocessing shaders consume and produce).
std::vector<unsigned char> frame_to_rgb8(const Frame &f)
{
    size_t plane = (size_t)f.w * f.h;
    std::vector<unsigned char> rgb(plane * 3);
    for (size_t i = 0; i < plane; i++) {
        rgb[3 * i + 0] = (unsigned char)std::lround(std::clamp(f.chw[0 * plane + i], 0.0f, 1.0f) * 255.0f);
        rgb[3 * i + 1] = (unsigned char)std::lround(std::clamp(f.chw[1 * plane + i], 0.0f, 1.0f) * 255.0f);
        rgb[3 * i + 2] = (unsigned char)std::lround(std::clamp(f.chw[2 * plane + i], 0.0f, 1.0f) * 255.0f);
    }
    return rgb;
}

Frame rgb8_to_frame(const unsigned char *rgb, int w, int h)
{
    Frame f;
    f.w = w;
    f.h = h;
    size_t plane = (size_t)w * h;
    f.chw.resize(plane * 3);
    for (size_t i = 0; i < plane; i++) {
        f.chw[0 * plane + i] = rgb[3 * i + 0] / 255.0f;
        f.chw[1 * plane + i] = rgb[3 * i + 1] / 255.0f;
        f.chw[2 * plane + i] = rgb[3 * i + 2] / 255.0f;
    }
    return f;
}

// ------------------------------------------------------------ image input

// Decode one image file to RGB with GStreamer, scaling to w x h
// (pass 0,0 to keep the image's own size).
//
// The one-shot pipeline
//   filesrc ! decodebin ! videoconvert ! videoscale ! capsfilter ! appsink
// autoplugs the right decoder (pngdec/jpegdec/...), normalises to packed RGB
// at the requested size (add-borders letterboxes rather than stretching or
// cropping), and appsink hands the decoded buffer to us. gst_video_frame_map
// exposes the pixels with their row stride — rows may be padded, so we must
// not assume width*3 bytes per row.
Frame decode_image(const std::string &path, int w, int h)
{
    gchar *quoted = g_strescape(path.c_str(), nullptr);
    std::string desc = "filesrc location=\"" + std::string(quoted) +
                       "\" ! decodebin ! videoconvert ! videoscale add-borders=true ! "
                       "video/x-raw,format=RGB,colorimetry=(string)sRGB,pixel-aspect-ratio=1/1";
    g_free(quoted);
    if (w > 0)
        desc += ",width=" + std::to_string(w) + ",height=" + std::to_string(h);
    desc += " ! appsink name=sink sync=false";

    GError *err = nullptr;
    GstElement *pipe = gst_parse_launch(desc.c_str(), &err);
    if (!pipe) {
        std::string msg = err ? err->message : "unknown";
        if (err) g_error_free(err);
        throw std::runtime_error("gst_parse_launch: " + msg);
    }
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipe), "sink");

    // Mute video-info category warnings while the pipeline negotiates and decodes.
    GstToolkit::GstCategoryHush hush("video-info", GST_LEVEL_ERROR);
    gst_element_set_state(pipe, GST_STATE_PLAYING);

    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        gst_element_set_state(pipe, GST_STATE_NULL);
        gst_object_unref(sink);
        gst_object_unref(pipe);
        throw std::runtime_error("failed to decode " + path);
    }

    GstCaps *caps = gst_sample_get_caps(sample);
    GstVideoInfo info;
    gst_video_info_from_caps(&info, caps);

    GstVideoFrame vframe;
    gst_video_frame_map(&vframe, &info, gst_sample_get_buffer(sample), GST_MAP_READ);

    Frame f;
    f.w = GST_VIDEO_INFO_WIDTH(&info);
    f.h = GST_VIDEO_INFO_HEIGHT(&info);
    f.chw.resize((size_t)3 * f.w * f.h);

    const guint8 *pixels = (const guint8 *)GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0);
    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0);
    size_t plane = (size_t)f.w * f.h;
    for (int y = 0; y < f.h; y++) {
        const guint8 *row = pixels + (size_t)y * stride;
        for (int x = 0; x < f.w; x++) {
            size_t i = (size_t)y * f.w + x;
            f.chw[0 * plane + i] = row[3 * x + 0] / 255.0f;
            f.chw[1 * plane + i] = row[3 * x + 1] / 255.0f;
            f.chw[2 * plane + i] = row[3 * x + 2] / 255.0f;
        }
    }

    gst_video_frame_unmap(&vframe);
    gst_sample_unref(sample);
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pipe);
    return f;
}

// ---------------------------------------------------------- model download

const char *kOnnxModel = "models/RIFE_fp32.onnx";
const char *kOnnxModelUrl =
    "https://huggingface.co/FuryTMP/RIFE_fp32/resolve/main/RIFE_fp32.onnx";

// ncnn models are platform-independent data (.param = text graph
// description, .bin = raw weights), identical in every release artifact —
// so fetch the two files directly, pinned to the release tag.
const char *kNcnnModelDir = "models/rife-v4.6";
const char *kNcnnModelBaseUrl =
    "https://raw.githubusercontent.com/nihui/rife-ncnn-vulkan/20221029/models/rife-v4.6/";


// ---------------------------------------------------- inference backends

class RifeBackend {
public:
    virtual ~RifeBackend() = default;
    virtual const char *describe() const = 0;
    // frame at time t (0..1) between a and b; t=0.5 always supported,
    // other values only if arbitrary_t() is true
    virtual Frame at(const Frame &a, const Frame &b, float t) = 0;
    virtual bool arbitrary_t() const = 0;
};

// Pass-through "backend" used when no inference backend is built or usable:
// the driver forces mid=0, so at() is never called in practice.
class RifeDummy : public RifeBackend {
public:
    const char *describe() const override { return "none (no interpolation)"; }
    bool arbitrary_t() const override { return true; }
    Frame at(const Frame &a, const Frame &, float) override { return a; }
};

#ifdef HAVE_NCNN
// -------------------------------------------- inference: ncnn / Vulkan GPU
//
// Wraps the RIFE class from ext/rife-ncnn-vulkan: ncnn networks (flownet)
// plus custom Vulkan compute shaders (warp op, pre/postprocessing). Model
// rife-v4.6 takes the interpolation time as an input, so any t works.
// Images are passed as ncnn::Mat views over interleaved RGB u8 buffers with
// elemsize=elempack=3; the preproc/postproc shaders do the u8<->float
// conversion on the GPU.
class RifeNCNN : public RifeBackend {
public:
    explicit RifeNCNN(const std::string &modeldir)
    {
        {
            // ncnn prints a GPU enumeration to stderr here; hide it
            SystemToolkit::StderrSilencer silence;
            ncnn::create_gpu_instance();
        }
        if (ncnn::get_gpu_count() == 0) {
            ncnn::destroy_gpu_instance();
            throw std::runtime_error("no Vulkan device available");
        }
        int gpuid = ncnn::get_default_gpu_index();
        desc_ = "Vulkan GPU (" +
                std::string(ncnn::get_gpu_info(gpuid).device_name()) + ")";

        int threads = std::max(1u, std::thread::hardware_concurrency() / 2);
        rife_ = std::make_unique<RIFE>(gpuid,
                                       /*tta_mode=*/false,
                                       /*tta_temporal_mode=*/false,
                                       /*uhd_mode=*/false,
                                       threads,
                                       /*rife_v2=*/false,
                                       /*rife_v4=*/true);
        if (rife_->load(modeldir) != 0)
            throw std::runtime_error("failed to load ncnn model " + modeldir);
    }

    ~RifeNCNN() override
    {
        rife_.reset();
        ncnn::destroy_gpu_instance();
    }

    const char *describe() const override { return desc_.c_str(); }
    bool arbitrary_t() const override { return true; }

    Frame at(const Frame &a, const Frame &b, float t) override
    {
        std::vector<unsigned char> in0v = frame_to_rgb8(a);
        std::vector<unsigned char> in1v = frame_to_rgb8(b);
        std::vector<unsigned char> outv((size_t)a.w * a.h * 3);

        ncnn::Mat in0(a.w, a.h, (void *)in0v.data(), (size_t)3, 3);
        ncnn::Mat in1(a.w, a.h, (void *)in1v.data(), (size_t)3, 3);
        ncnn::Mat out(a.w, a.h, (void *)outv.data(), (size_t)3, 3);

        if (rife_->process(in0, in1, t, out) != 0)
            throw std::runtime_error("ncnn RIFE processing failed");
        return rgb8_to_frame(outv.data(), a.w, a.h);
    }

private:
    std::unique_ptr<RIFE> rife_;
    std::string desc_;
};
#endif // HAVE_NCNN

#ifdef HAVE_ONNX
// ------------------------------------------------ inference: ONNX Runtime
//
// CPU-only fallback. An .onnx file is a serialized compute graph; ONNX
// Runtime loads it once into a session and runs it by feeding/collecting
// tensors. Model contract (FuryTMP/RIFE_fp32): input float32 [1,6,H,W] NCHW
// in 0..1 (channels 0-2 frame A, 3-5 frame B); output [1,3,H,W] at t=0.5
// only, hence bisection in the driver.
// 
class RifeONNX : public RifeBackend {
public:
    explicit RifeONNX(const std::string &model_path)
    {
        // Registration warnings fire while the schema registry is first
        // populated below (Env/Session creation); suppress that stderr noise.
        SystemToolkit::StderrSilencer hush;

        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
        // Ort::Env is the library-wide context (logger + thread pools); it must
        // outlive the session, which member declaration/destruction order
        // guarantees (env_ declared before session_, destroyed after it).
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "gst-rife");
        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), opts);

        // inputs/outputs are addressed by name; query instead of hard-coding
        Ort::AllocatorWithDefaultOptions alloc;
        in_name_ = session_->GetInputNameAllocated(0, alloc).get();
        out_name_ = session_->GetOutputNameAllocated(0, alloc).get();
    }

    const char *describe() const override { return "CPU (onnxruntime)"; }
    bool arbitrary_t() const override { return false; }

    Frame at(const Frame &a, const Frame &b, float) override
    {
        // build [1,6,H,W]: frame A's planes then frame B's
        size_t plane3 = a.chw.size();
        std::vector<float> input(2 * plane3);
        std::copy(a.chw.begin(), a.chw.end(), input.begin());
        std::copy(b.chw.begin(), b.chw.end(), input.begin() + plane3);

        // CreateTensor with a pointer is a view over `input` (no copy);
        // `input` must outlive Run()
        int64_t shape[4] = {1, 6, a.h, a.w};
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value in = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), shape, 4);

        const char *ins[] = {in_name_.c_str()};
        const char *outs[] = {out_name_.c_str()};
        auto result = session_->Run(Ort::RunOptions{nullptr}, ins, &in, 1, outs, 1);

        Frame m;
        m.w = a.w;
        m.h = a.h;
        const float *out = result[0].GetTensorData<float>();
        m.chw.assign(out, out + plane3);
        return m;
    }

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string in_name_, out_name_;
};
#endif // HAVE_ONNX

// Pick the inference backend: ncnn (Vulkan GPU) preferred, onnx (CPU) as
// fallback; either can be forced. Backends not compiled in (HAVE_NCNN/
// HAVE_ONNX) can neither be probed nor forced. Returns RifeDummy — no
// interpolation — when nothing is available.
std::unique_ptr<RifeBackend> make_backend(const std::string &backend, std::string *message = nullptr)
{
    std::unique_ptr<RifeBackend> rife;

#ifdef HAVE_NCNN
    if (backend != "onnx") {
        try {
            // Make sure the ncnn model files exist; fetch any missing one from the
            // pinned rife-ncnn-vulkan repository tag.
            std::string  _path = SystemToolkit::full_filename(SystemToolkit::settings_path(), kNcnnModelDir);
            for (const char *name : {"flownet.param", "flownet.bin"}) {
                std::string dest = SystemToolkit::full_filename(_path, name);
                if (!fs::exists(dest)) {
                    (*message) = "Downloading NCNN RIFE model...";
                    GstToolkit::download_file(std::string(kNcnnModelBaseUrl) + name, dest);
                }
            }
            // initialize the RIFE inference backend (Vulkan GPU)
            (*message) = "Using NCNN RIFE model on Vulkan GPU ";
            rife = std::make_unique<RifeNCNN>(_path);
        } catch (const std::exception &e) {
            if (backend == "ncnn") {
                throw;
                Log::Warning("Image Sequence: ncnn backend unavailable (%s)\n", e.what());
            }
#ifndef HAVE_ONNX
            else
                Log::Info("Image Sequence: ncnn backend unavailable (%s)\n", e.what());
#endif            
        }
    }
#else
    if (backend == "ncnn")
        throw std::runtime_error("this build has no ncnn backend");
#endif

#ifdef HAVE_ONNX
    if (!rife && backend != "ncnn") {
        // Make sure the ONNX model file exists; fetch the default one from Hugging Face if missing.
        std::string  _path = SystemToolkit::full_filename(SystemToolkit::settings_path(), kOnnxModel);
        if (!fs::exists(_path)) {
            (*message) = "Downloading ONNX RIFE model...";
            GstToolkit::download_file(kOnnxModelUrl, _path);
        }
        // initialize the RIFE inference backend (ONNX Runtime CPU)
        (*message) = "Using ONNX RIFE model";
        rife = std::make_unique<RifeONNX>(_path);
    }
#else
    if (backend == "onnx")
        throw std::runtime_error("this build has no onnx backend");
#endif

    if (!rife)
        rife = std::make_unique<RifeDummy>();
    return rife;
}

// ---------------------------------------------------------------- driver

using FrameSink = std::function<void(const Frame &)>;

// Emit the 2^depth - 1 intermediate frames between a and b, in temporal
// order, by recursive t=0.5 bisection: an in-order traversal of a binary
// tree. Preferred whenever --mid = 2^k - 1 (RIFE predicts the midpoint best).
void emit_bisect(RifeBackend &rife, const Frame &a, const Frame &b,
                 int depth, const FrameSink &push)
{
    if (depth == 0)
        return;
    Frame m = rife.at(a, b, 0.5f);
    emit_bisect(rife, a, m, depth - 1, push);
    push(m);
    emit_bisect(rife, m, b, depth - 1, push);
}

// Emit mid intermediate frames at evenly spaced timesteps (arbitrary-t
// backends only).
void emit_timesteps(RifeBackend &rife, const Frame &a, const Frame &b,
                    int mid, const FrameSink &push)
{
    for (int i = 1; i <= mid; i++)
        push(rife.at(a, b, (float)i / (mid + 1)));
}

guint8 to_u8(float v)
{
    return (guint8)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}

} // namespace

// ============================================================ public class

MultiFileRifeEncoder::MultiFileRifeEncoder()
    : started_(false)
    , finished_(false)
    , success_(false)
    , abort_(false)
    , total_frames_(0)
    , done_frames_(0)
{
    // Output filename is generated in start()
}

MultiFileRifeEncoder::~MultiFileRifeEncoder()
{
    stop();
    if (worker_.joinable())
        worker_.join();
}

std::string MultiFileRifeEncoder::generateOutputFilename(const std::list<std::string> &files)
{
    std::string base = std::string();
    base = BaseToolkit::common_prefix (files);
    if (!SystemToolkit::path_directory(base).empty())
        base += "image";

    std::string output = base + "_rife.mov";
    int counter = 1;
    while ( SystemToolkit::file_exists(output ) ) {
        output = base + "_rife_" + std::to_string(counter) + ".mov";
        counter++;
    }
    return output;
}

bool MultiFileRifeEncoder::start(const RifeOptions &options)
{
    if (started_) {
        message_ = "encoder already started";
        return false;
    }
    if (files_.size() < 2) {
        message_ = "not enough input files";
        return false;
    }
    if (options.mid < 0) {
        message_ = "invalid --mid";
        return false;
    }
    if (options.fps <= 0) {
        message_ = "invalid --fps";
        return false;
    }
    if (options.backend != "auto" && options.backend != "ncnn" &&
        options.backend != "onnx") {
        message_ = "unknown backend '" + options.backend + "'";
        return false;
    }

    output_filename_ = generateOutputFilename(files_);

    started_ = true;
    finished_ = false;
    success_ = false;
    abort_ = false;
    total_frames_ = 0;
    done_frames_ = 0;

    // Reclaim the thread object from a previous run before reusing it
    if (worker_.joinable())
        worker_.join();

    // launch worker thread to run() in the background, passing a copy of options
    worker_ = std::thread(&MultiFileRifeEncoder::run, this, options);

    return true;
}

void MultiFileRifeEncoder::stop()
{
    if (!started_ || finished_)
        return;

    abort_ = true;
    if (worker_.joinable())
        worker_.join();

    if (!success_ && !output_filename_.empty()) {
        if (SystemToolkit::remove_file(output_filename_))
            Log::Info("removed incomplete output file %s\n", output_filename_.c_str());
    }
}

void MultiFileRifeEncoder::reset() 
{
    files_.clear();
    started_ = false;
}

bool MultiFileRifeEncoder::finished() const
{
    return finished_ && files_.size() > 0;
}

bool MultiFileRifeEncoder::success() const
{
    return success_;
}

double MultiFileRifeEncoder::progress() const
{
    if (!started_ || finished_)
        return finished_ ? 1.0 : 0.0;

    int t = total_frames_, d = done_frames_;
    if (t > 0)
        return std::min(1.0, static_cast<double>(d) / static_cast<double>(t));
    return 0.0;
}

// Background processing: list images -> decode/interpolate -> encode.
void MultiFileRifeEncoder::run(RifeOptions options)
{
    GstElement *enc_pipe = nullptr, *src = nullptr;

    try {
        if (files_.size() < 2)
            throw std::runtime_error("need at least 2 images selected");
        int mid = options.mid;
        Log::Info("ImageSequence: %zu images, %d intermediate frame(s) per pair, %d fps",
                files_.size(), mid, options.fps);

        // choose the inference backend (downloads its model on first use)
        std::unique_ptr<RifeBackend> rife;

#if defined(HAVE_NCNN) || defined(HAVE_ONNX)
        if (mid > 0) {
            message_ = "Initializing AI Model...";
            rife = make_backend(options.backend, &message_);
            if (dynamic_cast<RifeDummy *>(rife.get())) {
                Log::Warning("ImageSequence: no interpolation backend available, "
                             "encoding images only");
                mid = 0;
            } else {
                Log::Info("ImageSequence: inference: %s", rife->describe());
            }
        }
#else
        rife = std::make_unique<RifeDummy>();
        mid = 0;
#endif

        if (mid == 0) {
            Log::Info("ImageSequence: encoding sequence with given images");
            message_ = "";
        }

        // interpolation mode: recursive t=0.5 bisection when mid = 2^k - 1
        // (smoother — RIFE predicts the midpoint best), else evenly spaced
        // timesteps (needs an arbitrary-t backend)
        int bisect_depth = 0;
        if (mid > 0) {
            while ((1 << bisect_depth) - 1 < mid) bisect_depth++;
            if ((1 << bisect_depth) - 1 != mid) {
                bisect_depth = 0;
                if (!rife->arbitrary_t())
                    throw std::runtime_error("mid must be 2^k - 1 (1, 3, 7, ...) "
                                             "with the onnx backend");
            }
            Log::Info("ImageSequence: interpolation: %s",
                    bisect_depth ? "recursive bisection (t=0.5)"
                                 : "evenly spaced timesteps");
        }

        // first image fixes the geometry (rounded even for the encoders)
        Frame first = decode_image(files_.front(), 0, 0);
        int w = first.w & ~1, h = first.h & ~1;
        if (w != first.w || h != first.h)
            first = decode_image(files_.front(), w, h);

        total_frames_ = (int)files_.size() + (int)(files_.size() - 1) * mid;

        // create a gstreamer pipeline
        std::string desc = "appsrc name=src format=time ! queue ! videoconvert ! videoscale ! ";

        // test for a hardware accelerated encoder
        if (Settings::application.render.gpu_decoding && (int) VideoRecorder::hardware_encoder.size() > 0 &&
            GstToolkit::has_feature(VideoRecorder::hardware_encoder[options.profile]) ) {

            desc += VideoRecorder::hardware_profile_description[Settings::application.image_sequence.profile];
        }
        // revert to software encoder
        else
            desc += VideoRecorder::profile_description[options.profile];
#ifndef NDEBUG
        Log::Info("ImageSequence: encoding pipeline '%s'", desc.c_str());
#endif
        // qt muxer in .mov file
        desc += "qtmux ! filesink location=\"" + output_filename_ + "\"";

        GError *err = nullptr;
        enc_pipe = gst_parse_launch(desc.c_str(), &err);
        if (!enc_pipe) {
            std::string msg = err ? err->message : "unknown";
            if (err) g_error_free(err);
            throw std::runtime_error("gst_parse_launch: " + msg);
        }
        src = gst_bin_get_by_name(GST_BIN(enc_pipe), "src");

        GstVideoInfo vinfo;
        gst_video_info_set_format(&vinfo, GST_VIDEO_FORMAT_RGB, w, h);
        vinfo.fps_n = options.fps;
        vinfo.fps_d = 1;
        GstCaps *caps = gst_video_info_to_caps(&vinfo);
        g_object_set(src, "caps", caps, nullptr);
        gst_caps_unref(caps);
        gst_element_set_state(enc_pipe, GST_STATE_PLAYING);

        // push one RGB frame into the encoder (restore row alignment; PTS
        // drives the output framerate)
        int fps = options.fps;
        auto push = [&](const Frame &f) {
            GstBuffer *buf = gst_buffer_new_allocate(nullptr, vinfo.size, nullptr);
            gst_buffer_add_video_meta(buf, GST_VIDEO_FRAME_FLAG_NONE,
                                      GST_VIDEO_FORMAT_RGB, w, h);
            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_WRITE);
            size_t plane = (size_t)w * h;
            int stride = GST_VIDEO_INFO_PLANE_STRIDE(&vinfo, 0);
            for (int y = 0; y < h; y++) {
                guint8 *row = map.data + (size_t)y * stride;
                for (int x = 0; x < w; x++) {
                    size_t i = (size_t)y * w + x;
                    row[3 * x + 0] = to_u8(f.chw[0 * plane + i]);
                    row[3 * x + 1] = to_u8(f.chw[1 * plane + i]);
                    row[3 * x + 2] = to_u8(f.chw[2 * plane + i]);
                }
            }
            gst_buffer_unmap(buf, &map);

            int n = done_frames_;
            GST_BUFFER_PTS(buf) = gst_util_uint64_scale(n, GST_SECOND, fps);
            GST_BUFFER_DURATION(buf) = gst_util_uint64_scale(1, GST_SECOND, fps);
            if (gst_app_src_push_buffer(GST_APP_SRC(src), buf) != GST_FLOW_OK)
                throw std::runtime_error("failed to push frame to encoder");
            done_frames_ = n + 1;
        };

        // -------- frame loop
        push(first);
        Frame prev = std::move(first);
        // for (size_t i = 1; i < files_.size() && !abort_; i++) {
        for (auto i = files_.begin(); i != files_.end() && !abort_; ++i) {
            Frame cur = decode_image(*i, w, h);
            if (mid > 0) {
                if (bisect_depth)
                    emit_bisect(*rife, prev, cur, bisect_depth, push);
                else
                    emit_timesteps(*rife, prev, cur, mid, push);
            }
            push(cur);
            prev = std::move(cur);
        }
        if (abort_)
            throw std::runtime_error("encoding stopped by user");

        // -------- finalize: EOS and wait for the muxer to write its index
        message_ = "Saving output file...";
        gst_app_src_end_of_stream(GST_APP_SRC(src));
        GstBus *bus = gst_element_get_bus(enc_pipe);
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
            (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        bool ok = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        if (!ok) {
            GError *e = nullptr;
            gst_message_parse_error(msg, &e, nullptr);
            std::string what = e ? e->message : "unknown";
            if (e) g_error_free(e);
            gst_message_unref(msg);
            gst_object_unref(bus);
            throw std::runtime_error("encoding failed: " + what);
        }
        gst_message_unref(msg);
        gst_object_unref(bus);

        Log::Info("ImageSequence: wrote %s (%d frames, %.2f s)", output_filename_.c_str(),
                  (int)done_frames_, (double)done_frames_ / fps);
        success_ = true;

    } catch (const std::exception &e) {
        // record the error; the caller surfaces it via error()
        message_ = e.what();
        success_ = false;
    }

    if (enc_pipe) {
        gst_element_set_state(enc_pipe, GST_STATE_NULL);
        if (src) gst_object_unref(src);
        gst_object_unref(enc_pipe);
    }

    // exit
    finished_ = true;
}
