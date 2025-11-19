Linux Flatpak package of vimix
================================

## Prerequirement: install flatpak

Instructions are at https://flatpak.org/setup/

e.g. for Ubuntu:

    sudo apt install flatpak


## Install vimix releases from Flathub

If you followed all instructions of the [flatpak setup](https://flatpak.org/setup/), vimix should be in the list of packages.

    flatpak install --user vimix


## Build local beta flatpack package of vimix

If you want to have the latest developper version of vimix (before releases), you can build a vimix flatpak yourself.

This way, the application vimix is still sandboxed (i.e. not installing libs in your system), removable (entirely free space after remove) and updatable (just re-compile to update).

### 1. Install flatpak build environments

If not already installed, install the builder and the flathub repository:

    sudo apt install flatpak-builder    
    flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

Install the runtime environments:
    
    flatpak install org.freedesktop.Sdk/x86_64/25.08
    flatpak install org.freedesktop.Platform/x86_64/25.08


### 2. Build vimix flatpak

These settings of git are needed to enable clone of local repos during build (done only once):

    git config --global --add protocol.file.allow always

Launch the build of the flatpak:

    flatpak-builder --user --install --from-git=https://github.com/brunoherbelin/vimix.git --from-git-branch=beta --delete-build-dirs --force-clean build flatpak/io.github.brunoherbelin.Vimix.json

The build will be quite long as some dependencies are also re-build from source. However, the build of dependencies is kept in cache; rebuilding vimix will subsequently be much faster.


## Run vimix flatpak

If all goes well, the package will have been generated and be able to run. The vimix app icon should be available. 

To run from command line:

    flatpak run io.github.brunoherbelin.Vimix


## Uninstall vimix flatpak

    flatpak uninstall vimix


# Developper information

The flatpak manifest for flathub is at https://github.com/flathub/io.github.brunoherbelin.Vimix

To build the vimix flatpak with code from local folder (debugging), change the following:

        {
            "name": "vimix",
            "buildsystem": "cmake",
            "config-opts": [
                "-DCMAKE_BUILD_TYPE=Release"
            ],
            "sources": [
                {
                "type":"dir",
                "path": "[your_development_dir]/vimix",
                }
            ]
        }
