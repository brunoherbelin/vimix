#!/bin/bash

# Test if running under wayland
if [ -z "$WAYLAND_DISPLAY" ]; then
    # not Wayland, nothing special
    vimix "$@"
else
    # Wayland: test if there is an nvidia GPU
    gpu=$(lspci | grep -i '.* vga .* nvidia .*')
    shopt -s nocasematch
    if [[ $gpu == *' nvidia '* ]]; then
        # with nvidia, request Wayland render offload
        printf 'Nvidia GPU present:  %s\n' "$gpu"
        __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia vimix "$@"
    else
        # otherwise, nothing special
        vimix "$@"
    fi
fi
