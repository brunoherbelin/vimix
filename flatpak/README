Linux Flatpak package of vimix
================================

## Prerequirement: install flatpak

Instructions are at https://flatpak.org/setup/

e.g. for Ubuntu:

    ~$ sudo apt install flatpak


## Install vimix from Flathub

If you followed all instructions of the [flatpak setup](https://flatpak.org/setup/), vimix should be in the list of packages.


## Build the flatpack package of vimix

If you want to have the latest developper version of vimix (before releases), you can build a vimix flatpak yourself.

This way, the application vimix is still sandboxed (i.e. not installing libs in your system), removable (entirely free space after remove) and updatable (just re-compile to update).

1. Install flatpak build environments

    ~$ sudo apt install flatpak-builder

    ~$ flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

    ~$ flatpak install org.gnome.Sdk/x86_64

Select version **43** in the list of proposed version

    ~$ flatpak install org.gnome.Platform

Select version **43** in the list of proposed version


2. Build vimix flatpak

This settings of git is needed to enable clone of local repos during build:

    ~$ git config --global --add protocol.file.allow always

Get the flatpak manifest for vimix:

    ~$ curl -O https://raw.githubusercontent.com/brunoherbelin/vimix/master/flatpak/io.github.brunoherbelin.Vimix.json

Launch the build of the flatpak:

    ~$ flatpak-builder --user --install --force-clean build io.github.brunoherbelin.Vimix.json

The build will be quite long as some dependencies are also re-build from source. However, the build of dependencies is kept in cache; rebuilding vimix will subsequently be much faster.


3. Run vimix flatpak

If all goes well, the package will have been generated and be able to run. The vimix app icon should be available. To run from command line:

    ~$ flatpak run io.github.brunoherbelin.Vimix


## Uninstall vimix flatpak

~$ flatpak uninstall vimix


# Developper information

The flatpak manifest for flathub is at https://github.com/brunoherbelin/flathub

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
                "path": "/home/bhbn/Development/vimix",
                }
            ]
        }
