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
#ifndef NCNNTOOLKIT_H
#define NCNNTOOLKIT_H

#include <string>

namespace NcnnToolkit
{

/**
 * @brief RAII handle on the process-wide ncnn Vulkan instance.
 *
 * ncnn::create_gpu_instance() is idempotent but ncnn::destroy_gpu_instance()
 * is not reference counted: it tears the instance down unconditionally. Two
 * concurrent users -- the RIFE frame interpolator and the Real-ESRGAN
 * upscaler both run in their own worker thread and can overlap -- would
 * therefore pull the instance out from under each other. Every user holds
 * one of these instead: the instance is created on the first acquisition and
 * destroyed only when the last holder is released.
 *
 * Constructing throws std::runtime_error when this build has no ncnn backend
 * or when no Vulkan device is available.
 */
class GpuInstance
{
public:
    GpuInstance();
    ~GpuInstance();
    GpuInstance(const GpuInstance &) = delete;
    GpuInstance &operator=(const GpuInstance &) = delete;

    // index of the default Vulkan device, to pass to ncnn model wrappers
    int index() const { return index_; }

    // human readable name of that device, e.g. "NVIDIA GeForce RTX 3060"
    const std::string &name() const { return name_; }

private:
    int index_;
    std::string name_;
};

}

#endif // NCNNTOOLKIT_H
