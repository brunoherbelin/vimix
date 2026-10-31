#!/bin/bash
which vimix >/dev/null || { echo "vimix not found"; exit 1; }
# test if there is an nvidia GPU
gpu=$(lspci | grep -i '.* vga .* nvidia .*')
shopt -s nocasematch
if [[ $gpu == *' nvidia '* ]]; then
    printf 'NVidia GPU present:  %s\n' "$gpu"
    # In flatpak, the NVIDIA driver is provided by a GL extension which must match
    # exactly the version of the host driver; it is installed by 'flatpak update'
    if [ -f /.flatpak-info ] && ! compgen -G '/usr/lib/*/GL/nvidia-*' >/dev/null; then
        printf '\nWARNING: the flatpak NVIDIA driver matching your system driver is not installed.\n' >&2
        printf '         vimix cannot use the NVIDIA GPU; run "flatpak update" to install it.\n\n' >&2
        printf 'Launching vimix without NVIDIA\n'
        vimix "$@"
        exit $?
    fi
    # with nvidia, request render offload (no effect if NVIDIA is the primary GPU)
    if [ -z "$WAYLAND_DISPLAY" ]; then
        printf 'Launching vimix with NVIDIA offload support under X11\n'
    else
        printf 'Launching vimix with NVIDIA offload support under Wayland\n'
    fi
    __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia vimix "$@"
else
    # otherwise, nothing special
    printf 'Launching vimix\n'
    vimix "$@"
fi
