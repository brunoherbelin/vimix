# vimix
Live Video Mixer

vimix performs graphical mixing and blending of multiple videos, with image processing effects in real-time.
The mix ouput is typically projected full-screen on an external monitor or a projector, and can.

vimix is the successor for GLMixer - https://sourceforge.net/projects/glmixer/


## Clone

'''
$ git clone --recursive https://github.com/brunoherbelin/vimix.git
'''

This will create the directory 'vimix', download the latest version of vimix code,
and (recursively) clone all the other git Dependencies.

If you want to compile a stable version, you could get the latest tagged version.
After the clone, you can list the tags with '$ git tag -l' and then checkout a specific tag:

'''
$ cd vimix
$ git checkout tags/0.2
'''

## Compile

'''
$ mkdir vimix-build
$ cd vimix-build
$ cmake -DCMAKE_BUILD_TYPE=Release ../vimix
$ cmake --build .
'''

### Dependencies

**Compiling tools:**

- gcc
- make
- cmake

**Libraries:**

- gstreamer
- libpng
- libglfw3


#### Ubuntu

**tools:**

    apt-get install build-essential cmake ninja-build

**libs:**

    apt-get install libpng-dev libglfw3-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
