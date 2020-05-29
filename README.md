# AMCDuke32 - Fork of EDuke32 for the AMC TC

AMCDuke32 is a fork of the eduke32 engine, created to serve as a base for the AMC TC.
eduke32 is based on the Duke Nukem 3D source, which was created on top of the Build engine
by Ken Silverman, released under the Build license.

Its primary purpose is to raise engine limits and alter hardcoded game behaviour, to allow
for greater flexibility in the development of the game. It also adds a small set of CON script
commands that enable modification of features not available in base eduke32. The changes are 
generally kept sparse to make merging with mainline eduke32 as smooth as possible.

Note that the AMC TC makes extensive use of eduke32 modding features, and hence will not be
made compatible with BuildGDX or Raze.

The original eduke32 source port was created by Richard "TerminX" Gobeille, and can be found at: https://voidpoint.io/terminx/eduke32

## The AMC TC

The AMC TC is a free standalone FPS built on the Duke 3D engine, and is loosely based on the world established by
Duke Nukem 3D, Blood, Shadow Warrior, and other FPS classics. It currently features three episodes, with a story
that involves a multitude of characters, the so-called AMC Squad, who attempt to defend Earth against a large
variety of threats, including extra-terrestrial and supernatural foes.

The gameplay is mixture of classic FPS gameplay with a variety of modern features. The levels are arranged in a
mission-based structure, which takes the player across a large variety of locations and themes.

The game is free and standalone, and can be downloaded at: https://www.moddb.com/games/the-amc-tc

## Installation & Compiling from Source

Windows builds for the latest engine revisions can be found in the releases page, along with old versions.
The latest engine revision will also be packaged together with the download itself.

For Linux and MacOS, we recommend compiling the binary from source, using the instructions given below:

__Required packages:__
    
     * Basic dev environment (GCC >= 4.8, GNU make, etc)
     * SDL2 >= 2.0 (SDL >= 1.2.10 also supported with SDL_TARGET=1)
     * SDL2_mixer >= 2.0 (SDL_mixer >= 1.2.7 also supported with SDL_TARGET=1)
     * NASM (highly recommended for i686/32-bit compilation to speed up 8-bit classic software renderer)
     * libGL and libGLU (required for OpenGL renderers)
     * libgtk+ >= 2.8.0 (required for the startup window)
     * libvorbis >= 1.1.2 (libvorbisfile, libogg)
        libvorbisfile
        libogg
     * libFLAC >= 1.2.1
     * libvpx >= 0.9.0

__On Debian / Ubuntu__

    sudo apt-get install build-essential nasm libgl1-mesa-dev libglu1-mesa-dev libsdl1.2-dev libsdl-mixer1.2-dev libsdl2-dev libsdl2-mixer-dev flac libflac-dev libvorbis-dev libvpx-dev libgtk2.0-dev freepats

__On Fedora 22-25__

    sudo dnf groupinstall "Development Tools"
    sudo dnf install g++ nasm mesa-libGL-devel mesa-libGLU-devel SDL2-devel SDL2_mixer-devel alsa-lib-devel libvorbis-devel libvpx-devel gtk2-devel flac flac-devel

Freepats is not packaged in Fedora, you must download and install it by yourself if desired. 
See also the "timidity-patch-freepats" package on others RPM based distros.

__Compiling__

To compile, simply run the command `make` in the base folder (with parameter `-j#` to speed up compilation 
with multiple threads). If successful, this should produce the following binaries in the base folder:
* `amctc`
* `mapster32`

The binaries do not support game autodetection. Instead, you should copy the binaries into the folder that 
contains the AMC TC data, i.e. the folder where the `amctc.grpinfo` file is located.

__Additional build instructions can be found here:__

* Windows: <https://wiki.eduke32.com/wiki/Building_EDuke32_on_Windows>
* Linux: <https://wiki.eduke32.com/wiki/Building_EDuke32_on_Linux>
* MacOS: <https://wiki.eduke32.com/wiki/Building_EDuke32_on_macOS>

## Credits and Licenses

The AMCDuke32 fork was created and is being maintained by Dino Bollinger.

* eduke32 is developed and maintained by Voidpoint LLC and is licensed under the GPL v2.0, see `gpl-2.0.txt`.  

* The Build Engine was created by Ken Silverman and is licensed under the BUILD license. See `source/build/buildlic.txt`. 

The AMC Team thanks the developers of eduke32 for their continued assistance and support over the years.

**THIS SOFTWARE IS PROVIDED ''AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.**
