# vimix
Live Video Mixing

vimix performs graphical mixing and blending of several movie clips and
computer generated graphics, with image processing effects in real-time.

Its intuitive and hands-on user interface gives direct control on image opacity and
shape for producing live graphics during concerts and VJ-ing sessions.

The ouput image is typically projected full-screen on an external
monitor or a projector, but can be recorded live (no audio).

vimix is the successor for GLMixer - https://sourceforge.net/projects/glmixer/

# License

GPL-3.0-or-later
See [LICENSE](https://github.com/brunoherbelin/vimix/blob/master/LICENSE)

# Install

Check the [Quick Installation Guide](https://github.com/brunoherbelin/vimix/wiki/Quick-Installation-Guide)

### Linux

Download and install a release package from https://snapcraft.io/vimix

    $ snap install vimix
    
NB: You'll need to setup the snap permissions.

### Mac OSX

Download and open a release package from https://github.com/brunoherbelin/vimix/releases
NB: You'll need to accept the exception in OSX security preference.

## Clone

    $ git clone --recursive https://github.com/brunoherbelin/vimix.git

This will create the directory 'vimix', download the latest version of vimix code,
and (recursively) clone all the internal git dependencies.

To only update a cloned git copy:

    $ git pull

## Compile

First time after git clone:

    $ mkdir vimix-build
    $ cd vimix-build
    $ cmake -DCMAKE_BUILD_TYPE=Release ../vimix
    
Compile (or re-compile after pull):
    
    $ cmake --build .

### Dependencies

**Compiling tools:**

- gcc
- make
- cmake
- git

**Libraries:**

- gstreamer
- gst-plugins : libav, base, good, bad & ugly
- libglfw3
- libicu

#### Install Dependencies

**Ubuntu**

    $ apt-get install build-essential cmake libpng-dev libglfw3-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-libav libicu-dev libgtk-3-dev 

**OSX with Brew**

    $ brew install cmake libpng glfw gstreamer gst-libav gst-plugins-bad gst-plugins-base gst-plugins-good gst-plugins-ugly icu4c


#### Generate snap

To generate the snap (from vimix directory):

    $ snapcraft
    
To install the locally created snap:
    
    $ snap install --dangerous vimix_0.5_amd64.snap
    
### Memcheck

To generate memory usage plots in [massif format](https://valgrind.org/docs/manual/ms-manual.html):

    $ G_SLICE=always-malloc valgrind --tool=massif ./vimix
    
To check for memory leaks:
    
    $ G_SLICE=always-malloc valgrind --leak-check=full --log-file=vimix_mem.txt ./vimix
