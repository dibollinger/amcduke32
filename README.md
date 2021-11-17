# AMCDuke32 - Fork of EDuke32 for the AMC TC

* [About](#About)
* [The AMC TC](#the-amc-tc)
* [Installation](#installation)
   * [Building from Source](#building-from-source)
* [Feature Differences](#feature-differences)
   * [CON Commands](#con-commands)
       * [definesoundv](#definesoundv)
       * [setmusicvolume](#setmusicvolume)
       * [mkdir](#mkdir)
   * [DEF Commands](#def-commands)
       * [keyconfig](#keyconfig)
   * [Struct Members](#struct-members)
       * [userquote_xoffset](#struct-members)
       * [userquote_yoffset](#struct-members)
       * [voicetoggle](#struct-members)
   * [General Changes](#general-changes)
       * [cl_keybindmode](#general-changes)
* [Credits and Licenses](#credits-and-licenses)

## About

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

## Installation

Windows builds for the latest engine revisions can be found in the releases page, along with old versions.
The latest engine revision will also be packaged together with the download itself.

For Linux and MacOS, we recommend compiling the binary from source, using the instructions given below.

### Building from Source

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

## Feature Differences

In this section we list the major changes to the modding API, including new CON and DEF script commands, and how to use them.

### CON Commands

Commands that extend the functionality of the CON VM.

#### __definesoundv__

Usage: `definesoundv <soundID> <filename> <pitch_lower> <pitch_upper> <priority> <flags> <distance> <volume>`

Variant of the [definesound](https://wiki.eduke32.com/wiki/Definesound) command, with an added parameter to change the sound's actual volume.

Defines a sound and assigns various properties to it. The maximum number of sounds that can be defined is 16384.

* `<value>`: This can be either the sound's number or the name that has been defined for that number.
* `<filename>`: The name of the sound file. Sound files are assumed to be in the same directory as the program unless a folder path is specified.
* `<pitch_lower>` and `<pitch_upper>`: Range in which the sound pitch can vary. Values may be positive or negative.
* `<priority>`: A value of 0 to 255 indicates the priority the sound has over other sounds that are playing simultaneously.
* `<flags>`: Bitfield which indicates what type of sound you are defining.
    * __1__:  The sound repeats if continually played.
    * __2__:  The sound is an ambient effect.
        * __3__: The sound will loop until instructed to stop.
    * __4__:  The sound is a player voice.  Disabling "Duke Talk" in the menu will mute this sound.
    * __8__:  The sound contains offensive content.  Disabling "Adult Mode" or enabling the parental lock in the menu will mute this sound.
    * __16__: The sound will always be heard from anywhere in the level.
    * __32__: If set, only one instance of the sound is allowed to play at a time. (also for `definesound`)
    * __128__: The sound is used in Duke-Tag.
* `<distance>`: Negative values increase the distance at which the sound is heard; positive ones reduce it.  Can range from -32768 to 32767.
* `<volume>`: Ranges from 0 to 16384, with 1024 being the default volume. Changes the actual volume of the sound being played.

#### __setmusicvolume__

Usage: `setmusicvolume <percent>`

Allows the CON script to lower the music volume independently of the player's settings. This does not affect the player's settings.

This command is intended to allow scripts to temporarily lower the music volume, while preserving the user's chosen volume defined in the sound settings menu.

* `<percent>`: Value from 0 to 100, acting as a percentage of the player's current music volume.

#### __mkdir__

Usage: `mkdir <quote_label>`

Creates a directory in the current moddir/profile directory/current working directory, with the given quote as path.

Does not throw an error if directory already exists.

### DEF Commands

Commands that extend the DEF script functionality.

#### __keyconfig__

Usage:
```
keyconfig
{
    gamefunc_Move_Forward
    gamefunc_Move_Backward
    gamefunc_Strafe_Left
    gamefunc_Strafe_Right
    ...
}
```

Allows reordering of gamefunc menu entries. List the [gamefunc names](https://wiki.eduke32.com/wiki/Getgamefuncbind) in the order in which you want them to appear inside the keyboard and mouse config menus, separated by newlines or whitespaces. Any omitted gamefuncs will not be listed in the menu.

This command is compatible with the CON commands [definegamefuncname](https://wiki.eduke32.com/wiki/Definegamefuncname) and [undefinegamefunc](https://wiki.eduke32.com/wiki/Undefinegamefunc).

### Struct Members

New struct members added by the fork.

* `userdef[].userquote_xoffset` and `userdef[].userquote_yoffset`: Alters the x and y position of the `userquote` text. Can be positive and negative.
* `userdef[].voicetoggle`: Read-only userdefs struct member, which acts as a bitfield.
    * __1__: If set, character voices are enabled (Duke-Talk).
    * __2__: Dummy value, reserved.
    * __4__: Character voices from other players are enabled.

### General Changes

* `MAXTILES` increased from 30720 to 32512.
* `MAXGAMEVARS` increased from 2048 to 4096 (also doubles `MAXGAMEARRAYS`).
  * Doubled from 1024 to 2048 in the editor.
* Mapster32: The key combination `[' + Y]` enables a pink tile selection background, to improve visibility of tiles in the selector.
* Looping ambient sounds are now able to overlap with instances of themselves.
* Add cvar `cl_keybindmode` to change keyboard config behaviour.
  * If 0, will allow multiple gamefuncs to be assigned to the same key in the keyboard config menu (original behavior).
  * If 1, will clear all existing gamefuncs for that key when binding a key to a gamefunc. This prevents accidentally assigning multiple gamefuncs to the same key, e.g. when default values exist.


## Credits and Licenses

The AMCDuke32 fork was created and is being maintained by Dino Bollinger.

* eduke32 was created by Richard "TerminX" Gobeille, and is maintained the eduke32 contributors. It is licensed under the GPL v2.0, see `gpl-2.0.txt`.
  * It can be found at: https://voidpoint.io/terminx/eduke32
* The Build Engine was created by Ken Silverman and is licensed under the BUILD license. See `source/build/buildlic.txt`.
* The AMC TC was created by James Stanfield and the AMC team.
  * The game can be found at: https://www.moddb.com/games/the-amc-tc

The AMC Team thanks the developers of eduke32 for their continued assistance and support over the years.

**THIS SOFTWARE IS PROVIDED ''AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.**
