#!/bin/bash
which vimix >/dev/null || { echo "vimix not found"; exit 1; }
# test if there is an nvidia GPU
gpu=$(lspci | grep -i '.* vga .* nvidia .*')
shopt -s nocasematch
if [[ $gpu == *' nvidia '* ]]; then
    # with nvidia, request Wayland render offload
    printf 'NVidia GPU present:  %s\n' "$gpu"
    # Test if running under wayland
    if [ -z "$WAYLAND_DISPLAY" ]; then
        # not Wayland
        printf 'Launching vimix with NVIDIA under X11\n' 
        __GLX_VENDOR_LIBRARY_NAME=nvidia vimix "$@"
    else
        # Wayland: 
        printf 'Launching vimix with NVIDIA offload support under Wayland\n'
        __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia DISPLAY=:0 vimix "$@"
    fi
else
    # otherwise, nothing special
    printf 'Launching vimix\n' 
    vimix "$@"
fi

