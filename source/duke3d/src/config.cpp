//-------------------------------------------------------------------------
/*
Copyright (C) 2016 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

#include "duke3d.h"
#include "scriplib.h"
#include "osdcmds.h"
#include "renderlayer.h"
#include "cmdline.h"

#include "vfs.h"

#ifdef __ANDROID__
# include "android.h"
#endif

#if defined RENDERTYPESDL && defined SDL_TARGET && SDL_TARGET > 1
# include "sdl_inc.h"
#endif

// we load this in to get default button and key assignments
// as well as setting up function mappings

#define __SETUP__   // JBF 20031211
#include "_functio.h"

hashtable_t h_gamefuncs    = { NUMGAMEFUNCTIONS<<1, NULL };

int32_t CONFIG_FunctionNameToNum(const char *func)
{
    if (!func || func[0] == '\0')
        return -1;

    return hash_findcase(&h_gamefuncs, func);
}


static char const * CONFIG_FunctionNumToName(int32_t func)
{
    if ((unsigned)func >= (unsigned)NUMGAMEFUNCTIONS)
        return "";
    return gamefunctions[func];
}


int32_t CONFIG_AnalogNameToNum(const char *func)
{
    if (!func)
        return -1;
    else if (!Bstrcasecmp(func, "analog_turning"))
        return analog_turning;
    else if (!Bstrcasecmp(func, "analog_strafing"))
        return analog_strafing;
    else if (!Bstrcasecmp(func, "analog_moving"))
        return analog_moving;
    else if (!Bstrcasecmp(func, "analog_lookingupanddown"))
        return analog_lookingupanddown;
    else
        return -1;
}


static char const * CONFIG_AnalogNumToName(int32_t func)
{
    switch (func)
    {
    case analog_turning:
        return "analog_turning";
    case analog_strafing:
        return "analog_strafing";
    case analog_moving:
        return "analog_moving";
    case analog_lookingupanddown:
        return "analog_lookingupanddown";
    }

    return "";
}


static void CONFIG_SetJoystickButtonFunction(int i, int j, int function)
{
    ud.config.JoystickFunctions[i][j] = function;
    CONTROL_MapButton(function, i, j, controldevice_joystick);
}
static void CONFIG_SetJoystickAnalogAxisSensitivity(int i, float sens)
{
    ud.config.JoystickAnalogueSensitivity[i] = sens;
    CONTROL_SetAnalogAxisSensitivity(i, sens, controldevice_joystick);
}
static void CONFIG_SetJoystickAnalogAxisInvert(int i, int invert)
{
    ud.config.JoystickAnalogueInvert[i] = invert;
    CONTROL_SetAnalogAxisInvert(i, invert);
}
static void CONFIG_SetJoystickAnalogAxisDeadSaturate(int i, int dead, int saturate)
{
    ud.config.JoystickAnalogueDead[i] = dead;
    ud.config.JoystickAnalogueSaturate[i] = saturate;
    JOYSTICK_SetDeadZone(i, dead, saturate);
}
static void CONFIG_SetJoystickDigitalAxisFunction(int i, int j, int function)
{
    ud.config.JoystickDigitalFunctions[i][j] = function;
    CONTROL_MapDigitalAxis(i, function, j);
}
static void CONFIG_SetJoystickAnalogAxisFunction(int i, int function)
{
    ud.config.JoystickAnalogueAxes[i] = function;
    CONTROL_MapAnalogAxis(i, function);
}


void CONFIG_SetDefaultKeys(const char (*keyptr)[MAXGAMEFUNCLEN], bool lazy/*=false*/)
{
    static char const s_gamefunc_[] = "gamefunc_";
    int constexpr strlen_gamefunc_  = ARRAY_SIZE(s_gamefunc_) - 1;

    if (!lazy)
    {
        Bmemset(ud.config.KeyboardKeys, 0xff, sizeof(ud.config.KeyboardKeys));
        CONTROL_ClearAllBinds();
    }

    for (int i=0; i < ARRAY_SSIZE(gamefunctions); ++i)
    {
        if (gamefunctions[i][0] == '\0')
            continue;

        auto &key = ud.config.KeyboardKeys[i];

        int const default0 = KB_StringToScanCode(keyptr[i<<1]);
        int const default1 = KB_StringToScanCode(keyptr[(i<<1)+1]);

        // skip the function if the default key is already used
        // or the function is assigned to another key
        if (lazy && (key[0] != 0xff || (CONTROL_KeyIsBound(default0) && Bstrlen(CONTROL_KeyBinds[default0].cmdstr) > strlen_gamefunc_
                        && CONFIG_FunctionNameToNum(CONTROL_KeyBinds[default0].cmdstr + strlen_gamefunc_) >= 0)))
        {
#if 0 // defined(DEBUGGINGAIDS)
            if (key[0] != 0xff)
                DLOG_F(INFO, "Skipping key '%s' bound to '%s'", keyptr[i<<1], CONTROL_KeyBinds[default0].cmdstr);
#endif
            continue;
        }

        key[0] = default0;
        key[1] = default1;

        if (key[0])
            CONTROL_FreeKeyBind(key[0]);

        if (key[1])
            CONTROL_FreeKeyBind(key[1]);

        if (i == gamefunc_Show_Console)
            OSD_CaptureKey(key[0]);
        else
            CONFIG_MapKey(i, key[0], 0, key[1], 0);
    }
}


