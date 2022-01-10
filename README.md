# AMCDuke32 - Fork of EDuke32 for The AMC Squad

* [About](#About)
* [The AMC Squad](#the-amc-squad)
* [Installation](#installation)
   * [Building from Source](#building-from-source)
* [Feature Differences](#feature-differences)
   * [CON Commands](#con-commands)
       * [definesoundv](#definesoundv)
       * [ifcfgvar](#ifcfgvar)
       * [ifpdistlvar and ifpdistgvar](#ifpdistlvar-and-ifpdistgvar)
       * [mkdir](#mkdir)
       * [profilenanostart](#profilenanostart)
       * [profilenanoend](#profilenanoend)
       * [profilenanolog](#profilenanolog)
       * [profilenanoreset](#profilenanoreset)
       * [setmusicvolume](#setmusicvolume)
   * [DEF Commands](#def-commands)
       * [keyconfig](#keyconfig)
       * [customsettings](#customsettings)
   * [Game Events](#game-events)
       * [EVENT_CSACTIVATELINK](#EVENT_CSACTIVATELINK)
       * [EVENT_CSPOSTMODIFYOPTION](#EVENT_CSPOSTMODIFYOPTION)
       * [EVENT_CSPREMODIFYOPTION](#EVENT_CSPREMODIFYOPTION)
       * [EVENT_CSPOPULATEMENU](#EVENT_CSPOPULATEMENU)
       * [EVENT_PREACTORDAMAGE](#EVENT_PREACTORDAMAGE)
       * [EVENT_POSTACTORDAMAGE](#EVENT_POSTACTORDAMAGE)
   * [Struct Members](#struct-members)
       * [userquote_xoffset](#struct-members)
       * [userquote_yoffset](#struct-members)
       * [voicetoggle](#struct-members)
       * [csarray](#struct-members)
       * [m_customsettings](#struct-members)
   * [Misc Changes](#general-changes)
       * [cl_keybindmode](#misc-changes)
       * [PROJECTILE_RADIUS_PICNUM_EX](#misc-changes)
* [Credits and Licenses](#credits-and-licenses)

## About

AMCDuke32 is a fork of the eduke32 engine, created to serve as a base for the AMC TC.
eduke32 is based on the Duke Nukem 3D source, which was created on top of the Build engine
by Ken Silverman, released under the Build license.

Its primary purpose is to raise engine limits and alter hardcoded game behaviour, to allow
for greater flexibility in the development of the game. It also adds a small set of CON script
commands that enable modification of features not available in base eduke32. The changes are
generally kept sparse to make merging with mainline eduke32 as smooth as possible.

Note that The AMC Squad makes extensive use of eduke32 modding features, and hence will not be
made compatible with BuildGDX or Raze.

The original eduke32 source port was created by Richard "TerminX" Gobeille, and can be found at: https://voidpoint.io/terminx/eduke32

## The AMC Squad

"The AMC Squad" is a free standalone FPS built on the Duke 3D engine, and is loosely based on the world established by
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
contains the AMC Squad data, i.e. the folder where the `amctc.grpinfo` file is located.

__Additional build instructions can be found here:__

* Windows: <https://wiki.eduke32.com/wiki/Building_EDuke32_on_Windows>
* Linux: <https://wiki.eduke32.com/wiki/Building_EDuke32_on_Linux>
* MacOS: <https://wiki.eduke32.com/wiki/Building_EDuke32_on_macOS>

## Feature Differences

In this section we list the major changes to the modding API, including new CON and DEF script commands, and how to use them.

### __CON Commands__

Commands that extend the functionality of the CON VM.

----

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

----

#### __ifcfgvar__

Usage: `ifcfgvar <var>`

Can only be used with variables. Will check if the CFG contains the given variable, and if yes, follows the `true` path, `false` otherwise.

Its main use is to allow loading variables stored inside the CFG that have a nonzero default value. The problem is that `loadgamevar` will overwrite the value of the variable with 0 if the variable is not present. By using this conditional, we can prevent this.

----

#### __ifpdistlvar and ifpdistgvar__

Usage: 
* `ifpdistlvar <var>`
* `ifpdistgvar <var>`

Branches if the distance from the actor to the player is less/greater than the value in the provided variable.

Variant of `ifpdistl` and `ifpdistg` that takes variables as arguments instead of a constant.

Required to allow customizing the particle effect distance at runtime, rather than at compile-time.

----

#### __mkdir__

Usage: `mkdir <quote_label>`

Creates a directory in the current moddir/profile directory/current working directory, with the given quote as path.

Does not throw an error if directory already exists.

----

#### __profilenanostart__

Usage: `profilenanostart <idx>`

This command is used for high-precision profiling of CON code. The maximum index is 31.

Running this command records an internal nanosecond timestamp at index `<idx>`, where `<idx>` is a label or a constant. This acts as the startpoint for timing the execution time of a sequence of CON commands.

----

#### __profilenanoend__

Usage: `profilenanoend <idx>`

This command is used for high-precision profiling of CON code. The maximum index is 31.

Records the endpoint nanosecond timestamp, and automatically computes the elapsed time between startpoint and endpoint for the given index. This assumes that `profilenanostart` was executed beforehand.

Timings that are recorded internally is the elapsed time in nanoseconds between start and stop, the cumulative sum of elapsed times, the square sum, as well as the number of timings that were sampled.

To print the results of the timing, use `profilenanolog`.

----

#### __profilenanolog__

Usage: `profilenanolog <idx>`

This command is used for high-precision profiling of CON code. The maximum index is 31.

Prints the current timing stats for index `<idx>`. This includes:
* The most recent elapsed time sample recorded.
* The number of measurements `N` in total since the last reset.
* The mean timing over all `N` measurements, in nanoseconds.
* The standard deviation over all `N` measurements, in nanoseconds.

----

#### __profilenanoreset__

Usage: `profilenanoreset <idx>`

This command is used for high-precision profiling of CON code. The maximum index is 31.

This command clears the recorded measurements, and resets all internal timestamps back to 0.

----

#### __setmusicvolume__

Usage: `setmusicvolume <percent>`

Allows the CON script to lower the music volume independently of the player's settings. This does not affect the player's settings.

This command is intended to allow scripts to temporarily lower the music volume, while preserving the user's chosen volume defined in the sound settings menu.

* `<percent>`: Value from 0 to 100, acting as a percentage of the player's current music volume.

----

### __DEF Commands__

Commands that extend the DEF script functionality.

----

#### __keyconfig__

Allows reordering of gamefunc menu entries.

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

List the [gamefunc names](https://wiki.eduke32.com/wiki/Getgamefuncbind) in the order in which you want them to appear inside the keyboard and mouse config menus, separated by newlines or whitespaces. Any omitted gamefuncs will not be listed in the menu.

This command is compatible with the CON commands [definegamefuncname](https://wiki.eduke32.com/wiki/Definegamefuncname) and [undefinegamefunc](https://wiki.eduke32.com/wiki/Undefinegamefunc).

----

#### __customsettings__

Can be used to define the structure of a custom settings menu. Has a maximum of 64 entries.

Usage:
```
customsettings
{
    title "Custom Settings"
    entry
    {
        name "Settings Entry 1"
        index 0
        font "small"
        type "on/off"
    }
    entry
    {
        name "Settings Entry 2"
        index 1
        font "big"
        type "yes/no"
    }
    entry
    {
        name "Settings Entry 3"
        index 2
        font "mini"
        type "multi"
        vstrings { "hello" "world" "foo" "bar" }
        values { 0 10 20 30 }
    }
    ...
}
```
For the token `customsettings`, the following subtokens are supported:
* `title`: Defines the text shown on the menu entry that leads to the custom settings menu. This is always placed inside the "Options" menu.
* `entry`: Defines an entry for the custom settings menu, see below.
    * The ordering of the menu is defined by the order in which these entries appear in the DEF file.
    * To make the system more robust, the order is independent of the index of the entry, see the `index` token.
* `link`: NOT IMPLEMENTED YET. Defines a link to a subpage of menu options.

For the token `entry`, the following subtokens are supported:
* `name`: Defines the text of the options entry.
* `index`: This token defines a index to uniquely identify the custom settings entry. This index separates the entry from the order in the DEF script, allowing the menu to be reordered without needing to alter the CON code. Minimum index is 0, maximum index is 63.
* `font`: Defines which type of font to use for the custom settings entry. Possible values are:
    * `big` | `bigfont` | `redfont`: Uses the large menu font, as seen on the title screen.
    * `small` | `smallfont` | `bluefont`: Uses the small menu font, as used in the Polymost settings.
    * `mini` | `minifont`: Uses the smallest menu font, as used in the keybind menu.
    * Other values are ignored. Default is `small`.
* `type`: Defines the behavior of the menu entry. Multiple options are available:
    * `button`: Acts as a simple button, which makes a sound and runs `EVENT_CSACTIVATELINK` when activated. 
        * Does not alter `userdef[].cs_array`.
    * `yes/no` | `no/yes`: Displays a boolean "Yes"/"No" toggle menu option. 
        * When activated, changes the value of `userdef[].cs_array` from 0 to 1, and vice-versa.
        * Default is set to "No" unless altered in `EVENT_CSPOPULATEMENU`.
    * `on/off` | `off/on` | `toggle`: Displays a boolean "ON"/"OFF" toggle menu option. 
        * When activated, changes the value of `userdef[].cs_array` from 0 to 1, and vice-versa. 
        * Default is set to "OFF" unless altered in `EVENT_CSPOPULATEMENU`.
    * `multi` | `choices`: Defines a multiple choice menu entry.
        * When interacted with, displays a range of options to select.
        * Using the arrow keys cycles through the options.
        * Requires the tokens `vstrings` and `values` to be defined for the entry, see below.
        * Can define at most 32 choices per entry.
    * `range`, `slider`: NOT IMPLEMENTED YET.
        * In the future, these tokens will define a slider to adjust an integer value gradually, either by keyboard or by using the mouse.
    * `spacer2` : 2 units of empty space. Cannot be selected. 
    * `spacer4` : 4 units of empty space. Cannot be selected.
    * `spacer6` : 6 units of empty space. Cannot be selected.
    * `spacer8` : 8 units of empty space. Cannot be selected.
* `vstrings {...}`: Only used for multiple choice menu entries. Defines a list of strings to represent the values inside the selection.
    * Each entry must be defined in the order of appearance in the menu.
    * Entries in `values` and `vstrings` must appear in matching order.
* `values {...}`: Only used for multiple choice menu entries.
Defines the list of integers that `userdef[].cs_array` will be set to when an option is selected.
    * Each entry must be defined in the order of appearance in the menu.
    * Entries in `values` and `vstrings` must appear in matching order.

The logic of these entries is furthermore controlled through the events:
* `EVENT_CSACTIVATELINK` : Triggered when a link entry is activated.
* `EVENT_CSPREMODIFYOPTION` : Triggered on activation of an option entry, before the value is modified.
* `EVENT_CSPOSTMODIFYOPTION` : Triggered on activation of an option entry, after the value is modified.
* `EVENT_CSPOPULATEMENU` : Triggered when opening the custom settings menu, before entries are set up.

----

### __Game Events__

Game events not present in regular eduke32.

#### __EVENT_CSACTIVATELINK__

Used in conjunction with the DEF command [customsettings](#customsettings).

This event is called each time a link entry or button is activated, i.e. an entry that is not a toggleable option.

Can be used to define behavior for buttons, e.g. a "set to default" button.

----

#### __EVENT_CSPREMODIFYOPTION__

Used in conjunction with the DEF command [customsettings](#customsettings).

This event is called each time a custom settings entry option is modified, before `userdef[].cs_array` is updated. This includes the Yes/No, On/Off and multiple choice entries.

Can be used to perform actions before `userdef[].cs_array` is modified.

----

#### __EVENT_CSPOSTMODIFYOPTION__

Used in conjunction with the DEF command [customsettings](#customsettings).

This event is called each time a custom settings entry option is modified, after `userdef[].cs_array` is updated. This includes the Yes/No, On/Off and multiple choice entries.

Can be used to perform actions after `userdef[].cs_array` is modified, e.g. to update gamevars and save them in the CFG.

----

#### __EVENT_CSPOPULATEMENU__

Used in conjunction with the DEF command [customsettings](#customsettings).

This event is called each time the custom settings menu is opened. Altering the values in `userdef[].cs_array` in this event will directly affect the chosen values shown inside the menu.

For instance, this can be used to change the default values of the menu, and/or to load values from the CFG, and update the `cs_array` entries with those values.

----

#### __EVENT_PREACTORDAMAGE__

This event is called when an actor runs `A_IncurDamage()`, just before decreasing `sprite[].extra` and changing the owner.

i.e. it occurs before the actor has taken damage and its health was updated.

* `sprite[].htextra` stores the damage to be applied, `sprite[].htowner` stores the source of the damage.
* `sprite[].extra` stores the current health, `sprite[].owner` stores the previous source of damage.
* Set RETURN to != 0 to cancel damage.
* Currently, this event is not executed when the player takes damage.

----

#### __EVENT_POSTACTORDAMAGE__

This event is called when an actor runs A_IncurDamage(), just after decreasing sprite[].extra, resetting htextra to -1 and updating htowner.

i.e. it occurs after the actor has taken damage and its health was updated.

* Currently, this event is not executed when the player takes damage.

----

### __Struct Members__

New struct members added by the fork.

* `userdef[].userquote_xoffset` and `userdef[].userquote_yoffset`: Alters the x and y position of the `userquote` text. Can be positive and negative.
* `userdef[].voicetoggle`: Read-only userdefs struct member, which acts as a bitfield.
    * __1__: If set, character voices are enabled (Duke-Talk).
    * __2__: Dummy value, reserved.
    * __4__: Character voices from other players are enabled.
* `userdef[].csarray` : This is an array for the custom settings menu, stores the current state of each entry in the menu by index.
    * Update the values of this struct inside event `EVENT_CSPOPULATEMENU`, and the entries will reflect the contents of the array.
    * In order to apply settings, retrieve the value from this array for the corresponding list entry in `EVENT_CSPOSTMODIFYOPTION`, and update your gamevars with it.
    * To store settings between game launches, use `savegamevar` and `loadgamevar` on your actual option variables.
* `userdef[].m_customsettings` : Index for the currently selected custom settings entry. 
    * IMPORTANT: Uses the index defined inside the DEF, not the ordering of the items!
    * The index will be set to -1 if the index was not defined within the DEF.

### __Misc Changes__

* `MAXTILES` increased from 30720 to 32512.
* `MAXGAMEVARS` increased from 2048 to 4096 (also doubles `MAXGAMEARRAYS`).
  * Doubled from 1024 to 2048 in the editor.
* Mapster32: The key combination `[' + Y]` enables a pink tile selection background, to improve visibility of tiles in the selector.
* Looping ambient sounds are now able to overlap with instances of themselves.
* Add cvar `cl_keybindmode` to change keyboard config behaviour.
  * If 0, will allow multiple gamefuncs to be assigned to the same key in the keyboard config menu (original behavior).
  * If 1, will clear all existing gamefuncs for that key when binding a key to a gamefunc. This prevents accidentally assigning multiple gamefuncs to the same key, e.g. when default values exist.
* Added a lower bound for distance between menu items. This allows menus to scroll properly now.
* Add `PROJECTILE_WORKSLIKE` flag `PROJECTILE_RADIUS_PICNUM_EX`:
    * Flag value is 1073741824 (0x40000000).
    * The flag changes the htpicnum of the projectile to the actual picnum, instead of using RADIUSEXPlOSION. However, it also preserves hardcoded behaviors specific to RADIUSEXPLOSION, including destroying spritewalls and spritefloors that have a hitag.
    * RPG projectiles can only burn trees, tires and boxes if this flag is set. Player will receive the same pushback as with RADIUSEXPLOSION when hit.
* Hardcoded anti-cheat measure removed from skill 4.
* Change crosshair size slider to range from 10 to 100.
* Make save settings menu and reset progress options available.
* Various bugfixes and changes that affected AMC specifically.

## Credits and Licenses

The AMCDuke32 fork was created and is being maintained by Dino Bollinger.

* eduke32 was created by Richard "TerminX" Gobeille, and is maintained the eduke32 contributors. It is licensed under the GPL v2.0, see `gpl-2.0.txt`.
  * It can be found at: https://voidpoint.io/terminx/eduke32
* The Build Engine was created by Ken Silverman and is licensed under the BUILD license. See `source/build/buildlic.txt`.
* The AMC Squad was created by James Stanfield and the AMC team.
  * The game can be found at: https://www.moddb.com/games/the-amc-tc

The maintainers of the game and the engine fork thank the developers of eduke32 for their continued assistance and support over the years.

**THIS SOFTWARE IS PROVIDED ''AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.**
