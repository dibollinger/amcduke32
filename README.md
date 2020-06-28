# Fork of EDuke32 for the AMC TC

This repository is a fork of https://voidpoint.io/terminx/eduke32 that makes some changes specifically for the AMC TC. 

It is intended to increase limits and allow flexibility in terms of features and compatibility.

**Link to the game: https://www.moddb.com/games/the-amc-tc**

This repository supersedes the previous at https://github.com/cdoom64hunter/eduke32-amcfork-legacy. 

## Description
The primary purpose of this fork is to raise the sound limit, alter hardcoded Duke 3D engine features to allow for greater flexibility in development 
of the TC, and to work around incompatibilities that may have been introduced by particular engine changes, such as actor clipping, which
may be difficult to resolve without adversely affecting other games based on the Build engine.

This repository is made public so that mappers can make use of recent eduke32 features to build maps using the AMC TC game data. It will be updated semi-regularly to keep up with mainline eduke32.

**Note:** The patch found on this repository is not intended to be a comprehensive bugfix, but rather a maintenance patch to fix critical issues that would otherwise appear when running the TC with newer versions of eduke32. The patch itself is also compatible with the old executable.

## Installation:
You can either compile from source, or download one of the builds from the release page.

**IMPORTANT:** To retain settings and game progress from a previous installation, it is imperative to rename `eduke32.cfg` to `amctc.cfg`. This is due to internal structure changes. This is a one-time change that will be simplified with the release of the next episode.

## Compiling from Source:
When compiling from source, make sure to copy the built executables, as well as the contents of the folder `amctc3.6.5_patch` into your AMC TC installation directory. The script patch is necessary to fix major issues that would otherwise appear with newer revisions of eduke32.

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

By default, game autodetection is disabled, and the binary expects the `amctc.grpinfo` file, as well as the game data to be present in the same folder.

If you intend to experiment with the exe for non-AMC TC purposes, compile with the following parameter:

```make NO_AMCTC=1```

## Credits and Licenses
eduke32 is licensed under GPL v2.0. The Build Engine was created by Ken Silverman and is licensed under the BUILD license. Credits and copyright goes to their respective authors.
 
The AMC Team thanks the developers of eduke32 for their continued assistance and support over the past years.

The CON and DEF code has been written by James Stanfield, with contributions from Cedric Haegeman, Dino Bollinger and Dan Gaskill.
The EBIKE code was written by Lezing Entertainment. The tank code was written by "Fox" from Duke4. Additional code authors have been credited appropriately in the respective script files.

Reuse of the CON and DEF code is permitted for creating your own eduke32 mods, provided that credit is given appropriately, either by mentioning the AMC TC or its authors. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

**THIS SOFTWARE IS PROVIDED ''AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.**