void CONFIG_SetDefaults(void)
{
    ud.config.scripthandle = -1;

#ifdef __ANDROID__
    droidinput.forward_sens = 5.f;
    droidinput.gameControlsAlpha = 0.5;
    droidinput.hideStick = 0;
    droidinput.pitch_sens = 5.f;
    droidinput.quickSelectWeapon = 1;
    droidinput.strafe_sens = 5.f;
    droidinput.toggleCrouch = 1;
    droidinput.yaw_sens = 5.f;

    ud.setup.xdim = droidinfo.screen_width;
    ud.setup.ydim = droidinfo.screen_height;
#else
# if defined RENDERTYPESDL && SDL_MAJOR_VERSION >= 2
    uint32_t inited = SDL_WasInit(SDL_INIT_VIDEO);
    if (inited == 0)
        SDL_Init(SDL_INIT_VIDEO);
    else if (!(inited & SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
    {
        ud.setup.xdim = dm.w;
        ud.setup.ydim = dm.h;
    }
    else
# endif
    {
        ud.setup.xdim = 1024;
        ud.setup.ydim = 768;
    }
#endif

#ifdef USE_OPENGL
    ud.setup.bpp = 32;
#else
    ud.setup.bpp = 8;
#endif

#if defined(_WIN32)
    ud.config.MixRate = 44100;
#elif defined __ANDROID__
    ud.config.MixRate = droidinfo.audio_sample_rate;
#else
    ud.config.MixRate = 48000;
#endif

#if defined GEKKO || defined __OPENDINGUX__
    ud.config.NumVoices = 32;
    ud.camera_time = 11;
#else
    ud.config.NumVoices = 64;
    ud.camera_time    = 4;
#endif

    // currently settings.cfg is only read after the startup window launches the game,
    // and rereading binds might be fickle so just enable this
    ud.setup.usejoystick = 1;

    g_myAimMode = 1;
    g_player[0].ps->aim_mode = 1;

    ud.setup.forcesetup       = 1;
    ud.setup.noautoload       = 1;
    ud.setup.fullscreen       = 1;
    ud.setup.usemouse         = 1;

    ud.althud                 = 1;
    ud.auto_run               = 1;
    ud.automsg                = 0;
    ud.autosave               = 1;
    ud.autosavedeletion       = 1;
    ud.autovote               = 0;
    ud.brightness             = 8;
    ud.camerasprite           = -1;
    ud.cashman                = 0;
    ud.color                  = 0;
    ud.config.AmbienceToggle  = 1;
#ifdef AMC_BUILD
    ud.config.AutoAim         = 0;
    ud.config.CheckForUpdates = 0;
#else
    ud.config.AutoAim         = 1;
    ud.config.CheckForUpdates = 1;
#endif
    ud.config.MasterVolume    = 255;
    ud.config.FXVolume        = 255;
    ud.config.VoiceVolume     = 255;
    ud.config.JoystickAimWeight = 4;
    ud.config.JoystickViewCentering = 4;
    ud.config.JoystickAimAssist = 1;
    ud.config.MouseBias       = 0;
    ud.config.MusicDevice     = ASS_AutoDetect;
    ud.config.MusicToggle     = 1;
    ud.config.MusicVolume     = 195;
    ud.config.NumBits         = 16;
    ud.config.NumChannels     = 2;
#ifdef ASS_REVERSESTEREO
    ud.config.ReverseStereo   = 0;
#endif
    ud.config.ShowWeapons     = 0;
    ud.config.SoundToggle     = 1;
    ud.config.VoiceToggle     = 5;  // bitfield, 1 = local, 2 = dummy, 4 = other players in DM
    ud.config.UsePrecache     = 1;
    ud.config.UseSoundPrecache = 0;
    ud.configversion          = 0;
    ud.crosshair              = 1;
    ud.crosshairscale         = 25;
    ud.default_skill          = 1;
    ud.democams               = 1;
    ud.detail                 = 0;
    ud.display_bonus_screen   = 1;
    ud.drawweapon             = 1;
    ud.fov                    = 90;
    ud.fta_on                 = 1;
    ud.god                    = 0;
    ud.hudontop               = 0;
    ud.idplayers              = 1;
    ud.kick_mode              = 0;
    ud.levelstats             = 0;
    ud.lockout                = 0;
    ud.m_marker               = 1;
    ud.maxautosaves           = 5;
    ud.menu_scrollbartilenum  = -1;
    ud.menu_scrollbarz        = 65536;
    ud.menu_scrollcursorz     = 65536;
    ud.menu_slidebarmargin    = 65536;
    ud.menu_slidebarz         = 65536;
    ud.menu_slidecursorz      = 65536;
    ud.menubackground         = 1;
    ud.mouseaiming            = 0;
    ud.mouseflip              = 1;
    ud.msgdisptime            = 120;
    ud.obituaries             = 1;
    ud.pwlockout[0]           = '\0';
    ud.quote_yoffset          = 0;
    ud.runkey_mode            = 0;
    ud.screen_size            = 4;
    ud.screen_tilting         = 1;
    ud.screenfade             = 1;
    ud.shadow_pal             = 4;
    ud.shadows                = 1;
    ud.show_level_text        = 1;
    ud.slidebar_paldisabled   = 1;
    ud.statusbarflags         = STATUSBAR_NOSHRINK;
    ud.statusbarmode          = 1;
    ud.statusbarscale         = 100;
    ud.team                   = 0;
    ud.textscale              = 200;
    ud.viewbob                = 1;
    ud.weaponscale            = 100;
    ud.weaponsway             = 1;
    ud.weaponswitch           = 3;  // new+empty

    Bstrcpy(ud.rtsname, G_DefaultRtsFile());

    if (!CommandName)
        Bstrcpy(szPlayerName, "Player");

#ifndef EDUKE32_STANDALONE
    Bstrcpy(ud.ridecule[0], "An inspiration for birth control.");
    Bstrcpy(ud.ridecule[1], "You're gonna die for that!");
    Bstrcpy(ud.ridecule[2], "It hurts to be you.");
    Bstrcpy(ud.ridecule[3], "Lucky son of a bitch.");
    Bstrcpy(ud.ridecule[4], "Hmmm... payback time.");
    Bstrcpy(ud.ridecule[5], "You bottom dwelling scum sucker.");
    Bstrcpy(ud.ridecule[6], "Damn, you're ugly.");
    Bstrcpy(ud.ridecule[7], "Ha ha ha... wasted!");
    Bstrcpy(ud.ridecule[8], "You suck!");
    Bstrcpy(ud.ridecule[9], "AARRRGHHHHH!!!");
#endif

    CONFIG_SetDefaultKeys(keydefaults);

    memset(ud.config.MouseFunctions, -1, sizeof(ud.config.MouseFunctions));
    memset(ud.config.JoystickFunctions, -1, sizeof(ud.config.JoystickFunctions));
    memset(ud.config.JoystickDigitalFunctions, -1, sizeof(ud.config.JoystickDigitalFunctions));

    CONTROL_MouseSensitivity = DEFAULTMOUSESENSITIVITY;

    for (int i=0; i<MAXMOUSEBUTTONS; i++)
    {
        ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(mousedefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        if (i>=4) continue;
        ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(mouseclickeddefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1, controldevice_mouse);
    }

#if !defined GEKKO
    CONFIG_SetGameControllerDefaults();
#else
    for (int i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdefaults[i]);
        ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(joystickclickeddefaults[i]);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1, controldevice_joystick);
    }

    for (int i=0; i<MAXJOYAXES; i++)
    {
        ud.config.JoystickAnalogueScale[i] = DEFAULTJOYSTICKANALOGUESCALE;
        ud.config.JoystickAnalogueInvert[i] = 0;
        ud.config.JoystickAnalogueDead[i] = DEFAULTJOYSTICKANALOGUEDEAD;
        ud.config.JoystickAnalogueSaturate[i] = DEFAULTJOYSTICKANALOGUESATURATE;
        CONTROL_SetAnalogAxisScale(i, ud.config.JoystickAnalogueScale[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, 0, controldevice_joystick);

        ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i*2]);
        ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i*2+1]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0, controldevice_joystick);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1, controldevice_joystick);

        ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(joystickanalogdefaults[i]);
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i], controldevice_joystick);
    }
