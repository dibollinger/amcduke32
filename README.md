# Fork of eduke32 for the AMC TC v4.0

This repository is a minor fork of https://voidpoint.io/terminx/eduke32 to ensure compatibility of the TC with current versions of the engine.
It supersedes the previous repository at https://github.com/cdoom64hunter/eduke32-amcfork. 

Its primary purpose is to raise the sound limit, alter hardcoded Duke 3D engine features to allow for greater flexibility in development 
of the TC, and to work around incompatibilities that may have been introduced by particular engine changes, such as actor clipping, which
may be difficult to resolve without adversely affecting other games based on the Build engine.

Specific changes can be viewed from the commit logs created by the maintainers of this repository.

## Installing and Compiling from Source

**Required packages:**
* Basic dev environment (GCC >= 4.8, GNU make, etc)
* SDL2 >= 2.0 (SDL >= 1.2.10 also supported with SDL_TARGET=1)
* SDL2_mixer >= 2.0 (SDL_mixer >= 1.2.7 also supported with SDL_TARGET=1)
* NASM (highly recommended for i686/32-bit compilation to speed up 8-bit classic software renderer)
* libGL and libGLU (required for OpenGL renderers)
* libgtk+ >= 2.8.0 (required for the startup window)
* libvorbis >= 1.1.2 (libvorbisfile, libogg)
* libvpx >= 0.9.0

**For further instructions, refer to:**
* Linux: https://wiki.eduke32.com/wiki/Building_EDuke32_on_Linux
* Windows: https://wiki.eduke32.com/wiki/Building_EDuke32_on_Windows
* MacOS: https://wiki.eduke32.com/wiki/Building_EDuke32_on_macOS

To compile, simply execute the following command in the base folder:

```make``` (optionally: ```make -j4``` to speed up compilation)

By default, game autodetection is disabled, and the binary expects the amctc.grpinfo file, as well as the game data to be present in the same folder as itself.
If you intend to experiment with the exe for non-AMC TC purposes, and restore compatibility with other Duke3D-based games, compile with the following parameter:

```make NO_AMCTC=1```

## License

eduke32 is licensed under GPL v2.0. The Build engine base is licensed under the BUILD license. 
Credits and copyright goes to their respective authors. The AMC team thanks the developers of eduke32
for their continued assistance and support over the past years.

Should any significant additions be made to the codebase, then those shall be licensed under the BSD license.

**THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.**
