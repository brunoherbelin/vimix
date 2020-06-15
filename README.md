# vimix
Live Video Mixer

*/!\ Work in progress*

v-mix is the successor for GLMixer - https://sourceforge.net/projects/glmixer/


## Clone

    git clone --recursive https://github.com/brunoherbelin/vimix.git

## Compile

```
cmake -G Ninja
ninja
```

### Dependencies

**Compiling tools:**

- gcc
- cmake
- Ninja

**Libraries:**

- gstreamer
- libpng
- libglfw3


#### Ubuntu

**tools:**

    apt-get install build-essential cmake ninja-build

**libs:**

    apt-get install libpng-dev libglfw3-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
