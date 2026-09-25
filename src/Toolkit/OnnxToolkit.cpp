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

#ifdef HAVE_ONNX
#include <numeric>
#include <unordered_map>
#include <onnxruntime_cxx_api.h>
#endif

#include "OnnxToolkit.h"
#include "SystemToolkit.h"


bool OnnxToolkit::available()
{
#ifdef HAVE_ONNX
    return true;
#else
    return false;
#endif
}

#ifdef HAVE_ONNX

struct OnnxToolkit::Session::Impl {
    // Ort::Env is the library-wide context (logger and thread pools) and has
    // to outlive the session, which member declaration order guarantees:
    // env_ is declared first, so it is destroyed last.
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::string input_name, output_name, provider;
};

namespace {

// Ask for an accelerated execution provider where one is worth having, and
// settle for the CPU one otherwise. Returns the name of the provider, and
// tells in `accelerated` whether it is not the plain CPU one.
//
// oneDNN (DNNL in Linux) is deliberately NOT used.
// It is both slower than onnxruntime's own CPU kernels and 8 to 14 times 
// hungrier for memory. Do not re-enable it without testing more.
//
// `static_shape` tells that the input will be of one fixed size, which is
// what lets OSX CoreML compile the model for the GPU or the Neural Engine at
// all; it then builds an ML Program, measured twice as fast as its default
// NeuralNetwork format on the upscalers.
std::string append_best_provider(Ort::SessionOptions &opts, bool static_shape, bool &accelerated)
{
    accelerated = false;
#if defined(__APPLE__)
    // Apple GPU / Neural Engine through CoreML
    std::unordered_map<std::string, std::string> coreml;
    std::string name = "CoreML";
    if (static_shape) {
        coreml["ModelFormat"] = "MLProgram";
        name += " (MLProgram)";
    }

    try {
        opts.AppendExecutionProvider("CoreML", coreml);
        accelerated = true;
        return name;
    } catch (const std::exception &) {}
#else
    (void) opts;
    (void) static_shape;
#endif
    return "CPU";
}

// Would the session gain from a static input shape? Only with CoreML, which
// cannot compile a model with an unbounded dimension for the GPU or the
// Neural Engine and leaves it all on the CPU. Any other provider -- the CPU
// one everywhere else -- takes any size just as fast, and pinning it would
// only cost full size tiles on the edges of the frame.
bool wants_static_shape()
{
#if defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

// Free dimensions of the model's input, by name, set to the values given in
// `shape` at their position. Returns nothing when the input is already
// static, or when a dimension cannot be pinned -- it has no name, or no
// value was given for it -- as the shape would not be static anyway.
std::vector<std::pair<std::string, int64_t>> free_dimensions(Ort::Env &env,
                                                             const std::string &model_path,
                                                             const std::vector<int64_t> &shape)
{
    std::vector<std::pair<std::string, int64_t>> pins;

    // the symbolic names are only told by a loaded model; load it raw, as
    // quickly as possible, just to read them
    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(ORT_DISABLE_ALL);
    Ort::Session probe(env, model_path.c_str(), opts);
    // the shape info is a view into the type info, which has to stay alive
    const Ort::TypeInfo type = probe.GetInputTypeInfo(0);
    const auto info = type.GetTensorTypeAndShapeInfo();
    const std::vector<int64_t> dims = info.GetShape();
    std::vector<const char *> names(dims.size(), nullptr);
    info.GetSymbolicDimensions(names.data(), names.size());

    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i] > 0)
            continue;
        if (i >= shape.size() || shape[i] <= 0 || names[i] == nullptr || *names[i] == '\0')
            return {};
        pins.emplace_back(names[i], shape[i]);
    }
    return pins;
}

}

OnnxToolkit::Session::Session(const std::string &model_path,
                              const std::vector<int64_t> &static_shape)
    : impl_(new Impl)
{
    // ONNX Runtime prints a wall of schema registration warnings the first
    // time its registry is populated; hide that noise
    SystemToolkit::StderrSilencer hush;

    impl_->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "vimix");

    // the free dimensions to pin, when a static shape is asked for and the
    // provider gains from it
    std::vector<std::pair<std::string, int64_t>> pins;
    if (!static_shape.empty() && wants_static_shape())
        pins = free_dimensions(*impl_->env, model_path, static_shape);

    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    bool accelerated = false;
    impl_->provider = append_best_provider(opts, !pins.empty(), accelerated);
    if (accelerated) {
        for (const auto &pin : pins)
            Ort::ThrowOnError(Ort::GetApi().AddFreeDimensionOverrideByName(opts, pin.first.c_str(), pin.second));
    }

    impl_->session = std::make_unique<Ort::Session>(*impl_->env, model_path.c_str(), opts);

    // inputs and outputs are addressed by name; query instead of hard-coding
    Ort::AllocatorWithDefaultOptions alloc;
    impl_->input_name = impl_->session->GetInputNameAllocated(0, alloc).get();
    impl_->output_name = impl_->session->GetOutputNameAllocated(0, alloc).get();
}

OnnxToolkit::Session::~Session()
{
}

size_t OnnxToolkit::Session::inputCount() const
{
    return impl_->session->GetInputCount();
}

std::vector<int64_t> OnnxToolkit::Session::inputShape() const
{
    return impl_->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
}

const std::string &OnnxToolkit::Session::describe() const
{
    return impl_->provider;
}

std::vector<float> OnnxToolkit::Session::run(const float *data, const std::vector<int64_t> &shape,
                                             std::vector<int64_t> *out_shape)
{
    const size_t count = std::accumulate(shape.begin(), shape.end(), (size_t) 1,
                                         [](size_t a, int64_t d) { return a * (size_t) d; });

    // CreateTensor over a pointer is a view, not a copy: `data` has to stay
    // alive until Run() returns, which it does, being the caller's buffer
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value in = Ort::Value::CreateTensor<float>(mem, const_cast<float *>(data), count,
                                                    shape.data(), shape.size());

    const char *ins[] = { impl_->input_name.c_str() };
    const char *outs[] = { impl_->output_name.c_str() };

    auto result = impl_->session->Run(Ort::RunOptions{ nullptr }, ins, &in, 1, outs, 1);

    const auto info = result[0].GetTensorTypeAndShapeInfo();
    const std::vector<int64_t> dims = info.GetShape();
    if (out_shape)
        *out_shape = dims;

    const float *out = result[0].GetTensorData<float>();
    return std::vector<float>(out, out + info.GetElementCount());
}

#else // HAVE_ONNX

// Without the backend there is nothing to load: the constructor always
// throws, so no Session ever exists and the other methods are unreachable.
struct OnnxToolkit::Session::Impl { std::string provider; };

OnnxToolkit::Session::Session(const std::string &, const std::vector<int64_t> &)
{
    throw std::runtime_error("this build has no ONNX Runtime backend");
}

OnnxToolkit::Session::~Session()
{
}

size_t OnnxToolkit::Session::inputCount() const
{
    return 0;
}

std::vector<int64_t> OnnxToolkit::Session::inputShape() const
{
    return std::vector<int64_t>();
}

const std::string &OnnxToolkit::Session::describe() const
{
    static const std::string none;
    return none;
}

std::vector<float> OnnxToolkit::Session::run(const float *, const std::vector<int64_t> &,
                                             std::vector<int64_t> *)
{
    return std::vector<float>();
}

#endif // HAVE_ONNX
