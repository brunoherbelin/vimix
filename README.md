# vimix
__Live Video Mixing__

<img src=docs/vimix_screenshot.png width="800">


Vimix performs graphical mixing and blending of several movie clips and
computer generated graphics, with image processing effects in real-time (OpenGL). 
Vimix supports GPU accelerated decoding and encoding.

Its intuitive and hands-on user interface gives direct control on image opacity and
shape for producing live graphics during concerts and VJ-ing sessions.

Video mapping can be configured for projection on all connected monitors.
The output can also be live streamed (SRT, shared memory) or recorded.

vimix is the successor for GLMixer - https://sourceforge.net/projects/glmixer/

# Documentation

See [Vimix community documentation](https://brunoherbelin.github.io/vimix-docs/)

# License

GPL-3.0-or-later
See [LICENSE](https://github.com/brunoherbelin/vimix/blob/master/LICENSE)

# Install vimix

Check the [Quick Installation Guide](https://github.com/brunoherbelin/vimix/wiki/Quick-Installation-Guide)

### Linux

Install a released [flatpak package](https://flathub.org/apps/details/io.github.brunoherbelin.Vimix)

    flatpak install vimix
    
Install a released [snap package](https://snapcraft.io/vimix) 

[![vimix](https://snapcraft.io/vimix/badge.svg)](https://snapcraft.io/vimix)

    snap install vimix
    
Install the stable debian package (slower release frequency)

    sudo apt install vimix

### Mac OSX

Download and install the [latest vimix Release](https://github.com/brunoherbelin/vimix/releases)

### Windows (experimental)

There is no released Windows version yet, but GitHub Actions builds a portable pre-release of vimix for every change.
It needs 64-bit Windows 10 (version 1903 or later) or Windows 11.

1. Open the [latest successful Windows builds](../../actions/workflows/windows-build.yml?query=is%3Asuccess) and click the most recent run.
2. Under *Artifacts* at the bottom of the page, download `vimix-windows-x86_64` (about 110 MB; you need to be signed in to GitHub).
3. Extract the whole zip to a folder (right-click, *Extract All*); vimix does not run from inside the zip.
4. Run `bin\vimix.exe`. The pre-release is not signed, so Windows may show "Windows protected your PC": click *More info*, then *Run anyway*.

Nothing is installed: to remove vimix, delete the folder (and its settings in `%APPDATA%\vimix`).

# Build vimix

To compile vimix from source, read these [instructions in the documentation](https://github.com/brunoherbelin/vimix/tree/master/docs).
On Windows, a single script downloads a private toolchain into the clone and builds a portable vimix (see [docs/README.md](docs/README.md#windows-experimental)).

For linux, it is simpler to build a flatpak package of the latest beta version. Detailed instructions are [here](https://github.com/brunoherbelin/vimix/tree/master/flatpak);

    flatpak-builder --user --install --from-git=https://github.com/brunoherbelin/vimix.git --from-git-branch=beta --delete-build-dirs --force-clean build flatpak/io.github.brunoherbelin.Vimix.json