#endif

    VM_OnEvent(EVENT_SETDEFAULTS, g_player[myconnectindex].ps->i, myconnectindex);
}


// generates key bindings to reflect changes to keyboard setup
void CONFIG_MapKey(int which, kb_scancode key1, kb_scancode oldkey1, kb_scancode key2, kb_scancode oldkey2)
{
    int const keys[] = { key1, key2, oldkey1, oldkey2 };
    char buf[2*MAXGAMEFUNCLEN];

    if (which == gamefunc_Show_Console)
        OSD_CaptureKey(key1);

    for (int k = 0; (unsigned)k < ARRAY_SIZE(keys); k++)
    {
        if (keys[k] == 0xff || !keys[k])
            continue;

        int match = 0;

        for (; match < ARRAY_SSIZE(sctokeylut); ++match)
        {
            if (keys[k] == sctokeylut[match].sc)
                break;
        }

        tempbuf[0] = 0;

        for (int i=NUMGAMEFUNCTIONS-1; i>=0; i--)
        {
            if (ud.config.KeyboardKeys[i][0] == keys[k] || ud.config.KeyboardKeys[i][1] == keys[k])
            {
                Bsprintf(buf, "gamefunc_%s; ", CONFIG_FunctionNumToName(i));
                Bstrcat(tempbuf,buf);
            }
        }

        int const len = Bstrlen(tempbuf);

        if (len >= 2)
        {
            tempbuf[len-2] = 0;  // cut off the trailing "; "
            CONTROL_BindKey(keys[k], tempbuf, 1, sctokeylut[match].key ? sctokeylut[match].key : "<?>");
        }
        else
        {
            CONTROL_FreeKeyBind(keys[k]);
        }
    }
}


