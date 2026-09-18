#!/bin/sh
# Fetch some external sources into ext/. 
# Ran automatically by CMake at configure time when ext/ is missing; 
# safe to run manually and re-run (idempotent).
#
# ncnn is cloned as an INDEPENDENT checkout 
# Partial clones (--filter=blob:none) never download the excluded content.
set -e
cd "$(dirname "$0")/.."

NCNN_URL=https://github.com/Tencent/ncnn
NCNN_COMMIT=b4ba207c18d3103d6df890c0e3a97b469b196b26   # the commit rife 20221029 pins
RIFE_URL=https://github.com/nihui/rife-ncnn-vulkan
RIFE_COMMIT=a7532fc3f9f8f008cd6eecd6f2ffe2a9698e0cf7   # release 20221029
ESRGAN_URL=https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan
ESRGAN_COMMIT=37026f49824c5cf84062e7c6a5dd71445dcf610f   # master 2022-04

# Clone ncnn (independent)
if [ ! -e ext/ncnn/CMakeLists.txt ]; then
    git clone --filter=blob:none --no-checkout "$NCNN_URL" ext/ncnn
    git -C ext/ncnn checkout "$NCNN_COMMIT"
fi
# Clone glslang at the commit ncnn pins, without its Test/ data
if [ ! -e ext/ncnn/glslang/CMakeLists.txt ]; then
    git -C ext/ncnn submodule init glslang
    gurl=$(git -C ext/ncnn config submodule.glslang.url)
    gsha=$(git -C ext/ncnn rev-parse HEAD:glslang)
    git clone --filter=blob:none --no-checkout "$gurl" ext/ncnn/glslang
    git -C ext/ncnn/glslang sparse-checkout set --no-cone '/*' '!/Test/'
    git -C ext/ncnn/glslang checkout "$gsha"
fi

# Clone rife-ncnn-vulkan (only src/; its own ncnn submodule is left uninitialised)
if [ ! -e ext/rife-ncnn-vulkan/src/rife.cpp ]; then
    git clone --filter=blob:none --no-checkout "$RIFE_URL" ext/rife-ncnn-vulkan
    git -C ext/rife-ncnn-vulkan sparse-checkout set src
    git -C ext/rife-ncnn-vulkan checkout "$RIFE_COMMIT"
fi

# Clone Real-ESRGAN-ncnn-vulkan (only src/; its own ncnn submodule is left
# uninitialised -- it is built against ext/ncnn above, which is pinned to the
# same commit RIFE uses). The repo's .gitmodules declares ssh urls
# (git@github.com:); rewrite them to https so no ssh key is ever needed.
if [ ! -e ext/realesrgan-ncnn-vulkan/src/realesrgan.cpp ]; then
    git clone --filter=blob:none --no-checkout "$ESRGAN_URL" ext/realesrgan-ncnn-vulkan
    git -C ext/realesrgan-ncnn-vulkan config url."https://github.com/".insteadOf "git@github.com:"
    git -C ext/realesrgan-ncnn-vulkan sparse-checkout set src
    git -C ext/realesrgan-ncnn-vulkan checkout "$ESRGAN_COMMIT"
fi

echo "ok: ext/ncnn (with glslang), ext/rife-ncnn-vulkan (src only), ext/realesrgan-ncnn-vulkan (src only)"
