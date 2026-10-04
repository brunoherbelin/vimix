# Build vimix

First, install dependencies according to your system (see below).

## Clone

    git clone --recursive https://github.com/brunoherbelin/vimix.git

This will create the directory 'vimix', download the latest version of vimix code,
and (recursively) clone all the internal git dependencies.

## Compile

First time after git clone:

    mkdir vimix-build && cd vimix-build && cmake -DCMAKE_BUILD_TYPE=Release ../vimix && cmake --build . -j$(nproc)  
    
This will create the directory 'vimix-build', configure the program for build, and compile vimix.
If successful, the compilation will end with:
     
     ...
     [100%] Linking CXX executable vimix
     [100%] Built target vimix

You can run vimix from your `vimix-build` directory with this command:

    __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia DISPLAY=:0 ./src/vimix

## Install

To install vimix in your system:

    sudo cmake --install .

Vimix is now installed and can be launched from the list of apps or with this command:

    launch_vimix.sh
    
## Update clone and re-compile 

Run these commands from the `vimix-build` directory if you did 'Clone' and 'Compile' previously and only want to get the latest update and rebuild.
    
    git -C ../vimix/ pull
    cmake --build .
    
This will pull the latest commit from git and recompile. 

## Try the Beta branch

Run this commands from the `vimix-build` directory before runing 'Update clone and re-compile above'

    git -C ../vimix/ checkout beta
    
It should say;

    branch 'beta' set up to track 'origin/beta'.
    Switched to a new branch 'beta'

## Dependencies

**Compiling tools:**

- gcc
- make
- cmake
- git

**Libraries:**

- gstreamer
- gst-plugins (libav, base, good, extra, bad & pipewire)
- libglfw3
- libicu (icu-i18n icu-uc icu-io)
- libpng

Optionnal:

- glm
- stb
- TinyXML2
- AbletonLink
- Shmdata
- Frei0r

### Install Dependencies

#### Ubuntu

Minimum:

    apt-get update && apt-get install -y build-essential cmake git libpng-dev libicu-dev libglfw3-dev libgtk-3-dev libavahi-client-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good

Recommended:

    apt-get install -y libavahi-client3 gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-plugins-extra gstreamer1.0-pipewire frei0r-plugins 

Optionnal (will be compiled if not installed):

    apt-get install -y libglm-dev libstb-dev libtinyxml2-dev ableton-link-dev 
    

Extra support for [Shmdata](https://github.com/nicobou/shmdata/blob/develop/doc/install-from-sources.md) to interface with [Splash](https://splashmapper.xyz/fr/)
  
    git clone https://gitlab.com/sat-metalab/shmdata.git
    mkdir shmdata-build
    cd shmdata-build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=Release -DWITH_PYTHON=0 -DWITH_SDCRASH=0 -DWITH_SDFLOW=0 ../shmdata-build
    cmake --build . --target package
    sudo dpkg -i ./libshmdata_1.3*_amd64.deb

#### OSX with [Homebrew](https://brew.sh/)

Compiler and git are provided by the Xcode command line tools:

    xcode-select --install

Minimum:

    brew install cmake pkgconf libpng glfw gstreamer icu4c

Rrecommended:

    brew install frei0r onnxruntime

Optionnal (will be compiled if not installed):

    brew install glm tinyxml2 