void CONFIG_SetupMouse(void)
{
    if (ud.config.scripthandle < 0)
        return;

    char str[80];
    char temp[80];

    for (int i=0; i<MAXMOUSEBUTTONS; i++)
    {
        Bsprintf(str,"MouseButton%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"MouseButtonClicked%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    for (int i=0; i<MAXMOUSEBUTTONS; i++)
    {
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1,  controldevice_mouse);
    }
}


void CONFIG_SetupJoystick(void)
{
    int32_t i;
    char str[80];
    char temp[80];
    int32_t scale;
    double sens;

    if (ud.config.scripthandle < 0) return;

    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        Bsprintf(str,"ControllerButton%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerButtonClicked%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (i=0; i<MAXJOYAXES; i++)
    {
        Bsprintf(str,"ControllerAnalogAxes%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str,"ControllerDigitalAxes%d_0",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerDigitalAxes%d_1",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerAnalogSensitivity%d",i);
        sens = ud.config.JoystickAnalogueSensitivity[i];
        SCRIPT_GetDouble(ud.config.scripthandle, "Controls", str, &sens);
        ud.config.JoystickAnalogueSensitivity[i] = sens;

        Bsprintf(str,"ControllerAnalogInvert%d",i);
        scale = ud.config.JoystickAnalogueInvert[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueInvert[i] = scale;

        Bsprintf(str,"ControllerAnalogDead%d",i);
        scale = ud.config.JoystickAnalogueDead[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueDead[i] = scale;

        Bsprintf(str,"ControllerAnalogSaturate%d",i);
        scale = ud.config.JoystickAnalogueSaturate[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueSaturate[i] = scale;
    }

    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1,  controldevice_joystick);
    }

    for (i=0; i<MAXJOYAXES; i++)
    {
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1);
        CONTROL_SetAnalogAxisSensitivity(i, ud.config.JoystickAnalogueSensitivity[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, ud.config.JoystickAnalogueInvert[i]);
        JOYSTICK_SetDeadZone(i, ud.config.JoystickAnalogueDead[i], ud.config.JoystickAnalogueSaturate[i]);
    }
}

struct GameControllerButtonSetting
{
    GameControllerButton button;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickButtonFunction(button, 0, function);
    }
};
struct GameControllerAnalogAxisSetting
{
    GameControllerAxis axis;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickAnalogAxisFunction(axis, function);
    }
};
struct GameControllerDigitalAxisSetting
{
    GameControllerAxis axis;
    int polarity;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickDigitalAxisFunction(axis, polarity, function);
    }
};

static void CONFIG_SetGameControllerAxesModern()
{
    static GameControllerAnalogAxisSetting const analogAxes[] =
    {
        { CONTROLLER_AXIS_LEFTX, analog_strafing },
        { CONTROLLER_AXIS_LEFTY, analog_moving },
        { CONTROLLER_AXIS_RIGHTX, analog_turning },
        { CONTROLLER_AXIS_RIGHTY, analog_lookingupanddown },
    };

    CONFIG_SetJoystickAnalogAxisSensitivity(CONTROLLER_AXIS_RIGHTX, DEFAULTJOYSTICKANALOGUESENSITIVITY);
    CONFIG_SetJoystickAnalogAxisSensitivity(CONTROLLER_AXIS_RIGHTY, DEFAULTJOYSTICKANALOGUESENSITIVITY);

    for (auto const & analogAxis : analogAxes)
        analogAxis.apply();
}

void CONFIG_SetGameControllerDefaults()
{
    CONFIG_SetGameControllerDefaultsClear();
    CONFIG_SetGameControllerAxesModern();

    static GameControllerButtonSetting const buttons[] =
    {
        { CONTROLLER_BUTTON_A, gamefunc_Open },
        { CONTROLLER_BUTTON_B, gamefunc_Inventory },
        { CONTROLLER_BUTTON_Y, gamefunc_Quick_Kick },
        { CONTROLLER_BUTTON_BACK, gamefunc_Map },
        { CONTROLLER_BUTTON_LEFTSTICK, gamefunc_Run },
        { CONTROLLER_BUTTON_RIGHTSTICK, gamefunc_Toggle_Crouch },
        { CONTROLLER_BUTTON_DPAD_UP, gamefunc_Previous_Weapon },
        { CONTROLLER_BUTTON_DPAD_DOWN, gamefunc_Next_Weapon },
        { CONTROLLER_BUTTON_LEFTSHOULDER, gamefunc_Crouch },
        { CONTROLLER_BUTTON_RIGHTSHOULDER, gamefunc_Alt_Fire },
        { CONTROLLER_BUTTON_MISC, gamefunc_Third_Person_View },
    };

    static GameControllerButtonSetting const buttonsDuke[] =
    {
        { CONTROLLER_BUTTON_X, gamefunc_Inventory },
        { CONTROLLER_BUTTON_DPAD_LEFT, gamefunc_Inventory_Left },
        { CONTROLLER_BUTTON_DPAD_RIGHT, gamefunc_Inventory_Right },
    };

    static GameControllerButtonSetting const buttonsFury[] =
    {
        { CONTROLLER_BUTTON_X, gamefunc_Steroids }, // Reload
        { CONTROLLER_BUTTON_DPAD_LEFT, gamefunc_MedKit },
        { CONTROLLER_BUTTON_DPAD_RIGHT, gamefunc_NightVision }, // Radar
    };

    static GameControllerDigitalAxisSetting const digitalAxes[] =
    {
        { CONTROLLER_AXIS_TRIGGERLEFT, 1, gamefunc_Jump },
        { CONTROLLER_AXIS_TRIGGERRIGHT, 1, gamefunc_Fire },
    };

    for (auto const & button : buttons)
        button.apply();

    if (FURY)
    {
        for (auto const & button : buttonsFury)
            button.apply();
    }
    else
    {
        for (auto const & button : buttonsDuke)
            button.apply();
    }

    for (auto const & digitalAxis : digitalAxes)
        digitalAxis.apply();

    ud.config.JoystickAimAssist     = 1;
    ud.config.JoystickAimWeight     = 4;
    ud.config.JoystickViewCentering = 4;
    ud.config.controllerRumble = 1;
}

void CONFIG_SetGameControllerDefaultsClear()
{
    for (int i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        CONFIG_SetJoystickButtonFunction(i, 0, -1);
        CONFIG_SetJoystickButtonFunction(i, 1, -1);
    }

    for (int i=0; i<MAXJOYAXES; i++)
    {
        CONFIG_SetJoystickAnalogAxisSensitivity(i, DEFAULTJOYSTICKANALOGUESENSITIVITY);
        CONFIG_SetJoystickAnalogAxisInvert(i, 0);
        CONFIG_SetJoystickAnalogAxisDeadSaturate(i, DEFAULTJOYSTICKANALOGUEDEAD, DEFAULTJOYSTICKANALOGUESATURATE);

        CONFIG_SetJoystickDigitalAxisFunction(i, 0, -1);
        CONFIG_SetJoystickDigitalAxisFunction(i, 1, -1);

        CONFIG_SetJoystickAnalogAxisFunction(i, -1);
    }
}

int CONFIG_ReadSetup(void)
{
    char tempbuf[1024];

    CONTROL_ClearAssignments();
    CONFIG_SetDefaults();

    ud.config.setupread = 1;
    pathsearchmode = 1;

    if (ud.config.scripthandle < 0)
    {
        if (buildvfs_exists(g_setupFileName))  // JBF 20031211
            ud.config.scripthandle = SCRIPT_Load(g_setupFileName);
#if !defined(EDUKE32_TOUCH_DEVICES) && !defined(EDUKE32_STANDALONE)
        else if (buildvfs_exists(SETUPFILENAME))
        {
            int const i = wm_ynbox("Import Configuration Settings",
                                   "Configuration file %s not found. "
                                   "Import configuration data from %s?",
                                   g_setupFileName, SETUPFILENAME);
            if (i)
                ud.config.scripthandle = SCRIPT_Load(SETUPFILENAME);
        }
#endif
#ifdef BACKUP_SETUPFILENAME
        else if (buildvfs_exists(BACKUP_SETUPFILENAME))
        {
            int const i = wm_ynbox("Import Configuration Settings",
                                   "Configuration file %s not found. "
                                   "Import configuration data from %s?",
                                   g_setupFileName, BACKUP_SETUPFILENAME);
            if (i)
                ud.config.scripthandle = SCRIPT_Load(BACKUP_SETUPFILENAME);
        }
#endif
    }

    pathsearchmode = 0;

    if (ud.config.scripthandle < 0)
        return -1;

    char commmacro[] = "CommbatMacro# ";

    for (int i = 0; i < MAXRIDECULE; i++)
    {
        commmacro[13] = i+'0';
        SCRIPT_GetString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[i][0]);
    }

    if (!CommandName)
    {
        Bmemset(tempbuf, 0, sizeof(tempbuf));
        SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","PlayerName",&tempbuf[0]);

        char nameBuf[64];

        while (Bstrlen(OSD_StripColors(nameBuf, tempbuf)) > 10)
            tempbuf[Bstrlen(tempbuf) - 1] = '\0';

        Bstrncpyz(szPlayerName, tempbuf, sizeof(szPlayerName));
    }

    SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "ConfigVersion", &ud.configversion);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "ForceSetup", &ud.setup.forcesetup);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "NoAutoLoad", &ud.setup.noautoload);

    int32_t cachesize;
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "CacheSize", &cachesize);

    if (cachesize > MAXCACHE1DSIZE)
        MAXCACHE1DSIZE = cachesize;

    if (g_noSetup == 0 && g_modDir[0] == '/')
    {
        SCRIPT_GetString(ud.config.scripthandle, "Setup","ModDir",&g_modDir[0]);

        if (!buildvfs_isdir(g_modDir))
        {
            LOG_F(WARNING, "Invalid user directory specified in cfg file: %s", g_modDir);
            Bsprintf(g_modDir,"/");
        }
    }

    if (g_grpNamePtr == NULL && g_addonNum == 0)
    {
        SCRIPT_GetStringPtr(ud.config.scripthandle, "Setup", "SelectedGRP", &g_grpNamePtr);
        if (g_grpNamePtr && !Bstrlen(g_grpNamePtr))
            g_grpNamePtr = dup_filename(G_DefaultGrpFile());
    }

    if (!NAM_WW2GI)
    {
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Out", &ud.lockout);
        SCRIPT_GetString(ud.config.scripthandle, "Screen Setup", "Password", &ud.pwlockout[0]);
    }

    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", (int32_t *)&maxrefreshfreq);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", &ud.setup.bpp);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenDisplay", &r_displayindex);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", &ud.setup.ydim);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", &ud.setup.fullscreen);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", &ud.setup.xdim);
    vec2_t windowPos;
    if (!SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", &windowPos.x)
        && !SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", &windowPos.y))
    {
        g_windowPos = windowPos;
        g_windowPosValid = true;
    }

    if (ud.setup.bpp < 8) ud.setup.bpp = 32;

#ifdef POLYMER
    int32_t rendmode = 0;
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Polymer", &rendmode);
    glrendmode = (rendmode > 0) ? REND_POLYMER : REND_POLYMOST;
#endif

    SCRIPT_GetNumber(ud.config.scripthandle, "Misc", "Executions", &ud.executions);

#ifdef _WIN32
    SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", &ud.config.CheckForUpdates);
    SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", &ud.config.LastUpdateCheck);
#endif

    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "UseJoystick", &ud.setup.usejoystick);
    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "UseMouse", &ud.setup.usemouse);

    // restore localization
    char locale[64] = "en";
    SCRIPT_GetString(ud.config.scripthandle, "Misc", "Locale", &locale[0]);
    localeSetCurrent(locale);

    ud.config.setupread = 1;
    return 0;
}

