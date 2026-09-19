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
#ifndef ONNXTOOLKIT_H
#define ONNXTOOLKIT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace OnnxToolkit
{

/**
 * @brief True when this build has the ONNX Runtime inference backend
 */
bool available();

/**
 * @brief One loaded ONNX model, ready to run
 *
 * Wraps the handful of ONNX Runtime objects every user of a model needs --
 * the library-wide environment, the session, the input and output names --
 * so that the neural networks in vimix (the RIFE frame interpolator and the
 * upscaler) do not each repeat them. It deliberately exposes only what they
 * have in common: a model with one input tensor and one output tensor.
 *
 * Constructing it loads the file, which throws std::runtime_error if the
 * model is missing or malformed, and selects the fastest execution provider
 * this machine offers, quietly falling back to plain CPU.
 *
 * Not thread safe: one session belongs to the thread which created it.
 */
class Session
{
public:
    explicit Session(const std::string &model_path);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    /**
     * @brief How many input tensors the model expects
     *
     * run() feeds exactly one, so a model wanting more cannot be driven from
     * here; callers should check this and report it rather than failing deep
     * inside the inference.
     */
    size_t inputCount() const;

    /**
     * @brief Declared shape of the first input, -1 for a dynamic dimension
     *
     * The channel count is usually the one fixed dimension, and it is what
     * tells a caller which variant of a model family it has been given.
     */
    std::vector<int64_t> inputShape() const;

    /**
     * @brief Run the model on one tensor
     * @param data Tensor contents, `shape` elements in row major order
     * @param shape Dimensions of the input tensor
     * @param out_shape Receives the shape of the result, when not null
     * @return The output tensor contents
     *
     * Throws std::runtime_error if the inference fails, with the message
     * ONNX Runtime reported.
     */
    std::vector<float> run(const float *data, const std::vector<int64_t> &shape,
                           std::vector<int64_t> *out_shape = nullptr);

    /**
     * @brief Name of the execution provider in use, for logging
     */
    const std::string &describe() const;

private:
    // ONNX Runtime types are kept out of this header
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}

#endif // ONNXTOOLKIT_H
