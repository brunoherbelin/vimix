# vimix
__Live Video Mixing__

<img src=docs/vimix_screenshot.png width="800">


vimix performs graphical mixing and blending of several movie clips and
computer generated graphics, with image processing effects in real-time.

Its intuitive and hands-on user interface gives direct control on image opacity and
shape for producing live graphics during concerts and VJ-ing sessions.

The output image is typically projected full-screen on an external
monitor or a projector, and can be streamed live (SRT, Shmdata) or recorded (without audio).

vimix is the successor for GLMixer - https://sourceforge.net/projects/glmixer/

# License

GPL-3.0-or-later
See [LICENSE](https://github.com/brunoherbelin/vimix/blob/master/LICENSE)

# Install vimix

Check the [Quick Installation Guide](https://github.com/brunoherbelin/vimix/wiki/Quick-Installation-Guide)

### Linux

Download and install a released [flatpak package](https://flathub.org/apps/details/io.github.brunoherbelin.Vimix)

    flatpak install vimix
    

[![vimix](https://snapcraft.io/vimix/badge.svg)](https://snapcraft.io/vimix)

Download and install a released [snap package](https://snapcraft.io/vimix) 

    snap install vimix
    
Install the stable debian package (slower release frequency)

    sudo apt install vimix

### Mac OSX

#### Download the [latest vimix Release](https://github.com/brunoherbelin/vimix/releases)

# Build vimix

To compile vimix from source, read these [instructions in the documentation](https://github.com/brunoherbelin/vimix/tree/master/docs).

For linux, it is simpler to build a flatpak package of the latest beta version. Detailed instructions are [here](https://github.com/brunoherbelin/vimix/tree/master/flatpak);

    flatpak-builder --user --install --from-git=https://github.com/brunoherbelin/vimix.git --from-git-branch=beta --delete-build-dirs --force-clean build flatpak/io.github.brunoherbelin.Vimix.json