void CONFIG_ReadSettings(void)
{
    char *dummy = NULL;
    char *const setupFileName = Xstrdup(g_setupFileName);
    char *const p = Bstrtoken(setupFileName, ".", &dummy, 1);

    if (!p || !Bstrcmp(g_setupFileName, SETUPFILENAME))
        Bsprintf(tempbuf, "settings.cfg");
    else
        Bsprintf(tempbuf, "%s_settings.cfg", p);

    Xfree(setupFileName);

    OSD_Exec(tempbuf);

    ud.config.setupread = 2;

    return;
}

void CONFIG_WriteSettings(void) // save binds and aliases to <cfgname>_settings.cfg
{
    if (ud.config.setupread != 2) return;

    char *dummy = NULL;
    char filename[BMAX_PATH];

    if (!Bstrcmp(g_setupFileName, SETUPFILENAME))
        Bsprintf(filename, "settings.cfg");
    else
        Bsprintf(filename, "%s_settings.cfg", Bstrtoken(g_setupFileName, ".", &dummy, 1));

    buildvfs_FILE fp = buildvfs_fopen_write(filename);

    if (fp)
    {
        buildvfs_fputstr(fp, "// this file is automatically generated by ");
        buildvfs_fputstrptr(fp, AppProperName);
        buildvfs_fputstr(fp,"\nunbindall\n");

        for (int i=0; i<MAXBOUNDKEYS+MAXMOUSEBUTTONS; i++)
        {
            if (CONTROL_KeyIsBound(i))
            {
                buildvfs_fputstr(fp, "bind \"");
                buildvfs_fputstrptr(fp, CONTROL_KeyBinds[i].key);
                if (CONTROL_KeyBinds[i].repeat)
                    buildvfs_fputstr(fp, "\" \"");
                else
                    buildvfs_fputstr(fp, "\" norepeat \"");
                buildvfs_fputstrptr(fp, CONTROL_KeyBinds[i].cmdstr);
                buildvfs_fputstr(fp, "\"\n");
            }
        }

        for (int i=0; i<NUMGAMEFUNCTIONS; ++i)
        {
            char const * name = CONFIG_FunctionNumToName(i);
            if (name && name[0] != '\0' && (ud.config.KeyboardKeys[i][0] == 0xff || !ud.config.KeyboardKeys[i][0]))
            {
                buildvfs_fputstr(fp, "unbound ");
                buildvfs_fputstrptr(fp, name);
                buildvfs_fputstr(fp, "\n");
            }
        }

        OSD_WriteAliases(fp);

        if (g_crosshairSum != -1 && g_crosshairSum != DefaultCrosshairColors.r+(DefaultCrosshairColors.g<<8)+(DefaultCrosshairColors.b<<16))
        {
            buildvfs_fputstr(fp, "crosshaircolor ");
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %d %d\n", CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);
            buildvfs_fputstrptr(fp, buf);
        }

        OSD_WriteCvars(fp);

        buildvfs_fclose(fp);

        LOG_F(INFO, "Wrote %s", filename);

        return;
    }

    LOG_F(ERROR, "Unable to write %s: %s.", filename, strerror(errno));
}

