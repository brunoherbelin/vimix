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

#include <mutex>
#include <stdexcept>

#ifdef HAVE_NCNN
#include "gpu.h"
#endif

#include "SystemToolkit.h"

#include "NcnnToolkit.h"

namespace {

// guards both the counter and the create/destroy calls themselves
std::mutex g_mutex;
int g_refcount = 0;

}

NcnnToolkit::GpuInstance::GpuInstance() : index_(-1)
{
#ifdef HAVE_NCNN
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_refcount == 0) {
        // ncnn prints its GPU enumeration to stderr here; hide it
        SystemToolkit::StderrSilencer silence;
        ncnn::create_gpu_instance();
    }

    if (ncnn::get_gpu_count() < 1) {
        if (g_refcount == 0)
            ncnn::destroy_gpu_instance();
        throw std::runtime_error("no Vulkan device available");
    }

    ++g_refcount;
    index_ = ncnn::get_default_gpu_index();
    name_ = ncnn::get_gpu_info(index_).device_name();
#else
    throw std::runtime_error("this build has no ncnn Vulkan backend");
#endif
}

NcnnToolkit::GpuInstance::~GpuInstance()
{
#ifdef HAVE_NCNN
    std::lock_guard<std::mutex> lock(g_mutex);

    // only reached when the constructor succeeded (it throws otherwise)
    if (--g_refcount == 0)
        ncnn::destroy_gpu_instance();
#endif
}
