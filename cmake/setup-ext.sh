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

echo "ok: ext/ncnn (with glslang), ext/rife-ncnn-vulkan (src only)"