void CONFIG_WriteSetup(uint32_t flags)
{
    if (!ud.config.setupread) return;

    if (ud.config.scripthandle < 0)
        ud.config.scripthandle = SCRIPT_Init(g_setupFileName);

    SCRIPT_PutNumber(ud.config.scripthandle, "Misc", "Executions", ud.executions, FALSE, FALSE);

    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "CacheSize", MAXCACHE1DSIZE, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ConfigVersion", BYTEVERSION_EDUKE32, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ForceSetup", ud.setup.forcesetup, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "NoAutoLoad", ud.setup.noautoload, FALSE, FALSE);

#ifdef POLYMER
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Polymer", glrendmode == REND_POLYMER, FALSE, FALSE);
#endif

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", ud.setup.bpp, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenDisplay", r_displayindex, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", ud.setup.ydim, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", ud.setup.fullscreen, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", ud.setup.xdim, FALSE, FALSE);

    if (g_grpNamePtr && !g_addonNum)
        SCRIPT_PutString(ud.config.scripthandle, "Setup", "SelectedGRP", g_grpNamePtr);

#ifdef STARTUP_SETUP_WINDOW
    if (g_noSetup == 0)
        SCRIPT_PutString(ud.config.scripthandle, "Setup", "ModDir", &g_modDir[0]);
