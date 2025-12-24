# Build vimix

## Clone

    git clone --recursive https://github.com/brunoherbelin/vimix.git

This will create the directory 'vimix', download the latest version of vimix code,
and (recursively) clone all the internal git dependencies.

## Compile

First time after git clone:

    mkdir vimix-build
    cd vimix-build
    cmake -DCMAKE_BUILD_TYPE=Release ../vimix
    cmake --build .
    
This will create the directory 'vimix-build', configure the program for build, and compile vimix.
If successful, the compilation will have produced the executable `vimix` in the `src` directory. 
You can run vimix with `./src/vimix` :
     
     ...
     [100%] Built target vimix
     ./src/vimix
    
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
- gst-plugins (libav, base, good, bad & ugly)
- libglfw3
- libicu (icu-i18n icu-uc icu-io)

Optionnal:

- glm
- stb
- TinyXML2
- AbletonLink
- Shmdata

### Install Dependencies

#### Ubuntu

    apt-get install build-essential cmake libpng-dev libglfw3-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-libav libicu-dev libgtk-3-dev 

Optionnal:

    apt-get install libglm-dev libstb-dev libtinyxml2-dev ableton-link-dev 
    
  > Follow these instructions to [install Shmdata](https://github.com/nicobou/shmdata/blob/develop/doc/install-from-sources.md).
  
    git clone https://gitlab.com/sat-metalab/shmdata.git
    mkdir shmdata-build
    cd shmdata-build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=Release -DWITH_PYTHON=0 -DWITH_SDCRASH=0 -DWITH_SDFLOW=0 ../shmdata-build
    cmake --build . --target package
    sudo dpkg -i ./libshmdata_1.3*_amd64.deb

#### OSX with Brew

    brew install cmake libpng glfw gstreamer icu4c

