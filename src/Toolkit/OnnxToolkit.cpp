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
// settle for the CPU one otherwise. 
//
// oneDNN (DNNL) is deliberately NOT used, although this onnxruntime ships it.
// It is both slower than onnxruntime's own CPU kernels and 8 to 14 times 
// hungrier for memory. Do not re-enable it without testing more.
std::string append_best_provider(Ort::SessionOptions &opts)
{
#if defined(__APPLE__)
    try {
        // Apple Neural Engine / GPU through CoreML. Unlike DNNL above this
        // has not been measured: watch memory when upscaling on a Mac.
        opts.AppendExecutionProvider("CoreML", {});
        return "CoreML";
    } catch (const std::exception &) {}
#else
    (void) opts;
#endif
    return "CPU";
}

}

OnnxToolkit::Session::Session(const std::string &model_path)
    : impl_(new Impl)
{
    // ONNX Runtime prints a wall of schema registration warnings the first
    // time its registry is populated; hide that noise
    SystemToolkit::StderrSilencer hush;

    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    impl_->provider = append_best_provider(opts);

    impl_->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "vimix");
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

OnnxToolkit::Session::Session(const std::string &)
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