#endif
    // exit early after only updating the values that can be changed from the startup window
    if (flags & 1)
    {
        SCRIPT_Save(ud.config.scripthandle, g_setupFileName);
        SCRIPT_Free(ud.config.scripthandle);
        return;
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", maxrefreshfreq, FALSE, FALSE);

    if (g_windowPosValid)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", g_windowPos.x, FALSE, FALSE);
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", g_windowPos.y, FALSE, FALSE);
    }

    if (!NAM_WW2GI)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Out",ud.lockout,FALSE,FALSE);
        SCRIPT_PutString(ud.config.scripthandle, "Screen Setup", "Password",ud.pwlockout);
    }

#ifdef _WIN32
    SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", ud.config.CheckForUpdates, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", ud.config.LastUpdateCheck, FALSE, FALSE);
#endif

    if (ud.setup.usemouse)
    {
        for (int i=0; i<MAXMOUSEBUTTONS; i++)
        {
            Bsprintf(buf, "MouseButton%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][0]));

            if (i >= (MAXMOUSEBUTTONS-2)) continue;

            Bsprintf(buf, "MouseButtonClicked%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][1]));
        }
    }

    if (ud.setup.usejoystick)
    {
        for (int dummy=0; dummy<MAXJOYBUTTONSANDHATS; dummy++)
        {
            Bsprintf(buf, "ControllerButton%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][0]));

            Bsprintf(buf, "ControllerButtonClicked%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][1]));
        }
        for (int dummy=0; dummy<MAXJOYAXES; dummy++)
        {
            Bsprintf(buf, "ControllerAnalogAxes%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_AnalogNumToName(ud.config.JoystickAnalogueAxes[dummy]));

            Bsprintf(buf, "ControllerDigitalAxes%d_0", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][0]));

            Bsprintf(buf, "ControllerDigitalAxes%d_1", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][1]));

            Bsprintf(buf, "ControllerAnalogSensitivity%d", dummy);
            SCRIPT_PutDouble(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueSensitivity[dummy], FALSE);

            Bsprintf(buf, "ControllerAnalogInvert%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueInvert[dummy], FALSE, FALSE);

            Bsprintf(buf, "ControllerAnalogDead%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueDead[dummy], FALSE, FALSE);

            Bsprintf(buf, "ControllerAnalogSaturate%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueSaturate[dummy], FALSE, FALSE);
        }
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "UseJoystick", ud.setup.usejoystick, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "UseMouse", ud.setup.usemouse, FALSE, FALSE);

    if (!CommandName)
        SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","PlayerName",&szPlayerName[0]);

    SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    char commmacro[] = "CommbatMacro# ";

    for (int dummy = 0; dummy < MAXRIDECULE; dummy++)
    {
        commmacro[13] = dummy+'0';
        SCRIPT_PutString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
    }

    // save localization
    const char* locale = localeGetCurrent();
    SCRIPT_PutString(ud.config.scripthandle, "Misc", "Locale", locale ? locale : "en");

    SCRIPT_Save(ud.config.scripthandle, g_setupFileName);

    if ((flags & 2) == 0)
        SCRIPT_Free(ud.config.scripthandle);

    LOG_F(INFO, "Wrote %s",g_setupFileName);
    CONFIG_WriteSettings();
    Bfflush(NULL);
}

char const * CONFIG_GetGameFuncOnKeyboard(int gameFunc)
{
    const char * string0 = KB_ScanCodeToString(ud.config.KeyboardKeys[gameFunc][0]);
    return string0[0] == '\0' ? KB_ScanCodeToString(ud.config.KeyboardKeys[gameFunc][1]) : string0;
}

const char mbtn_to_name[MAXMOUSEBUTTONS][2][32] = {
    {"Left Mouse",       "Double Left Mouse"},
    {"Right Mouse",      "Double Right Mouse"},
    {"Middle Mouse",     "Double Middle Mouse"},
    {"Mouse 4",   "Double Mouse 4"},
    {"Wheel Up",   ""},
    {"Wheel Down", ""},
    {"Mouse 5",   "Double Mouse 5"},
    {"Mouse 6",   "Double Mouse 6"},
    {"Mouse 7",   "Double Mouse 7"},
    {"Mouse 8",   "Double Mouse 8"},
};

char const * CONFIG_GetGameFuncOnMouse(int gameFunc)
{
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < MAXMOUSEBUTTONS; ++i)
            if (ud.config.MouseFunctions[i][j] == gameFunc)
                return mbtn_to_name[i][j];

    return "";
}

char const * CONFIG_GetGameFuncOnJoystick(int gameFunc)
{
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < joystick.numButtons; ++i)
            if (ud.config.JoystickFunctions[i][j] == gameFunc)
                return joyGetName(1, i);

    for (int i = 0; i < joystick.numAxes; ++i)
        for (int j = 0; j < 2; ++j)
            if (ud.config.JoystickDigitalFunctions[i][j] == gameFunc)
                return joyGetName(0, i);

    return "";
}

static const char *CONFIG_GetMapEntryName(char m[], char const * const mapname)
{
    strcpy(m, mapname);

    char *p = strrchr(m, '/');
    if (!p) p = strrchr(m, '\\');
    if (p) Bmemmove(m, p, Bstrlen(p)+1);
    for (p=m; *p; p++) *p = tolower(*p);

    // cheap hack because SCRIPT_GetNumber doesn't like the slashes
    p = m;
    while (*p == '/') p++;

    return p;
}

static void CONFIG_GetMD4EntryName(char m[], uint8_t const * const md4)
{
    sprintf(m, "MD4_%08x%08x%08x%08x",
            B_BIG32(B_UNBUF32(&md4[0])), B_BIG32(B_UNBUF32(&md4[4])),
            B_BIG32(B_UNBUF32(&md4[8])), B_BIG32(B_UNBUF32(&md4[12])));
}

int32_t CONFIG_GetMapBestTime(char const * const mapname, uint8_t const * const mapmd4)
{
    if (!ud.config.setupread || ud.config.scripthandle < 0)
        return -1;

    char m[37];

    CONFIG_GetMD4EntryName(m, mapmd4);

    int32_t t = -1;
    if (SCRIPT_GetNumber(ud.config.scripthandle, "MapTimes", m, &t))
    {
        // fall back to map filenames
        char m2[BMAX_PATH];
        auto p = CONFIG_GetMapEntryName(m2, mapname);

        SCRIPT_GetNumber(ud.config.scripthandle, "MapTimes", p, &t);
    }

    return t;
}

int CONFIG_SetMapBestTime(uint8_t const * const mapmd4, int32_t tm)
{
    if (ud.config.scripthandle < 0 && (ud.config.scripthandle = SCRIPT_Init(g_setupFileName)) < 0)
        return -1;

    char m[37];

    CONFIG_GetMD4EntryName(m, mapmd4);
    SCRIPT_PutNumber(ud.config.scripthandle, "MapTimes", m, tm, FALSE, FALSE);

    return 0;
}
