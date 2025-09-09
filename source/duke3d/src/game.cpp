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

#define game_c_

#include "anim.h"
#include "cheats.h"
#include "cmdline.h"
#include "colmatch.h"
#include "communityapi.h"
#include "compat.h"
#include "crc32.h"
#include "demo.h"
#include "duke3d.h"
#include "input.h"
#include "menus.h"
#include "microprofile.h"
//#include "minicoro.h"
#include "network.h"
#include "osdcmds.h"
#include "osdfuncs.h"
#include "palette.h"
#include "renderlayer.h"
#include "savegame.h"
#include "sbar.h"
#include "screens.h"

#ifdef __ANDROID__
#include "android.h"
#endif

#include "vfs.h"

// Uncomment to prevent anything except mirrors from drawing. It is sensible to
// also uncomment ENGINE_CLEAR_SCREEN in build/src/engine_priv.h.
//#define DEBUG_MIRRORS_ONLY

#if KRANDDEBUG
# define GAME_INLINE
# define GAME_STATIC
#else
# define GAME_INLINE inline
# define GAME_STATIC static
#endif

#ifdef _WIN32
# include <shellapi.h>
# define UPDATEINTERVAL 604800 // 1w
# include "winbits.h"
#else
# ifndef GEKKO
#  include <sys/ioctl.h>
# endif
#endif /* _WIN32 */

const char* AppProperName = APPNAME;
const char* AppTechnicalName = APPBASENAME;

int32_t g_quitDeadline = 0;

int32_t g_cameraDistance = 0, g_cameraClock = 0;
static int32_t g_quickExit;

char boardfilename[BMAX_PATH] = {0};
char currentboardfilename[BMAX_PATH] = {0};

int32_t voting = -1;
int32_t vote_map = -1, vote_episode = -1;

int32_t g_BenchmarkMode = BENCHMARKMODE_OFF;

int32_t g_Debug = 0;

int32_t g_vm_preempt = 1;

#ifndef EDUKE32_STANDALONE
static const char *defaultrtsfilename[GAMECOUNT] = { "DUKE.RTS", "NAM.RTS", "NAPALM.RTS", "WW2GI.RTS" };
#endif

int32_t g_Shareware = 0;

// This was 32 for a while, but I think lowering it to 24 will help things like the Dingoo.
// Ideally, we would look at our memory usage on our most cramped platform and figure out
// how much of that is needed for the underlying OS and things like SDL instead of guessing
#ifndef GEKKO
int32_t MAXCACHE1DSIZE = (96*1024*1024);
#else
int32_t MAXCACHE1DSIZE = (8*1024*1024);
#endif

int32_t tempwallptr;

static int32_t nonsharedtimer;

int32_t ticrandomseed;

int32_t hud_showmapname = 1;

int32_t g_levelTextTime = 0;

#if defined(RENDERTYPEWIN) && defined(USE_OPENGL)
extern char forcegl;
#endif

void M32RunScript(const char *s) { UNREFERENCED_PARAMETER(s); };  // needed for linking since it's referenced from build/src/osd.c

const char *G_DefaultRtsFile(void)
{
#ifndef EDUKE32_STANDALONE
    if (DUKE)
        return defaultrtsfilename[GAME_DUKE];
    else if (WW2GI)
        return defaultrtsfilename[GAME_WW2GI];
    else if (NAPALM)
    {
        if (!testkopen(defaultrtsfilename[GAME_NAPALM],0) && testkopen(defaultrtsfilename[GAME_NAM],0))
            return defaultrtsfilename[GAME_NAM]; // NAM/NAPALM Sharing
        else
            return defaultrtsfilename[GAME_NAPALM];
    }
    else if (NAM)
    {
        if (!testkopen(defaultrtsfilename[GAME_NAM],0) && testkopen(defaultrtsfilename[GAME_NAPALM],0))
            return defaultrtsfilename[GAME_NAPALM]; // NAM/NAPALM Sharing
        else
            return defaultrtsfilename[GAME_NAM];
    }
#endif

    return "";
}

enum gametokens
{
    T_INCLUDE = 0,
    T_INTERFACE = 0,
    T_LOADGRP = 1,
    T_MODE = 1,
    T_CACHESIZE = 2,
    T_ALLOW = 2,
    T_DEFINE,
    T_NOAUTOLOAD,
    T_INCLUDEDEFAULT,
    T_MUSIC,
    T_SOUND,
    T_FILE,
    T_CUTSCENE,
    T_ANIMSOUNDS,
    T_NOFLOORPALRANGE,
    T_ID,
    T_MINPITCH,
    T_MAXPITCH,
    T_PRIORITY,
    T_TYPE,
    T_DISTANCE,
    T_VOLUME,
    T_DELAY,
    T_GLOBALGAMEFLAGS,
    T_ASPECT,
    T_FORCEFILTER,
    T_FORCENOFILTER,
    T_TEXTUREFILTER,
    T_NEWGAMECHOICES,
    T_CHOICE,
    T_NAME,
    T_LOCKED,
    T_HIDDEN,
    T_USERCONTENT,
    T_LOCALIZATION,
    T_KEYCONFIG,
    T_CUSTOMSETTINGS,
    T_INDEX,
    T_LINK,
    T_FONT,
    T_VSTRINGS,
    T_VALUES,
};

static void gameTimerHandler(void)
{
    MUSIC_Update();
    G_HandleSpecialKeys();
}


void G_HandleSpecialKeys(void)
{
    auto &myplayer = *g_player[myconnectindex].ps;

    if (g_networkMode != NET_DEDICATED_SERVER && ALT_IS_PRESSED && KB_KeyPressed(sc_Enter))
    {
        if (videoSetGameMode(!ud.setup.fullscreen, ud.setup.xdim, ud.setup.ydim, ud.setup.bpp, ud.detail))
        {
            LOG_F(ERROR, "Failed setting video mode!");

            if (videoSetGameMode(ud.setup.fullscreen, ud.setup.xdim, ud.setup.ydim, ud.setup.bpp, ud.detail))
                G_GameExit("Fatal error: unable to recover from failure setting video mode!");
        }
        else
            ud.setup.fullscreen = !ud.setup.fullscreen;

        KB_ClearKeyDown(sc_Enter);
        g_restorePalette = 1;
        G_UpdateScreenArea();
    }

    if (KB_UnBoundKeyPressed(sc_F12))
    {
        KB_ClearKeyDown(sc_F12);
        videoCaptureScreen(
#ifdef AMC_BUILD
        "amc0000.tga"
#elif !defined(EDUKE32_STANDALONE)
        "duke0000.tga"
#else
        "capt0000.tga"
#endif
        ,
        0);
        P_DoQuote(QUOTE_SCREEN_SAVED, &myplayer);
    }

    // only dispatch commands here when not in a game
    if ((myplayer.gm & MODE_GAME) != MODE_GAME)
        OSD_DispatchQueued();

#ifdef DEBUGGINGAIDS
    if (g_quickExit == 0 && KB_KeyPressed(sc_LeftControl) && KB_KeyPressed(sc_LeftAlt) && KB_KeyPressed(sc_End))
    {
        g_quickExit = 1;
        G_GameExit("Quick Exit.");
    }
#endif
}

void G_GameQuit(void)
{
    if (numplayers < 2)
        G_GameExit();

    if (g_gameQuit == 0)
    {
        g_gameQuit = 1;
        g_quitDeadline = (int32_t) totalclock+120;
        g_netDisconnect = 1;
    }

    if ((totalclock > g_quitDeadline) && (g_gameQuit == 1))
        G_GameExit("Timed out.");
}


int32_t A_CheckInventorySprite(spritetype *s)
{
    switch (tileGetMapping(s->picnum))
    {
    case FIRSTAID__:
    case STEROIDS__:
    case HEATSENSOR__:
    case BOOTS__:
    case JETPACK__:
    case HOLODUKE__:
    case AIRTANK__:
        return 1;
    default:
        return 0;
    }
}


EDUKE32_NORETURN void app_exit(int returnCode);

//static int g_programExitCode = INT_MIN;

//void g_switchRoutine(mco_coro *co)
//{
//    mco_result res = mco_resume(co);
//    Bassert(res == MCO_SUCCESS);
//
//    if (g_programExitCode != INT_MIN)
//    {
//        if (mco_running() == nullptr)
//            Bexit(g_programExitCode);
//
//        res = mco_yield(mco_running());
//        Bassert(res == MCO_SUCCESS);
//    }
//
//    if (res != MCO_SUCCESS)
//        fatal_exit(mco_result_description(res));
//}

void app_exit(int returnCode)
{
    //if (mco_running())
    //{
    //    g_programExitCode = returnCode;
    //    mco_yield(mco_running());
    //}

#ifndef NETCODE_DISABLE
    enet_deinitialize();
#endif
    if (returnCode != EXIT_SUCCESS)
        Bexit(returnCode);

    Bexit(EXIT_SUCCESS);
}

void G_GameExit(const char *msg)
{
    if (msg && *msg != 0 && g_player[myconnectindex].ps != NULL)
        g_player[myconnectindex].ps->palette = BASEPAL;

    if (ud.recstat == 1)
        G_CloseDemoWrite();
    else if (ud.recstat == 2)
        MAYBE_FCLOSE_AND_NULL(g_demo_filePtr);
    // JBF: fixes crash on demo playback
    // PK: modified from original

    if (!g_quickExit)
    {
        if (VM_OnEventWithReturn(EVENT_EXITGAMESCREEN, g_player[myconnectindex].ps->i, myconnectindex, 0) == 0 &&
           g_mostConcurrentPlayers > 1 && g_player[myconnectindex].ps->gm & MODE_GAME && GTFLAGS(GAMETYPE_SCORESHEET) && msg == nullptr)
        {
            G_BonusScreen(1);
            videoSetGameMode(ud.setup.fullscreen, ud.setup.xdim, ud.setup.ydim, ud.setup.bpp, ud.detail);
        }

        // shareware and TEN screens
        if (VM_OnEventWithReturn(EVENT_EXITPROGRAMSCREEN, g_player[myconnectindex].ps->i, myconnectindex, 0) == 0 && msg == nullptr)
            G_DisplayExtraScreens();
    }

    if (msg != nullptr)
        LOG_F(INFO, "%s", msg);

    if (in3dmode())
        G_Shutdown();

    if (msg != nullptr)
    {
        char titlebuf[256];
        Bsnprintf(titlebuf, sizeof(titlebuf), HEAD2 " %s", s_buildRev);
        wm_msgbox(titlebuf, "%s", msg);
    }

    Bfflush(NULL);
    app_exit(EXIT_SUCCESS);
}


#ifdef YAX_DEBUG
// ugh...
char m32_debugstr[64][128];
int32_t m32_numdebuglines=0;

static void M32_drawdebug(void)
{
    int i, col=paletteGetClosestColor(255,255,255);
    int x=4, y=8;

    if (m32_numdebuglines>0)
    {
        videoBeginDrawing();
        for (i=0; i<m32_numdebuglines && y<ydim-8; i++, y+=8)
            printext256(x,y,col,0,m32_debugstr[i],xdim>640?0:1);
        videoEndDrawing();
    }
    m32_numdebuglines=0;
}
#endif


static int32_t G_DoThirdPerson(const DukePlayer_t *pp, vec3_t *vect, int16_t *vsectnum, int16_t ang, int16_t horiz)
{
    auto const sp = &sprite[pp->i];
    int32_t i, hx, hy;
    int32_t bakcstat = sp->cstat;
    hitdata_t hit;

    vec3_t n = {
        sintable[(ang+1536)&2047]>>4,
        sintable[(ang+1024)&2047]>>4,
        (horiz-100) * 128
    };

    updatesectorz(vect->x,vect->y,vect->z,vsectnum);

    sp->cstat &= ~0x101;
    hitscan(vect, *vsectnum, n.x,n.y,n.z, &hit, CLIPMASK1);
    sp->cstat = bakcstat;

    if (*vsectnum < 0)
        return -1;

    hx = hit.x-(vect->x);
    hy = hit.y-(vect->y);

    if (klabs(n.x)+klabs(n.y) > klabs(hx)+klabs(hy))
    {
        *vsectnum = hit.sect;

        if (hit.wall >= 0)
        {
            int32_t daang = getangle(wall[wall[hit.wall].point2].x-wall[hit.wall].x,
                             wall[wall[hit.wall].point2].y-wall[hit.wall].y);

            i = n.x*sintable[daang] + n.y*sintable[(daang+1536)&2047];

            if (klabs(n.x) > klabs(n.y))
                hx -= mulscale28(n.x,i);
            else hy -= mulscale28(n.y,i);
        }
        else if (hit.sprite < 0)
        {
            if (klabs(n.x) > klabs(n.y))
                hx -= (n.x>>5);
            else hy -= (n.y>>5);
        }

        if (klabs(n.x) > klabs(n.y))
            i = divscale16(hx,n.x);
        else i = divscale16(hy,n.y);

        if (i < CAMERADIST)
            CAMERADIST = i;
    }

    vect->x += mulscale16(n.x,CAMERADIST);
    vect->y += mulscale16(n.y,CAMERADIST);
    vect->z += mulscale16(n.z,CAMERADIST);

    CAMERADIST = min(CAMERADIST+(((int32_t) totalclock-CAMERACLOCK)<<10),65536);
    CAMERACLOCK = (int32_t) totalclock;

    updatesectorz(vect->x,vect->y,vect->z,vsectnum);

    return 0;
}

#ifdef LEGACY_ROR
char ror_protectedsectors[bitmap_size(MAXSECTORS)];
static int32_t drawing_ror = 0;
static int32_t ror_sprite = -1;

static void G_OROR_DupeSprites(spritetype const *sp)
{
    // dupe the sprites touching the portal to the other sector
    int32_t k;
    spritetype const *refsp;

    refsp = &sprite[sp->yvel];

    for (SPRITES_OF_SECT(sp->sectnum, k))
    {
        if (spritesortcnt >= maxspritesonscreen)
            break;

        if (sprite[k].picnum != SECTOREFFECTOR && sprite[k].z >= sp->z)
        {
            tspriteptr_t tsp = renderAddTSpriteFromSprite(k);
            Duke_ApplySpritePropertiesToTSprite(tsp, (uspriteptr_t)&sprite[k]);

            tsp->x += (refsp->x - sp->x);
            tsp->y += (refsp->y - sp->y);
            tsp->z += -sp->z + actor[sp->yvel].ceilingz;
            tsp->sectnum = refsp->sectnum;

//            OSD_Printf("duped sprite of pic %d at %d %d %d\n",tsp->picnum,tsp->x,tsp->y,tsp->z);
        }
    }
}

static int16_t SE40backupStat[MAXSECTORS];
static int32_t SE40backupZ[MAXSECTORS];

static void G_SE40(int32_t smoothratio)
{
    if ((unsigned)ror_sprite < MAXSPRITES)
    {
        int32_t x, y, z;
        int16_t sect;
        int32_t level = 0;
        auto const sp = &sprite[ror_sprite];
        const int32_t sprite2 = sp->yvel;

        if ((unsigned)sprite2 >= MAXSPRITES)
            return;

        if (klabs(sector[sp->sectnum].floorz - sp->z) < klabs(sector[sprite[sprite2].sectnum].floorz - sprite[sprite2].z))
            level = 1;

        x = CAMERA(pos.x) - sp->x;
        y = CAMERA(pos.y) - sp->y;
        z = CAMERA(pos.z) - (level ? sector[sp->sectnum].floorz : sector[sp->sectnum].ceilingz);

        sect = sprite[sprite2].sectnum;
        updatesector(sprite[sprite2].x + x, sprite[sprite2].y + y, &sect);

        if (sect != -1)
        {
            int32_t renderz, picnum;
            // XXX: PK: too large stack allocation for my taste
            int32_t i;
            int32_t pix_diff, newz;
            //                initprintf("drawing ror\n");

            if (level)
            {
                // renderz = sector[sprite[sprite2].sectnum].ceilingz;
                renderz = sprite[sprite2].z - (sprite[sprite2].yrepeat * tilesiz[sprite[sprite2].picnum].y<<1);
                picnum = sector[sprite[sprite2].sectnum].ceilingpicnum;
                sector[sprite[sprite2].sectnum].ceilingpicnum = 562;
                tilesiz[562].x = tilesiz[562].y = 0;

                pix_diff = klabs(z) >> 8;
                newz = - ((pix_diff / 128) + 1) * (128<<8);

                for (i = 0; i < numsectors; i++)
                {
                    SE40backupStat[i] = sector[i].ceilingstat;
                    SE40backupZ[i] = sector[i].ceilingz;
                    if (!bitmap_test(ror_protectedsectors, i) || sp->lotag == 41)
                    {
                        sector[i].ceilingstat = 1;
                        sector[i].ceilingz += newz;
                    }
                }
            }
            else
            {
                // renderz = sector[sprite[sprite2].sectnum].floorz;
                renderz = sprite[sprite2].z;
                picnum = sector[sprite[sprite2].sectnum].floorpicnum;
                sector[sprite[sprite2].sectnum].floorpicnum = 562;
                tilesiz[562].x = tilesiz[562].y = 0;

                pix_diff = klabs(z) >> 8;
                newz = ((pix_diff / 128) + 1) * (128<<8);

                for (i = 0; i < numsectors; i++)
                {
                    SE40backupStat[i] = sector[i].floorstat;
                    SE40backupZ[i] = sector[i].floorz;
                    if (!bitmap_test(ror_protectedsectors, i) || sp->lotag == 41)
                    {
                        sector[i].floorstat = 1;
                        sector[i].floorz = +newz;
                    }
                }
            }

#ifdef POLYMER
            if (videoGetRenderMode() == REND_POLYMER)
                polymer_setanimatesprites(G_DoSpriteAnimations, CAMERA(pos.x), CAMERA(pos.y), CAMERA(pos.z), fix16_to_int(CAMERA(q16ang)), smoothratio);
#endif
            renderDrawRoomsQ16(sprite[sprite2].x + x, sprite[sprite2].y + y,
                      z + renderz, CAMERA(q16ang), CAMERA(q16horiz), sect);
            drawing_ror = 1 + level;

            if (drawing_ror == 2) // viewing from top
                G_OROR_DupeSprites(sp);

            G_DoSpriteAnimations(CAMERA(pos.x),CAMERA(pos.y),CAMERA(pos.z),fix16_to_int(CAMERA(q16ang)),smoothratio);
            renderDrawMasks();

            if (level)
            {
                sector[sprite[sprite2].sectnum].ceilingpicnum = picnum;
                for (i = 0; i < numsectors; i++)
                {
                    sector[i].ceilingstat = SE40backupStat[i];
                    sector[i].ceilingz = SE40backupZ[i];
                }
            }
            else
            {
                sector[sprite[sprite2].sectnum].floorpicnum = picnum;

                for (i = 0; i < numsectors; i++)
                {
                    sector[i].floorstat = SE40backupStat[i];
                    sector[i].floorz = SE40backupZ[i];
                }
            }
        }
    }
}
#endif

void G_HandleMirror(int32_t x, int32_t y, int32_t z, fix16_t a, fix16_t q16horiz, int32_t smoothratio)
{
    MICROPROFILE_SCOPEI("Game", EDUKE32_FUNCTION, MP_YELLOWGREEN);

    if (bitmap_test(gotpic, MIRROR)
#ifdef POLYMER
        && (videoGetRenderMode() != REND_POLYMER)
#endif
        )
    {
        if (g_mirrorCount == 0)
        {
            // NOTE: We can have g_mirrorCount==0 but gotpic'd MIRROR,
            // for example in LNGA2.
            bitmap_clear(gotpic, MIRROR);

            //give scripts the chance to reset gotpics for effects that run in EVENT_DISPLAYROOMS
            //EVENT_RESETGOTPICS must be called after the last call to EVENT_DISPLAYROOMS in a frame, but before any engine-side renderDrawRoomsQ16
            VM_OnEvent(EVENT_RESETGOTPICS, -1, -1);
            return;
        }

        int32_t i = 0, dst = INT32_MAX;

        for (bssize_t k=g_mirrorCount-1; k>=0; k--)
        {
            if (!wallvisible(x, y, g_mirrorWall[k]))
                continue;

            const int32_t j =
                klabs(wall[g_mirrorWall[k]].x - x) +
                klabs(wall[g_mirrorWall[k]].y - y);

            if (j < dst)
                dst = j, i = k;
        }

        if (wall[g_mirrorWall[i]].overpicnum != MIRROR)
        {
            // Try to find a new mirror wall in case the original one was broken.

            int32_t startwall = sector[g_mirrorSector[i]].wallptr;
            int32_t endwall = startwall + sector[g_mirrorSector[i]].wallnum;

            for (bssize_t k=startwall; k<endwall; k++)
            {
                int32_t j = wall[k].nextwall;
                if (j >= 0 && (wall[j].cstat&32) && wall[j].overpicnum==MIRROR)  // cmp. premap.c
                {
                    g_mirrorWall[i] = j;
                    break;
                }
            }
        }

        if (wall[g_mirrorWall[i]].overpicnum == MIRROR)
        {
            int32_t tposx, tposy;
            fix16_t tang;

            //prepare to render any scripted EVENT_DISPLAYROOMS extras as mirrored
            renderPrepareMirror(x, y, z, a, q16horiz, g_mirrorWall[i], &tposx, &tposy, &tang);

            int32_t j = g_visibility;
            g_visibility = (j>>1) + (j>>2);

            //backup original camera position
            auto origCam = CAMERA(pos);
            fix16_t origCamq16ang   = CAMERA(q16ang);
            fix16_t origCamq16horiz = CAMERA(q16horiz);

            //set the camera inside the mirror facing out
            CAMERA(pos)      = { tposx, tposy, z };
            CAMERA(q16ang)   = tang;
            CAMERA(q16horiz) = q16horiz;

            display_mirror = 1;
            VM_OnEventWithReturn(EVENT_DISPLAYROOMS, g_player[0].ps->i, 0, 0);
            display_mirror = 0;

            //reset the camera position
            CAMERA(pos)      = origCam;
            CAMERA(q16ang)   = origCamq16ang;
            CAMERA(q16horiz) = origCamq16horiz;

            //give scripts the chance to reset gotpics for effects that run in EVENT_DISPLAYROOMS
            //EVENT_RESETGOTPICS must be called after the last call to EVENT_DISPLAYROOMS in a frame, but before any engine-side renderDrawRoomsQ16
            VM_OnEvent(EVENT_RESETGOTPICS, -1, -1);

            //prepare to render the mirror
            renderPrepareMirror(x, y, z, a, q16horiz, g_mirrorWall[i], &tposx, &tposy, &tang);

            if (videoGetRenderMode() != REND_POLYMER)
            {
#ifdef YAX_ENABLE
                yax_preparedrawrooms();
                auto const didmirror =
#endif
                    renderDrawRoomsQ16(tposx,tposy,z,tang,q16horiz,g_mirrorSector[i]+MAXSECTORS);
#ifdef YAX_ENABLE
                //POGO: if didmirror == 0, we may simply wish to abort instead of rendering with yax_drawrooms (which may require cleaning yax state)
                if (videoGetRenderMode() != REND_CLASSIC || didmirror)
                    yax_drawrooms(G_DoSpriteAnimations, g_mirrorSector[i], didmirror, smoothratio);
#endif
            }
#ifdef USE_OPENGL
            else
                renderDrawRoomsQ16(tposx,tposy,z,tang,q16horiz,g_mirrorSector[i]+MAXSECTORS);
            // XXX: Sprites don't get drawn with TROR/Polymost
#endif
            display_mirror = 1;
            G_DoSpriteAnimations(tposx,tposy,z,fix16_to_int(tang),smoothratio);
            display_mirror = 0;

            renderDrawMasks();
            renderCompleteMirror();   //Reverse screen x-wise in this function
            g_visibility = j;
        }
    }
}

static void G_ClearGotMirror()
{
#ifdef SPLITSCREEN_MOD_HACKS
    if (!g_fakeMultiMode)
#endif
    {
        // HACK for splitscreen mod: this is so that mirrors will be drawn
        // from showview commands. Ugly, because we'll attempt do draw mirrors
        // each frame then. But it's better than not drawing them, I guess.
        // XXX: fix the sequence of setting/clearing this bit. Right now,
        // we always draw one frame without drawing the mirror, after which
        // the bit gets set and drawn subsequently.
        bitmap_clear(gotpic, MIRROR);
    }
}

#ifdef USE_OPENGL
static void G_ReadGLFrame(void)
{
    MICROPROFILE_SCOPEI("Game", EDUKE32_FUNCTION, MP_YELLOWGREEN);

    // Save OpenGL screenshot with Duke3D palette
    // NOTE: maybe need to move this to the engine...
    auto frame = (palette_t *)Xaligned_alloc(16, xdim * ydim * sizeof(palette_t));
    char *const pic = (char *) waloff[TILE_SAVESHOT];

    Bassert(waloff[TILE_SAVESHOT]);

    int const xf = divscale16(ydim*4/3, 320);
    int const yf = divscale16(ydim, 200);  // (ydim<<16)/200

    videoBeginDrawing();
    glReadPixels(0, 0, xdim, ydim, GL_RGBA, GL_UNSIGNED_BYTE, frame);
    videoEndDrawing();

    for (int y = 0; y < 200; y++)
    {
        const int32_t base = mulscale16(200 - y - 1, yf)*xdim;

        for (int x = 0; x < 320; x++)
        {
            const palette_t *pix = &frame[base + mulscale16(x, xf) + (xdim-(ydim*4/3))/2];
            pic[320 * y + x] = paletteGetClosestColor(pix->r, pix->g, pix->b);
        }
    }

    Xaligned_free(frame);
}
#endif

void G_DrawRooms(int32_t playerNum, int32_t smoothRatio)
{
    MICROPROFILE_SCOPEI("Game", EDUKE32_FUNCTION, MP_YELLOWGREEN);

    auto const &thisPlayer = g_player[playerNum];
    auto const  pPlayer    = thisPlayer.ps;

    int const viewingRange = viewingrange;

    if (g_networkMode == NET_DEDICATED_SERVER) return;

    totalclocklock = totalclock;
    rotatespritesmoothratio = smoothRatio;

    if (pub > 0 || videoGetRenderMode() >= REND_POLYMOST) // JBF 20040101: redraw background always
    {
#ifndef EDUKE32_TOUCH_DEVICES
        if (ud.screen_size >= 8)
#endif
            G_DrawBackground();
        pub = 0;
    }

    VM_OnEvent(EVENT_DISPLAYSTART, pPlayer->i, playerNum);

    if ((ud.overhead_on == 2 && !automapping) || ud.show_help || (pPlayer->cursectnum == -1 && videoGetRenderMode() != REND_CLASSIC))
    {
        if (g_screenCapture && pPlayer->cursectnum == -1)
            LOG_F(ERROR, "Unable to capture screenshot for savegame because player cursectnum is -1!");
        return;
    }

    if (r_usenewaspect)
    {
        newaspect_enable = 1;
        videoSetCorrectedAspect();
    }

    if (pPlayer->on_crane > -1)
        smoothRatio = 65536;

    int const playerVis = pPlayer->visibility;
    g_visibility        = (playerVis <= 0) ? 0 : (int32_t)(playerVis * (numplayers > 1 ? 1.f : r_ambientlightrecip));

    CAMERA(sect) = pPlayer->cursectnum;

    G_DoInterpolations(smoothRatio);
    G_AnimateCamSprite(smoothRatio);
    G_DoConveyorInterp(smoothRatio);
    G_InterpolateLights(smoothRatio);

    if (g_screenCapture)
    {
        walock[TILE_SAVESHOT] = CACHE1D_PERMANENT;

        if (waloff[TILE_SAVESHOT] == 0)
        {
            g_cache.allocateBlock(&waloff[TILE_SAVESHOT],200*320,&walock[TILE_SAVESHOT]);
            tileSetSize(TILE_SAVESHOT, 200, 320);
        }

        if (videoGetRenderMode() == REND_CLASSIC)
            renderSetTarget(TILE_SAVESHOT, 200, 320);
    }

    if (ud.camerasprite >= 0)
    {
        auto const pSprite = &sprite[ud.camerasprite];

        pSprite->yvel = clamp(TrackerCast(pSprite->yvel), -100, 300);

        CAMERA(q16ang) = fix16_from_int(actor[ud.camerasprite].tempang
                                      + mulscale16(((pSprite->ang + 1024 - actor[ud.camerasprite].tempang) & 2047) - 1024, smoothRatio));

#ifdef USE_OPENGL
        renderSetRollAngle(0);
#endif

        int const noDraw = VM_OnEventWithReturn(EVENT_DISPLAYROOMSCAMERA, ud.camerasprite, playerNum, 0);

        if (noDraw != 1)  // event return values other than 0 and 1 are reserved
        {
#ifdef DEBUGGINGAIDS
            if (EDUKE32_PREDICT_FALSE(noDraw != 0))
                LOG_F(ERROR, "EVENT_DISPLAYROOMSCAMERA return value must be 0 or 1, all other values are reserved.");
#endif

#ifdef LEGACY_ROR
            G_SE40(smoothRatio);
#endif
#ifdef POLYMER
            if (videoGetRenderMode() == REND_POLYMER)
                polymer_setanimatesprites(G_DoSpriteAnimations, pSprite->x, pSprite->y, pSprite->z - ZOFFSET6, fix16_to_int(CAMERA(q16ang)), smoothRatio);
#endif
            yax_preparedrawrooms();
            renderDrawRoomsQ16(pSprite->x, pSprite->y, pSprite->z - ZOFFSET6, CAMERA(q16ang), fix16_from_int(pSprite->yvel), pSprite->sectnum);
            yax_drawrooms(G_DoSpriteAnimations, pSprite->sectnum, 0, smoothRatio);
            G_DoSpriteAnimations(pSprite->x, pSprite->y, pSprite->z - ZOFFSET6, fix16_to_int(CAMERA(q16ang)), smoothRatio);
            renderDrawMasks();
        }
    }
    else
    {
        int32_t floorZ, ceilZ;
        int32_t tiltcx, tiltcy, tiltcs=0;    // JBF 20030807

        int vr            = divscale22(1, sprite[pPlayer->i].yrepeat + 28);
        int screenTilting = (videoGetRenderMode() == REND_CLASSIC
                             && ((ud.screen_tilting && pPlayer->rotscrnang

#ifdef SPLITSCREEN_MOD_HACKS
                                  && !g_fakeMultiMode
#endif
                                  )));

        vr = Blrintf(float(vr) * tanf(ud.fov * (fPI/360.f)));

        if (!r_usenewaspect)
            renderSetAspect(vr, yxaspect);
        else
            renderSetAspect(mulscale16(vr, viewingrange), yxaspect);

        if (!g_screenCapture)
        {
            if (screenTilting)
            {
                int32_t oviewingrange = viewingrange;  // save it from renderSetAspect()
                const int16_t tang = (ud.screen_tilting) ? pPlayer->rotscrnang : 0;

                if (tang == 1024)
                    screenTilting = 2;
                else
                {
                    // Maximum possible allocation size passed to allocache() below
                    // since there is no equivalent of free() for allocache().
    #if MAXYDIM >= 640
                    int const maxTiltSize = 640*640;
    #else
                    int const maxTiltSize = 320*320;
    #endif
                    // To render a tilted screen in high quality, we need at least
                    // 640 pixels of *Y* dimension.
    #if MAXYDIM >= 640
                    // We also need
                    //  * xdim >= 640 since tiltcx will be passed as setview()'s x2
                    //    which must be less than xdim.
                    //  * ydim >= 640 (sic!) since the tile-to-draw-to will be set
                    //    up with dimension 400x640, but the engine's arrays like
                    //    lastx[] are alloc'd with *xdim* elements! (This point is
                    //    the dynamic counterpart of the #if above since we now
                    //    allocate these engine arrays tightly.)
                    // XXX: The engine should be in charge of setting up everything
                    // so that no oob access occur.
                    if (xdim >= 640 && ydim >= 640)
                    {
                        tiltcs = 2;
                        tiltcx = 640;
                        tiltcy = 400;
                    }
                    else
    #endif
                    {
                        // JBF 20030807: Increased tilted-screen quality
                        tiltcs = 1;

                        // NOTE: The same reflections as above apply here, too.
                        // TILT_SETVIEWTOTILE_320.
                        tiltcx = 320;
                        tiltcy = 200;
                    }

                    // If the view is rotated (not 0 or 180 degrees modulo 360 degrees),
                    // we render onto a square tile and display a portion of that
                    // rotated on-screen later on.
                    const int32_t viewtilexsiz = (tang&1023) ? tiltcx : tiltcy;
                    const int32_t viewtileysiz = tiltcx;

                    walock[TILE_TILT] = CACHE1D_PERMANENT;
                    if (waloff[TILE_TILT] == 0)
                        g_cache.allocateBlock(&waloff[TILE_TILT], maxTiltSize, &walock[TILE_TILT]);

                    renderSetTarget(TILE_TILT, viewtilexsiz, viewtileysiz);

                    if ((tang&1023) == 512)
                    {
                        //Block off unscreen section of 90ø tilted screen
                        int const j = tiltcx-(60*tiltcs);
                        for (bssize_t i=(60*tiltcs)-1; i>=0; i--)
                        {
                            startumost[i] = 1;
                            startumost[i+j] = 1;
                            startdmost[i] = 0;
                            startdmost[i+j] = 0;
                        }
                    }

                    int vRange = (tang & 511);

                    if (vRange > 256)
                        vRange = 512 - vRange;

                    vRange = sintable[vRange + 512] * 8 + sintable[vRange] * 5;
                    renderSetAspect(mulscale16(oviewingrange, vRange >> 1), yxaspect);
                }
            }
    #ifdef USE_OPENGL
            else if (videoGetRenderMode() >= REND_POLYMOST)
            {
                if (ud.screen_tilting
    #ifdef SPLITSCREEN_MOD_HACKS
                    && !g_fakeMultiMode
    #endif
                )
                {
                    renderSetRollAngle(pPlayer->orotscrnang + mulscale16(((pPlayer->rotscrnang - pPlayer->orotscrnang + 1024)&2047)-1024, smoothRatio));
                }
                else
                {
                    renderSetRollAngle(0);
                }
            }
    #endif
        }

        if (pPlayer->newowner < 0)
        {
            CAMERA(pos) = { pPlayer->pos.x - mulscale16(65536-smoothRatio, pPlayer->pos.x - pPlayer->opos.x),
                            pPlayer->pos.y - mulscale16(65536-smoothRatio, pPlayer->pos.y - pPlayer->opos.y),
                            pPlayer->pos.z - mulscale16(65536-smoothRatio, pPlayer->pos.z - pPlayer->opos.z) };

            if (thisPlayer.smoothcamera)
            {
                CAMERA(q16ang)   = pPlayer->oq16ang
                                 + mulscale16(((pPlayer->q16ang + F16(1024) - pPlayer->oq16ang) & 0x7FFFFFF) - F16(1024), smoothRatio);
                CAMERA(q16horiz) = pPlayer->oq16horiz + pPlayer->oq16horizoff
                                 + mulscale16((pPlayer->q16horiz + pPlayer->q16horizoff - pPlayer->oq16horiz - pPlayer->oq16horizoff), smoothRatio);
            }
            else
            {
                CAMERA(q16ang)   = pPlayer->q16ang;
                CAMERA(q16horiz) = pPlayer->q16horiz + pPlayer->q16horizoff;
            }

            CAMERA(q16ang) += fix16_from_int(pPlayer->olook_ang)
                            + mulscale16(fix16_from_int(((pPlayer->look_ang + 1024 - pPlayer->olook_ang) & 2047) - 1024), smoothRatio);

            if (ud.viewbob)
            {
                int zAdd = (pPlayer->opyoff + mulscale16(pPlayer->pyoff-pPlayer->opyoff, smoothRatio));

                if (pPlayer->over_shoulder_on)
                    zAdd >>= 3;

                CAMERA(pos.z) += zAdd;
            }

            if (pPlayer->over_shoulder_on)
            {
                CAMERA(pos.z) -= 3072;

                if (G_DoThirdPerson(pPlayer, &CAMERA(pos), &CAMERA(sect), fix16_to_int(CAMERA(q16ang)), fix16_to_int(CAMERA(q16horiz))) < 0)
                {
                    CAMERA(pos.z) += 3072;
                    G_DoThirdPerson(pPlayer, &CAMERA(pos), &CAMERA(sect), fix16_to_int(CAMERA(q16ang)), fix16_to_int(CAMERA(q16horiz)));
                }
            }
        }
        else
        {
            vec3_t const camVect = G_GetCameraPosition(pPlayer->newowner, smoothRatio);

            // looking through viewscreen
            CAMERA(pos)      = camVect;
            CAMERA(q16ang)   = pPlayer->q16ang
                                + mulscale16(((fix16_from_int(sprite[pPlayer->newowner].ang) + F16(1024) - pPlayer->q16ang) & 0x7FFFFFF) - F16(1024), smoothRatio)
                                + fix16_from_int(pPlayer->look_ang);
            CAMERA(q16horiz) = fix16_from_int(100 + sprite[pPlayer->newowner].shade);
            CAMERA(sect)     = sprite[pPlayer->newowner].sectnum;
        }

        ceilZ  = actor[pPlayer->i].ceilingz;
        floorZ = actor[pPlayer->i].floorz;

        if (g_earthquakeTime > 0 && pPlayer->on_ground == 1)
        {
            CAMERA(pos.z) += 256 - (((g_earthquakeTime)&1) << 9);
            CAMERA(q16ang)   += fix16_from_int((2 - ((g_earthquakeTime)&2)) << 2);

            I_AddForceFeedback(g_earthquakeTime << FF_WEAPON_DMG_SCALE, g_earthquakeTime << FF_WEAPON_DMG_SCALE, g_earthquakeTime << FF_WEAPON_TIME_SCALE);
        }

        if (sprite[pPlayer->i].pal == 1)
            CAMERA(pos.z) -= ZOFFSET7;

        if (pPlayer->newowner < 0 && pPlayer->spritebridge == 0)
        {
            // NOTE: when shrunk, p->pos.z can be below the floor.  This puts the
            // camera into the sector again then.

            if (CAMERA(pos.z) < (pPlayer->truecz + ZOFFSET6))
                CAMERA(pos.z) = ceilZ + ZOFFSET6;
            else if (CAMERA(pos.z) > (pPlayer->truefz - ZOFFSET6))
                CAMERA(pos.z) = floorZ - ZOFFSET6;
        }

        while (CAMERA(sect) >= 0)  // if, really
        {
            getzsofslope(CAMERA(sect),CAMERA(pos.x),CAMERA(pos.y),&ceilZ,&floorZ);
#ifdef YAX_ENABLE
            if (yax_getbunch(CAMERA(sect), YAX_CEILING) >= 0)
            {
                if (CAMERA(pos.z) < ceilZ)
                {
                    updatesectorz(CAMERA(pos.x), CAMERA(pos.y), CAMERA(pos.z), &CAMERA(sect));
                    break;  // since CAMERA(sect) might have been updated to -1
                    // NOTE: fist discovered in WGR2 SVN r134, til' death level 1
                    //  (Lochwood Hollow).  A problem REMAINS with Polymost, maybe classic!
                }
            }
            else
#endif
                if (CAMERA(pos.z) < ceilZ+ZOFFSET6)
                    CAMERA(pos.z) = ceilZ+ZOFFSET6;

#ifdef YAX_ENABLE
            if (yax_getbunch(CAMERA(sect), YAX_FLOOR) >= 0)
            {
                if (CAMERA(pos.z) > floorZ)
                    updatesectorz(CAMERA(pos.x), CAMERA(pos.y), CAMERA(pos.z), &CAMERA(sect));
            }
            else
#endif
                if (CAMERA(pos.z) > floorZ-ZOFFSET6)
                    CAMERA(pos.z) = floorZ-ZOFFSET6;

            break;
        }

        // NOTE: might be rendering off-screen here, so CON commands that draw stuff
        //  like showview must cope with that situation or bail out!
        int const noDraw = VM_OnEventWithReturn(EVENT_DISPLAYROOMS, pPlayer->i, playerNum, 0);

        CAMERA(q16horiz) = fix16_clamp(CAMERA(q16horiz), F16(HORIZ_MIN), F16(HORIZ_MAX));

        if (noDraw != 1)  // event return values other than 0 and 1 are reserved
        {
#ifdef DEBUGGINGAIDS
            if (EDUKE32_PREDICT_FALSE(noDraw != 0))
                LOG_F(ERROR, "EVENT_DISPLAYROOMS return value must be 0 or 1, all other values are reserved.");
#endif

            G_HandleMirror(CAMERA(pos.x), CAMERA(pos.y), CAMERA(pos.z), CAMERA(q16ang), CAMERA(q16horiz), smoothRatio);
            G_ClearGotMirror();
#ifdef LEGACY_ROR
            G_SE40(smoothRatio);
#endif
#ifdef POLYMER
            if (videoGetRenderMode() == REND_POLYMER)
                polymer_setanimatesprites(G_DoSpriteAnimations, CAMERA(pos.x),CAMERA(pos.y),CAMERA(pos.z),fix16_to_int(CAMERA(q16ang)),smoothRatio);
#endif
            // for G_PrintCoords
            dr_viewingrange = viewingrange;
            dr_yxaspect = yxaspect;
#ifdef DEBUG_MIRRORS_ONLY
            bitmap_set(gotpic, MIRROR);
#else
            yax_preparedrawrooms();
            renderDrawRoomsQ16(CAMERA(pos.x),CAMERA(pos.y),CAMERA(pos.z),CAMERA(q16ang),CAMERA(q16horiz),CAMERA(sect));
            yax_drawrooms(G_DoSpriteAnimations, CAMERA(sect), 0, smoothRatio);
#ifdef LEGACY_ROR
            if ((unsigned)ror_sprite < MAXSPRITES && drawing_ror == 1)  // viewing from bottom
                G_OROR_DupeSprites(&sprite[ror_sprite]);
#endif
            G_DoSpriteAnimations(CAMERA(pos.x),CAMERA(pos.y),CAMERA(pos.z),fix16_to_int(CAMERA(q16ang)),smoothRatio);
#ifdef LEGACY_ROR
            drawing_ror = 0;
#endif
            renderDrawMasks();
#endif
        }

        if (g_screenCapture)
        {
            if (noDraw)
                LOG_F(ERROR, "Unable to capture screenshot for savegame because EVENT_DISPLAYROOMS returned %d!", noDraw);

            g_screenCapture = 0;

            if (videoGetRenderMode() == REND_CLASSIC)
                renderRestoreTarget();
#ifdef USE_OPENGL
            else
            {
                G_ReadGLFrame();
                tileInvalidate(TILE_SAVESHOT, 0, 255);
            }
#endif
        }
        else if (screenTilting)
        {
            const int16_t tang = (ud.screen_tilting) ? pPlayer->rotscrnang : 0;

            if (screenTilting == 2)  // tang == 1024
            {
                videoBeginDrawing();
                {
                    const int32_t height = windowxy2.y-windowxy1.y+1;
                    const int32_t width = windowxy2.x-windowxy1.x+1;

                    uint8_t *f = (uint8_t *)(frameplace + ylookup[windowxy1.y]);
                    int32_t x, y;

                    for (y=0; y < (height>>1); y++)
                        swapbufreverse(f + y*bytesperline + windowxy2.x,
                                       f + (height-1-y)*bytesperline + windowxy1.x,
                                       width);

                    f += (height>>1)*bytesperline + windowxy1.x;

                    if (height&1)
                        for (x=0; x<(width>>1); x++)
                            swapchar(&f[x], &f[width-1-x]);
                }
                videoEndDrawing();
            }
            else
            {
                renderRestoreTarget();
                picanm[TILE_TILT].xofs = picanm[TILE_TILT].yofs = 0;

                int tiltZoom = (tang&511);

                if (tiltZoom > 256)
                    tiltZoom = 512 - tiltZoom;

                tiltZoom = sintable[tiltZoom + 512] * 8 + sintable[tiltZoom] * 5;
                tiltZoom >>= tiltcs;  // JBF 20030807

                rotatesprite_win(160 << 16, 100 << 16, tiltZoom, tang + 512, TILE_TILT, 0, 0, 4 + 2 + 64 + 1024);
                walock[TILE_TILT] = CACHE1D_FREE;
            }
        }
    }

    G_RestoreInterpolations();
    G_ResetConveyorInterp();

    {
        // Totalclock count of last step of p->visibility converging towards
        // ud.const_visibility.
        static int32_t lastvist;
        const int32_t visdif = ud.const_visibility-pPlayer->visibility;

        // Check if totalclock was cleared (e.g. restarted game).
        if (totalclock < lastvist)
            lastvist = 0;

        // Every 2nd totalclock increment (each 1/60th second), ...
        while (totalclock >= lastvist+2)
        {
            // ... approximately three-quarter the difference between
            // p->visibility and ud.const_visibility.
            const int32_t visinc = visdif>>2;

            if (klabs(visinc) == 0)
            {
                pPlayer->visibility = ud.const_visibility;
                break;
            }

            pPlayer->visibility += visinc;
            lastvist = (int32_t) totalclock;
        }
    }

    if (r_usenewaspect)
    {
        newaspect_enable = 0;
        renderSetAspect(viewingRange, tabledivide32_noinline(65536 * ydim * 8, xdim * 5));
    }

    VM_OnEvent(EVENT_DISPLAYROOMSEND, g_player[screenpeek].ps->i, screenpeek);
}

void G_DumpDebugInfo(void)
{
    static char const s_WEAPON[] = "WEAPON";
    int32_t i,j,x;
    //    buildvfs_FILE fp = buildvfs_fopen_write("condebug.log");

    VM_ScriptInfo(insptr, 64);
    LOG_F(INFO, "Current gamevar values:");

    for (i=0; i<MAX_WEAPONS; i++)
    {
        for (j=0; j<numplayers; j++)
        {
            LOG_F(INFO, "Player %d", j);
            buildprint(s_WEAPON, i, "_CLIP ", PWEAPON(j, i, Clip));
            buildprint(s_WEAPON, i, "_RELOAD ", PWEAPON(j, i, Reload));
            buildprint(s_WEAPON, i, "_FIREDELAY ", PWEAPON(j, i, FireDelay));
            buildprint(s_WEAPON, i, "_TOTALTIME ", PWEAPON(j, i, TotalTime));
            buildprint(s_WEAPON, i, "_HOLDDELAY ", PWEAPON(j, i, HoldDelay));
            buildprint(s_WEAPON, i, "_FLAGS ", PWEAPON(j, i, Flags));
            buildprint(s_WEAPON, i, "_SHOOTS ", PWEAPON(j, i, Shoots));
            buildprint(s_WEAPON, i, "_SPAWNTIME ", PWEAPON(j, i, SpawnTime));
            buildprint(s_WEAPON, i, "_SPAWN ", PWEAPON(j, i, Spawn));
            buildprint(s_WEAPON, i, "_SHOTSPERBURST ", PWEAPON(j, i, ShotsPerBurst));
            buildprint(s_WEAPON, i, "_WORKSLIKE ", PWEAPON(j, i, WorksLike));
            buildprint(s_WEAPON, i, "_INITIALSOUND ", PWEAPON(j, i, InitialSound));
            buildprint(s_WEAPON, i, "_FIRESOUND ", PWEAPON(j, i, FireSound));
            buildprint(s_WEAPON, i, "_SOUND2TIME ", PWEAPON(j, i, Sound2Time));
            buildprint(s_WEAPON, i, "_SOUND2SOUND ", PWEAPON(j, i, Sound2Sound));
            buildprint(s_WEAPON, i, "_RELOADSOUND1 ", PWEAPON(j, i, ReloadSound1));
            buildprint(s_WEAPON, i, "_RELOADSOUND2 ", PWEAPON(j, i, ReloadSound2));
            buildprint(s_WEAPON, i, "_SELECTSOUND ", PWEAPON(j, i, SelectSound));
            buildprint(s_WEAPON, i, "_FLASHCOLOR ", PWEAPON(j, i, FlashColor));
        }
    }

    for (x=0; x<MAXSTATUS; x++)
    {
        j = headspritestat[x];
        while (j >= 0)
        {
            buildprint("Sprite ", j, " (", TrackerCast(sprite[j].x), ",", TrackerCast(sprite[j].y), ",", TrackerCast(sprite[j].z),
                ") (picnum: ", TrackerCast(sprite[j].picnum), ")");
            for (i=0; i<g_gameVarCount; i++)
            {
                if (aGameVars[i].flags & (GAMEVAR_PERACTOR))
                {
                    if (aGameVars[i].pValues[j] != aGameVars[i].defaultValue)
                    {
                        buildprint("gamevar ", aGameVars[i].szLabel, " ", aGameVars[i].pValues[j], " GAMEVAR_PERACTOR");
                        if (aGameVars[i].flags != GAMEVAR_PERACTOR)
                        {
                            buildprint(" // ");
                            if (aGameVars[i].flags & (GAMEVAR_SYSTEM))
                            {
                                buildprint(" (system)");
                            }
                        }
                    }
                }
            }
            j = nextspritestat[j];
        }
    }
    Gv_DumpValues();
//    buildvfs_fclose(fp);
    saveboard("debug.map", &g_player[myconnectindex].ps->pos, fix16_to_int(g_player[myconnectindex].ps->q16ang),
              g_player[myconnectindex].ps->cursectnum);
}

// if <set_movflag_uncond> is true, set the moveflag unconditionally,
// else only if it equals 0.
static int32_t G_InitActor(int32_t i, uint16_t tilenum, int32_t set_movflag_uncond)
{
    if (g_tile[tilenum].execPtr)
    {
        SH(i) = g_tile[tilenum].execPtr[0];
        AC_ACTION_ID(actor[i].t_data) = g_tile[tilenum].execPtr[1];
        AC_MOVE_ID(actor[i].t_data) = g_tile[tilenum].execPtr[2];

        if (set_movflag_uncond || SHT(i) == 0)  // AC_MOVFLAGS
            SHT(i) = g_tile[tilenum].execPtr[3];

        return 1;
    }

    return 0;
}

int32_t A_InsertSprite(int16_t whatsect,int32_t s_x,int32_t s_y,int32_t s_z, uint16_t s_pn,int8_t s_s,
                       uint8_t s_xr,uint8_t s_yr,int16_t s_a,int16_t s_ve,int16_t s_zv,int16_t s_ow,int16_t s_ss)
{


    int32_t newSprite;

#ifdef NETCODE_DISABLE
    newSprite = insertsprite(whatsect, s_ss);
#else
    newSprite = Net_InsertSprite(whatsect, s_ss);

#endif

    if (EDUKE32_PREDICT_FALSE((unsigned)newSprite >= MAXSPRITES))
    {
        G_DumpDebugInfo();
        LOG_F(ERROR, "Failed spawning pic %d spr from pic %d spr %d at x:%d,y:%d,z:%d,sect:%d",
                          s_pn,s_ow < 0 ? -1 : TrackerCast(sprite[s_ow].picnum),s_ow,s_x,s_y,s_z,whatsect);
        LOG_F(ERROR, "Too many sprites spawned.");
        fatal_exit("Too many sprites spawned.");
    }

#if 1//def DEBUGGINGAIDS
    g_spriteStat.numins++;
#endif

    sprite[newSprite] = { s_x, s_y, s_z, 0, s_pn, s_s, 0, 0, 0, s_xr, s_yr, 0, 0, whatsect, s_ss, s_a, s_ow, s_ve, 0, s_zv, 0, 0, 0 };

    auto &a = actor[newSprite];
    a = {};
    a.bpos = { s_x, s_y, s_z };

    if ((unsigned)s_ow < MAXSPRITES)
    {
        a.htpicnum = sprite[s_ow].picnum;
        a.floorz   = actor[s_ow].floorz;
        a.ceilingz = actor[s_ow].ceilingz;
    }

    a.stayput = -1;
    a.htextra = -1;
    a.htowner = s_ow;

#ifdef POLYMER
    practor[newSprite].lightId = -1;
#endif

    G_InitActor(newSprite, s_pn, 1);

    spriteext[newSprite]    = {};
    spritesmooth[newSprite] = {};

    A_ResetVars(newSprite);

    if (VM_HaveEvent(EVENT_EGS))
    {
        int32_t p, pl = A_FindPlayer(&sprite[newSprite], &p);

        block_deletesprite++;
        VM_ExecuteEvent(EVENT_EGS, newSprite, pl, p);
        block_deletesprite--;
    }

    return newSprite;
}

#ifdef YAX_ENABLE
void Yax_SetBunchZs(int32_t sectnum, int32_t cf, int32_t daz)
{
    int32_t i, bunchnum = yax_getbunch(sectnum, cf);

    if (bunchnum < 0 || bunchnum >= numyaxbunches)
        return;

    for (SECTORS_OF_BUNCH(bunchnum, YAX_CEILING, i))
        SECTORFLD(i,z, YAX_CEILING) = daz;
    for (SECTORS_OF_BUNCH(bunchnum, YAX_FLOOR, i))
        SECTORFLD(i,z, YAX_FLOOR) = daz;
}

static void Yax_SetBunchInterpolation(int32_t sectnum, int32_t cf)
{
    int32_t i, bunchnum = yax_getbunch(sectnum, cf);

    if (bunchnum < 0 || bunchnum >= numyaxbunches)
        return;

    for (SECTORS_OF_BUNCH(bunchnum, YAX_CEILING, i))
        G_SetInterpolation(&sector[i].ceilingz);
    for (SECTORS_OF_BUNCH(bunchnum, YAX_FLOOR, i))
        G_SetInterpolation(&sector[i].floorz);
}
#else
# define Yax_SetBunchInterpolation(sectnum, cf)
#endif

// A_Spawn has two forms with arguments having different meaning:
//
// 1. spriteNum>=0: Spawn from parent sprite <spriteNum> with picnum <tileNum>
// 2. spriteNum<0: Spawn from already *existing* sprite <tileNum>
int A_Spawn(int spriteNum, uint16_t tileNum)
{
    int         newSprite;
    spritetype *pSprite;
    actor_t *   pActor;
    int         sectNum;

    if (spriteNum >= 0)
    {
        // spawn from parent sprite <j>
        newSprite = A_InsertSprite(sprite[spriteNum].sectnum,sprite[spriteNum].x,sprite[spriteNum].y,sprite[spriteNum].z,
                           tileNum,0,0,0,0,0,0,spriteNum,0);
        actor[newSprite].htpicnum = sprite[spriteNum].picnum;
    }
    else
    {
        // spawn from already existing sprite <pn>
        newSprite = tileNum;
        auto &s = sprite[newSprite];
        auto &a = actor[newSprite];

        a = { };
        a.bpos = { s.x, s.y, s.z };

        a.htpicnum = s.picnum;

        if (s.picnum == SECTOREFFECTOR && s.lotag == 50)
            a.htpicnum = s.owner;

        if (s.picnum == LOCATORS && s.owner != -1)
            a.htowner = s.owner;
        else
            s.owner = a.htowner = newSprite;

        a.floorz   = sector[s.sectnum].floorz;
        a.ceilingz = sector[s.sectnum].ceilingz;
        a.stayput = a.htextra = -1;

#ifdef POLYMER
        practor[newSprite].lightId = -1;
#endif

        if ((s.cstat & CSTAT_SPRITE_ALIGNMENT)
#ifndef EDUKE32_STANDALONE
            && s.picnum != SPEAKER && s.picnum != LETTER && s.picnum != DUCK && s.picnum != TARGET && s.picnum != TRIPBOMB
#endif
            && s.picnum != VIEWSCREEN && s.picnum != VIEWSCREEN2 && (!(s.picnum >= CRACK1 && s.picnum <= CRACK4)))
        {
            if (s.shade == 127)
                goto SPAWN_END;

#ifndef EDUKE32_STANDALONE
            if (A_CheckSwitchTile(newSprite) && (s.cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_WALL)
            {
                if (s.pal && s.picnum != ACCESSSWITCH && s.picnum != ACCESSSWITCH2)
                {
                    if (((!g_netServer && ud.multimode < 2)) || ((g_netServer || ud.multimode > 1) && !GTFLAGS(GAMETYPE_DMSWITCHES)))
                    {
                        s.xrepeat = s.yrepeat = 0;
                        s.lotag = s.hitag = 0;
                        s.cstat = 32768;
                        goto SPAWN_END;
                    }
                }

                s.cstat |= 257;

                if (s.pal && s.picnum != ACCESSSWITCH && s.picnum != ACCESSSWITCH2)
                    s.pal = 0;

                goto SPAWN_END;
            }
#endif

            if (s.hitag)
            {
                changespritestat(newSprite, STAT_FALLER);
                s.cstat |= 257;
                s.extra = g_impactDamage;
                goto SPAWN_END;
            }
        }

        if (s.cstat & 1)
            s.cstat |= 256;

        if (!G_InitActor(newSprite, s.picnum, 0))
            T2(newSprite) = T5(newSprite) = 0;  // AC_MOVE_ID, AC_ACTION_ID
        else
        {
            A_GetZLimits(newSprite);
            actor[newSprite].bpos = sprite[newSprite].xyz;
        }
    }

    pSprite = &sprite[newSprite];
    pActor  = &actor[newSprite];
    sectNum = pSprite->sectnum;

    //some special cases that can't be handled through the dynamictostatic system.

    if (pSprite->picnum >= CAMERA1 && pSprite->picnum <= CAMERA1 + 4)
        pSprite->picnum = CAMERA1;
#ifndef EDUKE32_STANDALONE
    else if (pSprite->picnum >= BOLT1 && pSprite->picnum <= BOLT1 + 3)
        pSprite->picnum = BOLT1;
    else if (pSprite->picnum >= SIDEBOLT1 && pSprite->picnum <= SIDEBOLT1 + 3)
        pSprite->picnum = SIDEBOLT1;
#endif
        switch (tileGetMapping(pSprite->picnum))
        {
        case FOF__:
            pSprite->xrepeat = pSprite->yrepeat = 0;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;
        case CAMERA1__:
            pSprite->extra = 1;
            pSprite->cstat &= 32768;

            if (g_damageCameras)
                pSprite->cstat |= 257;

            if ((!g_netServer && ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else
            {
                pSprite->pal = 0;
                changespritestat(newSprite, STAT_ACTOR);
            }
            goto SPAWN_END;
#ifndef EDUKE32_STANDALONE
        case CAMERAPOLE__:
            pSprite->extra = 1;
            pSprite->cstat &= 32768;

            if (g_damageCameras)
                pSprite->cstat |= 257;
            fallthrough__;
        case GENERICPOLE__:
            if ((!g_netServer && ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else
                pSprite->pal = 0;
            goto SPAWN_END;

        case BOLT1__:
        case SIDEBOLT1__:
            T1(newSprite) = pSprite->xrepeat;
            T2(newSprite) = pSprite->yrepeat;
            pSprite->yvel = 0;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case WATERSPLASH2__:
            if (spriteNum >= 0)
            {
                setsprite(newSprite, &sprite[spriteNum].xyz);
                pSprite->xrepeat = pSprite->yrepeat = 8+(krand()&7);
            }
            else pSprite->xrepeat = pSprite->yrepeat = 16+(krand()&15);

            pSprite->shade = -16;
            pSprite->cstat |= 128;

            if (spriteNum >= 0)
            {
                if (sector[sprite[spriteNum].sectnum].lotag == ST_2_UNDERWATER)
                {
                    pSprite->z = getceilzofslope(sectNum, pSprite->x, pSprite->y) + (16 << 8);
                    pSprite->cstat |= 8;
                }
                else if (sector[sprite[spriteNum].sectnum].lotag == ST_1_ABOVE_WATER)
                    pSprite->z = getflorzofslope(sectNum, pSprite->x, pSprite->y);
            }

            if (sector[sectNum].floorpicnum == FLOORSLIME || sector[sectNum].ceilingpicnum == FLOORSLIME)
                pSprite->pal = 7;
            fallthrough__;
        case DOMELITE__:
            if (pSprite->picnum == DOMELITE)
                pSprite->cstat |= 257;
            fallthrough__;
        case NEON1__:
        case NEON2__:
        case NEON3__:
        case NEON4__:
        case NEON5__:
        case NEON6__:
            if (pSprite->picnum != WATERSPLASH2)
                pSprite->cstat |= 257;
            fallthrough__;
        case NUKEBUTTON__:
        case JIBS1__:
        case JIBS2__:
        case JIBS3__:
        case JIBS4__:
        case JIBS5__:
        case JIBS6__:
        case HEADJIB1__:
        case ARMJIB1__:
        case LEGJIB1__:
        case LIZMANHEAD1__:
        case LIZMANARM1__:
        case LIZMANLEG1__:
        case DUKETORSO__:
        case DUKEGUN__:
        case DUKELEG__:
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;
        case TONGUE__:
            if (spriteNum >= 0)
                pSprite->ang = sprite[spriteNum].ang;
            pSprite->z -= 38<<8;
            pSprite->zvel = 256-(krand()&511);
            pSprite->xvel = 64-(krand()&127);
            changespritestat(newSprite, STAT_PROJECTILE);
            goto SPAWN_END;
        case NATURALLIGHTNING__:
            pSprite->cstat &= ~257;
            pSprite->cstat |= 32768;
            goto SPAWN_END;
        case TRANSPORTERSTAR__:
        case TRANSPORTERBEAM__:
            if (spriteNum == -1)
                goto SPAWN_END;
            if (pSprite->picnum == TRANSPORTERBEAM)
            {
                pSprite->xrepeat = 31;
                pSprite->yrepeat = 1;
                pSprite->z = sector[sprite[spriteNum].sectnum].floorz-PHEIGHT;
            }
            else
            {
                if (sprite[spriteNum].statnum == STAT_PROJECTILE)
                    pSprite->xrepeat = pSprite->yrepeat = 8;
                else
                {
                    pSprite->xrepeat = 48;
                    pSprite->yrepeat = 64;
                    if (sprite[spriteNum].statnum == STAT_PLAYER || A_CheckEnemySprite(&sprite[spriteNum]))
                        pSprite->z -= ZOFFSET5;
                }
            }

            pSprite->shade = -127;
            pSprite->cstat = 128|2;
            pSprite->ang = sprite[spriteNum].ang;

            pSprite->xvel = 128;
            changespritestat(newSprite, STAT_MISC);
            A_SetSprite(newSprite,CLIPMASK0);
            setsprite(newSprite,&pSprite->xyz);
            goto SPAWN_END;
        case FEMMAG1__:
        case FEMMAG2__:
            pSprite->cstat &= ~257;
            changespritestat(newSprite, STAT_DEFAULT);
            goto SPAWN_END;
        case DUKETAG__:
        case SIGN1__:
        case SIGN2__:
            if ((!g_netServer && ud.multimode < 2) && pSprite->pal)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else pSprite->pal = 0;
            goto SPAWN_END;

        case MASKWALL1__:
        case MASKWALL2__:
        case MASKWALL3__:
        case MASKWALL4__:
        case MASKWALL5__:
        case MASKWALL6__:
        case MASKWALL7__:
        case MASKWALL8__:
        case MASKWALL9__:
        case MASKWALL10__:
        case MASKWALL11__:
        case MASKWALL12__:
        case MASKWALL13__:
        case MASKWALL14__:
        case MASKWALL15__:
        {
            int const j    = pSprite->cstat & SPAWN_PROTECT_CSTAT_MASK;
            pSprite->cstat = j | CSTAT_SPRITE_BLOCK;
            changespritestat(newSprite, STAT_DEFAULT);
            goto SPAWN_END;
        }

        case PODFEM1__:
            pSprite->extra <<= 1;
            fallthrough__;
        case FEM1__:
        case FEM2__:
        case FEM3__:
        case FEM4__:
        case FEM5__:
        case FEM6__:
        case FEM7__:
        case FEM8__:
        case FEM9__:
        case FEM10__:
        case NAKED1__:
        case STATUE__:
        case TOUGHGAL__:
            pSprite->yvel  = pSprite->hitag;
            pSprite->hitag = -1;
            fallthrough__;
        case BLOODYPOLE__:
            pSprite->cstat   |= 257;
            pSprite->clipdist = 32;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case QUEBALL__:
        case STRIPEBALL__:
            pSprite->cstat    = 256;
            pSprite->clipdist = 8;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case DUKELYINGDEAD__:
            if (spriteNum >= 0 && sprite[spriteNum].picnum == APLAYER)
            {
                pSprite->xrepeat = sprite[spriteNum].xrepeat;
                pSprite->yrepeat = sprite[spriteNum].yrepeat;
                pSprite->shade   = sprite[spriteNum].shade;
                pSprite->pal     = g_player[P_Get(spriteNum)].ps->palookup;
            }
            fallthrough__;
        case DUKECAR__:
        case HELECOPT__:
            //                if(sp->picnum == HELECOPT || sp->picnum == DUKECAR) sp->xvel = 1024;
            pSprite->cstat = 0;
            pSprite->extra = 1;
            pSprite->xvel  = 292;
            pSprite->zvel  = 360;
            fallthrough__;
        case BLIMP__:
            pSprite->cstat   |= 257;
            pSprite->clipdist = 128;
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case RESPAWNMARKERRED__:
            pSprite->xrepeat = pSprite->yrepeat = 24;
            if (spriteNum >= 0)
                pSprite->z = actor[spriteNum].floorz;  // -(1<<4);
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case MIKE__:
            pSprite->yvel  = pSprite->hitag;
            pSprite->hitag = 0;
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;
        case WEATHERWARN__:
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case SPOTLITE__:
            T1(newSprite) = pSprite->x;
            T2(newSprite) = pSprite->y;
            goto SPAWN_END;
        case BULLETHOLE__:
            pSprite->xrepeat = 3;
            pSprite->yrepeat = 3;
            pSprite->cstat   = 16 + (krand() & 12);

            A_AddToDeleteQueue(newSprite);
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case MONEY__:
        case MAIL__:
        case PAPER__:
            pActor->t_data[0] = krand() & 2047;

            pSprite->cstat   = krand() & 12;
            pSprite->xrepeat = 8;
            pSprite->yrepeat = 8;
            pSprite->ang     = krand() & 2047;

            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case SHELL__: //From the player
        case SHOTGUNSHELL__:
            if (spriteNum >= 0)
            {
                int shellAng;

                if (sprite[spriteNum].picnum == APLAYER)
                {
                    int const  playerNum = P_Get(spriteNum);
                    auto const pPlayer   = g_player[playerNum].ps;

                    shellAng = fix16_to_int(pPlayer->q16ang) - (krand() & 63) + 8;  // Fine tune

                    T1(newSprite) = krand() & 1;

                    pSprite->z = (3 << 8) + pPlayer->pyoff + pPlayer->pos.z - (fix16_to_int((pPlayer->q16horizoff + pPlayer->q16horiz - F16(100))) << 4);

                    if (pSprite->picnum == SHOTGUNSHELL)
                        pSprite->z += (3 << 8);

                    pSprite->zvel = -(krand() & 255);
                }
                else
                {
                    shellAng          = pSprite->ang;
                    pSprite->z = sprite[spriteNum].z - PHEIGHT + (3 << 8);
                }

                pSprite->x     = sprite[spriteNum].x + (sintable[(shellAng + 512) & 2047] >> 7);
                pSprite->y     = sprite[spriteNum].y + (sintable[shellAng & 2047] >> 7);
                pSprite->shade = -8;

                if (pSprite->yvel == 1 || NAM_WW2GI)
                {
                    pSprite->ang  = shellAng + 512;
                    pSprite->xvel = 30;
                }
                else
                {
                    pSprite->ang  = shellAng - 512;
                    pSprite->xvel = 20;
                }

                pSprite->xrepeat = pSprite->yrepeat = 4;

                changespritestat(newSprite, STAT_MISC);
            }
            goto SPAWN_END;

        case WATERBUBBLE__:
            if (spriteNum >= 0)
            {
                if (sprite[spriteNum].picnum == APLAYER)
                    pSprite->z -= (16 << 8);

                pSprite->ang = sprite[spriteNum].ang;
            }

            pSprite->xrepeat = pSprite->yrepeat = 4;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case CRANE__:

            pSprite->cstat |= 64|257;

            pSprite->picnum += 2;
            pSprite->z = sector[sectNum].ceilingz+(48<<8);
            T5(newSprite) = tempwallptr;

            g_origins[tempwallptr] = pSprite->xy;
            g_origins[tempwallptr+2].x = pSprite->z;


            if (headspritestat[STAT_DEFAULT] != -1)
            {
                int findSprite = headspritestat[STAT_DEFAULT];

                do
                {
                    if (sprite[findSprite].picnum == CRANEPOLE && pSprite->hitag == (sprite[findSprite].hitag))
                    {
                        g_origins[tempwallptr + 2].y = findSprite;

                        T2(newSprite) = sprite[findSprite].sectnum;

                        sprite[findSprite].xrepeat = 48;
                        sprite[findSprite].yrepeat = 128;

                        g_origins[tempwallptr + 1] = sprite[findSprite].xy;
                        sprite[findSprite].xyz     = pSprite->xyz;
                        sprite[findSprite].shade   = pSprite->shade;

                        setsprite(findSprite, &sprite[findSprite].xyz);
                        break;
                    }
                    findSprite = nextspritestat[findSprite];
                } while (findSprite >= 0);
            }

            tempwallptr += 3;
            pSprite->owner = -1;
            pSprite->extra = 8;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case TRASH__:
            pSprite->ang = krand()&2047;
            pSprite->xrepeat = pSprite->yrepeat = 24;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case WATERDRIP__:
            if (spriteNum >= 0 && (sprite[spriteNum].statnum == STAT_PLAYER || sprite[spriteNum].statnum == STAT_ACTOR))
            {
                if (sprite[spriteNum].pal != 1)
                {
                    pSprite->pal = 2;
                    pSprite->z -= (18<<8);
                }
                else pSprite->z -= (13<<8);

                pSprite->shade = 32;
                pSprite->ang   = getangle(g_player[0].ps->pos.x - pSprite->x, g_player[0].ps->pos.y - pSprite->y);
                pSprite->xvel  = 48 - (krand() & 31);

                A_SetSprite(newSprite, CLIPMASK0);
            }
            else if (spriteNum == -1)
            {
                pSprite->z += ZOFFSET6;
                T1(newSprite) = pSprite->z;
                T2(newSprite) = krand()&127;
            }
            fallthrough__;
        case WATERDRIPSPLASH__:
            pSprite->xrepeat = pSprite->yrepeat = 24;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case PLUG__:
            pSprite->lotag = 9999;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;
        case TARGET__:
        case DUCK__:
        case LETTER__:
            pSprite->extra = 1;
            pSprite->cstat |= 257;
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case BOSS2STAYPUT__:
        case BOSS3STAYPUT__:
        case BOSS5STAYPUT__:
            if (!WORLDTOUR)
                break;
            fallthrough__;
        case OCTABRAINSTAYPUT__:
        case LIZTROOPSTAYPUT__:
        case PIGCOPSTAYPUT__:
        case LIZMANSTAYPUT__:
        case BOSS1STAYPUT__:
        case PIGCOPDIVE__:
        case COMMANDERSTAYPUT__:
        case BOSS4STAYPUT__:
            pActor->stayput = pSprite->sectnum;
            fallthrough__;
        case GREENSLIME__:
            if (pSprite->picnum == GREENSLIME)
                pSprite->extra = 1;
            fallthrough__;
        case BOSS5__:
        case FIREFLY__:
            if (!WORLDTOUR && (pSprite->picnum == BOSS5 || pSprite->picnum == FIREFLY))
                break;
            fallthrough__;
        case BOSS1__:
        case BOSS2__:
        case BOSS3__:
        case BOSS4__:
        case ROTATEGUN__:
        case DRONE__:
        case LIZTROOPONTOILET__:
        case LIZTROOPJUSTSIT__:
        case LIZTROOPSHOOT__:
        case LIZTROOPJETPACK__:
        case LIZTROOPDUCKING__:
        case LIZTROOPRUNNING__:
        case LIZTROOP__:
        case OCTABRAIN__:
        case COMMANDER__:
        case PIGCOP__:
        case LIZMAN__:
        case LIZMANSPITTING__:
        case LIZMANFEEDING__:
        case LIZMANJUMP__:
        case ORGANTIC__:
        case RAT__:
        case SHARK__:

            if (pSprite->pal == 0)
            {
                switch (tileGetMapping(pSprite->picnum))
                {
                case LIZTROOPONTOILET__:
                case LIZTROOPSHOOT__:
                case LIZTROOPJETPACK__:
                case LIZTROOPDUCKING__:
                case LIZTROOPRUNNING__:
                case LIZTROOPSTAYPUT__:
                case LIZTROOPJUSTSIT__:
                case LIZTROOP__: pSprite->pal = 22; break;
                }
            }
            else
            {
                if (!PLUTOPAK)
                    pSprite->extra <<= 1;
            }

            if (pSprite->picnum == BOSS4STAYPUT || pSprite->picnum == BOSS1 || pSprite->picnum == BOSS2 ||
                pSprite->picnum == BOSS1STAYPUT || pSprite->picnum == BOSS3 || pSprite->picnum == BOSS4 ||
                (WORLDTOUR && (pSprite->picnum == BOSS2STAYPUT || pSprite->picnum == BOSS3STAYPUT ||
                pSprite->picnum == BOSS5STAYPUT || pSprite->picnum == BOSS5)))
            {
                if (spriteNum >= 0 && sprite[spriteNum].picnum == RESPAWN)
                    pSprite->pal = sprite[spriteNum].pal;

                if (pSprite->pal && (!WORLDTOUR || pSprite->pal != 22))
                {
                    pSprite->clipdist = 80;
                    pSprite->xrepeat  = pSprite->yrepeat = 40;
                }
                else
                {
                    pSprite->xrepeat  = pSprite->yrepeat = 80;
                    pSprite->clipdist = 164;
                }
            }
            else
            {
                if (pSprite->picnum != SHARK)
                {
                    pSprite->xrepeat  = pSprite->yrepeat = 40;
                    pSprite->clipdist = 80;
                }
                else
                {
                    pSprite->xrepeat  = pSprite->yrepeat = 60;
                    pSprite->clipdist = 40;
                }
            }

            // If spawned from parent sprite (as opposed to 'from premap'),
            // ignore skill.
            if (spriteNum >= 0)
                pSprite->lotag = 0;

            if ((pSprite->lotag > ud.player_skill) || ud.monsters_off == 1)
            {
                pSprite->xrepeat=pSprite->yrepeat=0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            else
            {
                A_Fall(newSprite);

                if (pSprite->picnum == RAT)
                {
                    pSprite->ang = krand()&2047;
                    pSprite->xrepeat = pSprite->yrepeat = 48;
                    pSprite->cstat = 0;
                }
                else
                {
                    pSprite->cstat |= 257;

                    if (pSprite->picnum != SHARK)
                        g_player[myconnectindex].ps->max_actors_killed++;
                }

                if (pSprite->picnum == ORGANTIC) pSprite->cstat |= 128;

                if (spriteNum >= 0)
                {
                    pActor->timetosleep = 0;
                    A_PlayAlertSound(newSprite);
                    changespritestat(newSprite, STAT_ACTOR);
                }
                else changespritestat(newSprite, STAT_ZOMBIEACTOR);
            }

            if (pSprite->picnum == ROTATEGUN)
                pSprite->zvel = 0;

            goto SPAWN_END;

        case REACTOR2__:
        case REACTOR__:
            pSprite->extra = g_impactDamage;
            pSprite->cstat |= 257;
            if ((!g_netServer && ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->pal   = 0;
            pSprite->shade = -17;

            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case HEAVYHBOMB__:
            if (spriteNum >= 0)
                pSprite->owner = spriteNum;
            else pSprite->owner = newSprite;

            pSprite->xrepeat = pSprite->yrepeat = 9;
            pSprite->yvel = 4;
            pSprite->cstat |= 257;

            if ((!g_netServer && ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            pSprite->pal   = 0;
            pSprite->shade = -17;

            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case RECON__:
            if (pSprite->lotag > ud.player_skill)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            g_player[myconnectindex].ps->max_actors_killed++;
            pActor->t_data[5] = 0;
            if (ud.monsters_off == 1)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            pSprite->extra = 130;
            pSprite->cstat |= 256; // Make it hitable

            if ((!g_netServer && ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            pSprite->pal   = 0;
            pSprite->shade = -17;

            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case FLAMETHROWERSPRITE__:
        case FLAMETHROWERAMMO__:
            if (!WORLDTOUR)
                break;
            fallthrough__;

        case ATOMICHEALTH__:
        case STEROIDS__:
        case HEATSENSOR__:
        case SHIELD__:
        case AIRTANK__:
        case TRIPBOMBSPRITE__:
        case JETPACK__:
        case HOLODUKE__:

        case FIRSTGUNSPRITE__:
        case CHAINGUNSPRITE__:
        case SHOTGUNSPRITE__:
        case RPGSPRITE__:
        case SHRINKERSPRITE__:
        case FREEZESPRITE__:
        case DEVISTATORSPRITE__:

        case SHOTGUNAMMO__:
        case FREEZEAMMO__:
        case HBOMBAMMO__:
        case CRYSTALAMMO__:
        case GROWAMMO__:
        case BATTERYAMMO__:
        case DEVISTATORAMMO__:
        case RPGAMMO__:
        case BOOTS__:
        case AMMO__:
        case AMMOLOTS__:
        case COLA__:
        case FIRSTAID__:
        case SIXPAK__:

            if (spriteNum >= 0)
            {
                pSprite->lotag = 0;
                pSprite->z -= ZOFFSET5;
                pSprite->zvel = -1024;
                A_SetSprite(newSprite, CLIPMASK0);
                pSprite->cstat = krand()&4;
            }
            else
            {
                pSprite->owner = newSprite;
                pSprite->cstat = 0;
            }

            if (((!g_netServer && ud.multimode < 2) && pSprite->pal != 0) || (pSprite->lotag > ud.player_skill))
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->pal = 0;

            if (pSprite->picnum == ATOMICHEALTH)
                pSprite->cstat |= 128;

            fallthrough__;
        case ACCESSCARD__:
            if ((g_netServer || ud.multimode > 1) && !GTFLAGS(GAMETYPE_ACCESSCARDSPRITES) && pSprite->picnum == ACCESSCARD)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            else
            {
                if (pSprite->picnum == AMMO)
                    pSprite->xrepeat = pSprite->yrepeat = 16;
                else pSprite->xrepeat = pSprite->yrepeat = 32;
            }

            pSprite->shade = -17;

            if (spriteNum >= 0)
            {
                changespritestat(newSprite, STAT_ACTOR);
            }
            else
            {
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
                A_Fall(newSprite);
            }
            goto SPAWN_END;

        case WATERFOUNTAIN__:
            SLT(newSprite) = 1;
            fallthrough__;
        case TREE1__:
        case TREE2__:
        case TIRE__:
        case CONE__:
        case BOX__:
            pSprite->cstat = 257; // Make it hitable
            sprite[newSprite].extra = 1;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case FLOORFLAME__:
            pSprite->shade = -127;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case BOUNCEMINE__:
            pSprite->owner = newSprite;
            pSprite->cstat |= 1+256; //Make it hitable
            pSprite->xrepeat = pSprite->yrepeat = 24;
            pSprite->shade = -127;
            pSprite->extra = g_impactDamage<<2;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case STEAM__:
            if (spriteNum >= 0)
            {
                pSprite->ang = sprite[spriteNum].ang;
                pSprite->cstat = 16+128+2;
                pSprite->xrepeat=pSprite->yrepeat=1;
                pSprite->xvel = -8;
                A_SetSprite(newSprite, CLIPMASK0);
            }
            fallthrough__;
        case CEILINGSTEAM__:
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case TOILET__:
        case STALL__:
            pSprite->lotag = 1;
            pSprite->cstat |= 257;
            pSprite->clipdist = 8;
            pSprite->owner = newSprite;
            goto SPAWN_END;

        case CANWITHSOMETHING__:
        case CANWITHSOMETHING2__:
        case CANWITHSOMETHING3__:
        case CANWITHSOMETHING4__:
        case RUBBERCAN__:
            pSprite->extra = 0;
            fallthrough__;
        case EXPLODINGBARREL__:
        case HORSEONSIDE__:
        case FIREBARREL__:
        case NUKEBARREL__:
        case FIREVASE__:
        case NUKEBARRELDENTED__:
        case NUKEBARRELLEAKED__:
        case WOODENHORSE__:
            if (spriteNum >= 0)
                pSprite->xrepeat = pSprite->yrepeat = 32;
            pSprite->clipdist = 72;
            A_Fall(newSprite);
            if (spriteNum >= 0)
                pSprite->owner = spriteNum;
            else pSprite->owner = newSprite;
            fallthrough__;
        case EGG__:
            if (ud.monsters_off == 1 && pSprite->picnum == EGG)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else
            {
                if (pSprite->picnum == EGG)
                    pSprite->clipdist = 24;
                pSprite->cstat = 257|(krand()&4);
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
            }
            goto SPAWN_END;

        case TOILETWATER__:
            pSprite->shade = -16;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case LASERLINE__:
            pSprite->yrepeat = 6;
            pSprite->xrepeat = 32;

            if (g_tripbombLaserMode == 1)
                pSprite->cstat = 16 + 2;
            else if (g_tripbombLaserMode == 0 || g_tripbombLaserMode == 2)
                pSprite->cstat = 16;
            else
            {
                pSprite->xrepeat = 0;
                pSprite->yrepeat = 0;
            }

            if (spriteNum >= 0) pSprite->ang = actor[spriteNum].t_data[5]+512;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case FORCESPHERE__:
            if (spriteNum == -1)
            {
                pSprite->cstat = 32768;
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
            }
            else
            {
                pSprite->xrepeat = pSprite->yrepeat = 1;
                changespritestat(newSprite, STAT_MISC);
            }
            goto SPAWN_END;

        case BLOOD__:
            pSprite->xrepeat = pSprite->yrepeat = 16;
            pSprite->z -= (26<<8);
            if (spriteNum >= 0 && sprite[spriteNum].pal == 6)
                pSprite->pal = 6;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case LAVAPOOL__:
            if (!WORLDTOUR)
                break;
            fallthrough__;
        case BLOODPOOL__:
        case PUKE__:
        {
            int16_t pukeSect = pSprite->sectnum;

            updatesector(pSprite->x + 108, pSprite->y + 108, &pukeSect);
            if (pukeSect >= 0 && sector[pukeSect].floorz == sector[pSprite->sectnum].floorz)
            {
                updatesector(pSprite->x - 108, pSprite->y - 108, &pukeSect);
                if (pukeSect >= 0 && sector[pukeSect].floorz == sector[pSprite->sectnum].floorz)
                {
                    updatesector(pSprite->x + 108, pSprite->y - 108, &pukeSect);
                    if (pukeSect >= 0 && sector[pukeSect].floorz == sector[pSprite->sectnum].floorz)
                    {
                        updatesector(pSprite->x - 108, pSprite->y + 108, &pukeSect);
                        if (pukeSect >= 0 && sector[pukeSect].floorz != sector[pSprite->sectnum].floorz)
                            goto zero_puke;
                    }
                    else goto zero_puke;
                }
                else goto zero_puke;
            }
            else
            {
            zero_puke:
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            if (sector[sectNum].lotag == ST_1_ABOVE_WATER)
            {
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            if (spriteNum >= 0 && pSprite->picnum != PUKE)
            {
                if (sprite[spriteNum].pal == 1)
                    pSprite->pal = 1;
                else if (sprite[spriteNum].pal != 6 && sprite[spriteNum].picnum != NUKEBARREL && sprite[spriteNum].picnum != TIRE)
                    pSprite->pal = (sprite[spriteNum].picnum == FECES) ? 7 : 2;  // Brown or red
                else
                    pSprite->pal = 0;  // green

                if (sprite[spriteNum].picnum == TIRE)
                    pSprite->shade = 127;
            }
            pSprite->cstat |= 32;
            if (pSprite->picnum == LAVAPOOL)
                pSprite->z = getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y) - 200;
            fallthrough__;
        }
        case FECES__:
            if (spriteNum >= 0)
                pSprite->xrepeat = pSprite->yrepeat = 1;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case BLOODSPLAT1__:
        case BLOODSPLAT2__:
        case BLOODSPLAT3__:
        case BLOODSPLAT4__:
            pSprite->cstat |= 16;
            pSprite->xrepeat = 7 + (krand() & 7);
            pSprite->yrepeat = 7 + (krand() & 7);
            pSprite->z += (tilesiz[pSprite->picnum].y * pSprite->yrepeat) >> 2;

            if (spriteNum >= 0 && sprite[spriteNum].pal == 6)
                pSprite->pal = 6;

            A_AddToDeleteQueue(newSprite);
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case TRIPBOMB__:
            if (pSprite->lotag > ud.player_skill)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->xrepeat = 4;
            pSprite->yrepeat = 5;
            pSprite->hitag   = newSprite;
            pSprite->owner   = pSprite->hitag;
            pSprite->xvel    = 16;

            A_SetSprite(newSprite, CLIPMASK0);

            pActor->t_data[0] = 17;
            pActor->t_data[2] = 0;
            pActor->t_data[5] = pSprite->ang;

            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case SPACEMARINE__:
            pSprite->extra = 20;
            pSprite->cstat |= 257;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;
        case DOORSHOCK__:
            pSprite->cstat |= 1+256;
            pSprite->shade = -12;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;
        case HYDRENT__:
        case PANNEL1__:
        case PANNEL2__:
        case SATELITE__:
        case FUELPOD__:
        case SOLARPANNEL__:
        case ANTENNA__:
        case CHAIR1__:
        case CHAIR2__:
        case CHAIR3__:
        case BOTTLE1__:
        case BOTTLE2__:
        case BOTTLE3__:
        case BOTTLE4__:
        case BOTTLE5__:
        case BOTTLE6__:
        case BOTTLE7__:
        case BOTTLE8__:
        case BOTTLE10__:
        case BOTTLE11__:
        case BOTTLE12__:
        case BOTTLE13__:
        case BOTTLE14__:
        case BOTTLE15__:
        case BOTTLE16__:
        case BOTTLE17__:
        case BOTTLE18__:
        case BOTTLE19__:
        case OCEANSPRITE1__:
        case OCEANSPRITE2__:
        case OCEANSPRITE3__:
        case OCEANSPRITE5__:
        case MONK__:
        case INDY__:
        case LUKE__:
        case JURYGUY__:
        case SCALE__:
        case VACUUM__:
        case CACTUS__:
        case CACTUSBROKE__:
        case HANGLIGHT__:
        case FETUS__:
        case FETUSBROKE__:
        case CAMERALIGHT__:
        case MOVIECAMERA__:
        case IVUNIT__:
        case POT1__:
        case POT2__:
        case POT3__:
        case TRIPODCAMERA__:
        case SUSHIPLATE1__:
        case SUSHIPLATE2__:
        case SUSHIPLATE3__:
        case SUSHIPLATE4__:
        case SUSHIPLATE5__:
        case WAITTOBESEATED__:
        case VASE__:
        case PIPE1__:
        case PIPE2__:
        case PIPE3__:
        case PIPE4__:
        case PIPE5__:
        case PIPE6__:
#endif
        case GRATE1__:
        case FANSPRITE__:
            pSprite->clipdist = 32;
            pSprite->cstat |= 257;
            fallthrough__;
        case OCEANSPRITE4__:
            changespritestat(newSprite, STAT_DEFAULT);
            goto SPAWN_END;

        case FRAMEEFFECT1_13__:
            if (PLUTOPAK)
                break;
            fallthrough__;
        case FRAMEEFFECT1__:
            if (spriteNum >= 0)
            {
                pSprite->xrepeat = sprite[spriteNum].xrepeat;
                pSprite->yrepeat = sprite[spriteNum].yrepeat;
                T2(newSprite) = sprite[spriteNum].picnum;
            }
            else pSprite->xrepeat = pSprite->yrepeat = 0;

            changespritestat(newSprite, STAT_MISC);

            goto SPAWN_END;
        case FOOTPRINTS__:
        case FOOTPRINTS2__:
        case FOOTPRINTS3__:
        case FOOTPRINTS4__:
            if (spriteNum >= 0)
            {
                int16_t footSect = pSprite->sectnum;

                updatesector(pSprite->x + 84, pSprite->y + 84, &footSect);
                if (footSect >= 0 && sector[footSect].floorz == sector[pSprite->sectnum].floorz)
                {
                    updatesector(pSprite->x - 84, pSprite->y - 84, &footSect);
                    if (footSect >= 0 && sector[footSect].floorz == sector[pSprite->sectnum].floorz)
                    {
                        updatesector(pSprite->x + 84, pSprite->y - 84, &footSect);
                        if (footSect >= 0 && sector[footSect].floorz == sector[pSprite->sectnum].floorz)
                        {
                            updatesector(pSprite->x - 84, pSprite->y + 84, &footSect);
                            if (footSect >= 0 && sector[footSect].floorz != sector[pSprite->sectnum].floorz)
                            {
                                pSprite->xrepeat = pSprite->yrepeat = 0;
                                changespritestat(newSprite, STAT_MISC);
                                goto SPAWN_END;
                            }
                        }
                        else goto zero_footprint;
                    }
                    else goto zero_footprint;
                }
                else
                {
                zero_footprint:
                    pSprite->xrepeat = pSprite->yrepeat = 0;
                    goto SPAWN_END;
                }

                pSprite->cstat = 32 + ((g_player[P_Get(spriteNum)].ps->footprintcount & 1) << 2);
                pSprite->ang   = sprite[spriteNum].ang;
            }

            pSprite->z = sector[sectNum].floorz;

            if (sector[sectNum].lotag != ST_1_ABOVE_WATER && sector[sectNum].lotag != ST_2_UNDERWATER)
                pSprite->xrepeat = pSprite->yrepeat = 32;

            A_AddToDeleteQueue(newSprite);
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case VIEWSCREEN__:
        case VIEWSCREEN2__:
            pSprite->owner = newSprite;
            pSprite->lotag = pSprite->extra = 1;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;
        case RESPAWN__:
            pSprite->extra = 66-13;
            fallthrough__;
        case MUSICANDSFX__:
            if ((!g_netServer && ud.multimode < 2) && pSprite->pal == 1)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            pSprite->cstat = 32768;
            changespritestat(newSprite, STAT_FX);
            goto SPAWN_END;

        case EXPLOSION2__:
#ifdef POLYMER
            if (pSprite->yrepeat > 32)
            {
                G_AddGameLight(newSprite, pSprite->sectnum, { 0, 0, LIGHTZOFF(newSprite) }, 32768, 0, 100,255+(95<<8), PR_LIGHT_PRIO_MAX_GAME);
                practor[newSprite].lightcount = 2;
            }
            fallthrough__;
#endif
#ifndef EDUKE32_STANDALONE
        case ONFIRE__:
            if (!WORLDTOUR && pSprite->picnum == ONFIRE)
                break;
            fallthrough__;
        case EXPLOSION2BOT__:
        case BURNING__:
        case BURNING2__:
        case SMALLSMOKE__:
        case SHRINKEREXPLOSION__:
        case COOLEXPLOSION1__:
#endif
            if (spriteNum >= 0)
            {
                pSprite->ang = sprite[spriteNum].ang;
                pSprite->shade = -64;
                pSprite->cstat = 128|(krand()&4);
            }

            if (pSprite->picnum == EXPLOSION2 || pSprite->picnum == EXPLOSION2BOT)
            {
                pSprite->xrepeat = pSprite->yrepeat = 48;
                pSprite->shade = -127;
                pSprite->cstat |= 128;
            }
            else if (pSprite->picnum == SHRINKEREXPLOSION)
                pSprite->xrepeat = pSprite->yrepeat = 32;
            else if (pSprite->picnum == SMALLSMOKE || pSprite->picnum == ONFIRE)
            {
                // 64 "money"
                pSprite->xrepeat = pSprite->yrepeat = 24;
            }
            else if (pSprite->picnum == BURNING || pSprite->picnum == BURNING2)
                pSprite->xrepeat = pSprite->yrepeat = 4;

            pSprite->cstat |= 8192;

            if (spriteNum >= 0)
            {
#ifdef YAX_ENABLE
                int const floorZ = yax_getflorzofslope(pSprite->sectnum, pSprite->xy);
#else
                int const floorZ = getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
#endif

                if (pSprite->z > floorZ-ZOFFSET4)
                    pSprite->z = floorZ-ZOFFSET4;
            }

            if (pSprite->picnum == ONFIRE)
            {
                pActor->bpos.x = pSprite->x += (krand()%256)-128;
                pActor->bpos.y = pSprite->y += (krand()%256)-128;
                pActor->bpos.z = pSprite->z -= krand()%10240;
                pSprite->cstat |= 128;
            }

            changespritestat(newSprite, STAT_MISC);

            goto SPAWN_END;

        case PLAYERONWATER__:
            if (spriteNum >= 0)
            {
                pSprite->xrepeat = sprite[spriteNum].xrepeat;
                pSprite->yrepeat = sprite[spriteNum].yrepeat;
                pSprite->zvel = 128;
                if (sector[pSprite->sectnum].lotag != ST_2_UNDERWATER)
                    pSprite->cstat |= 32768;
            }
            changespritestat(newSprite, STAT_DUMMYPLAYER);
            goto SPAWN_END;

        case APLAYER__:
            pSprite->xrepeat = 0;
            pSprite->yrepeat = 0;
            pSprite->cstat   = 32768;

            changespritestat(newSprite, ((!g_netServer && ud.multimode < 2)
                                         || ((g_gametypeFlags[ud.coop] & GAMETYPE_COOPSPAWN) / GAMETYPE_COOPSPAWN) != pSprite->lotag)
                                        ? STAT_MISC
                                        : STAT_PLAYER);
            goto SPAWN_END;
        case TOUCHPLATE__:
            T3(newSprite) = sector[sectNum].floorz;

            if (sector[sectNum].lotag != ST_1_ABOVE_WATER && sector[sectNum].lotag != ST_2_UNDERWATER)
                sector[sectNum].floorz = pSprite->z;

            if (pSprite->pal && (g_netServer || ud.multimode > 1))
            {
                pSprite->xrepeat=pSprite->yrepeat=0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
#ifndef EDUKE32_STANDALONE
            fallthrough__;
        case WATERBUBBLEMAKER__:
            if (EDUKE32_PREDICT_FALSE(pSprite->hitag && pSprite->picnum == WATERBUBBLEMAKER))
            {
                // JBF 20030913: Pisses off X_Move(), eg. in bobsp2
                LOG_F(WARNING, "WATERBUBBLEMAKER %d @ %d,%d with hitag!=0. Applying fixup.",
                           newSprite,TrackerCast(pSprite->x),TrackerCast(pSprite->y));
                pSprite->hitag = 0;
            }
#endif
            pSprite->cstat |= 32768;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case MASTERSWITCH__:
            pSprite->cstat |= 32768;
            pSprite->extra = pSprite->hitag;
            pSprite->yvel = 0;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case LOCATORS__:
            pSprite->cstat |= 32768;
            changespritestat(newSprite, STAT_LOCATOR);
            goto SPAWN_END;

        case ACTIVATORLOCKED__:
        case ACTIVATOR__:
            pSprite->cstat = 32768;
            if (pSprite->picnum == ACTIVATORLOCKED)
                sector[pSprite->sectnum].lotag |= 16384;
            changespritestat(newSprite, STAT_ACTIVATOR);
            goto SPAWN_END;

        case OOZ__:
        case OOZ2__:
        {
            pSprite->shade = -12;

            if (spriteNum >= 0)
            {
                if (sprite[spriteNum].picnum == NUKEBARREL)
                    pSprite->pal = 8;
                A_AddToDeleteQueue(newSprite);
            }

            changespritestat(newSprite, STAT_ACTOR);

            A_GetZLimits(newSprite);

            int const oozSize = (pActor->floorz-pActor->ceilingz)>>9;

            pSprite->yrepeat = oozSize;
            pSprite->xrepeat = 25 - (oozSize >> 1);
            pSprite->cstat |= (krand() & 4);

            goto SPAWN_END;
        }

        case SECTOREFFECTOR__:
            pSprite->cstat |= 32768;
            pSprite->xrepeat = pSprite->yrepeat = 0;

            switch (pSprite->lotag)
            {
#ifdef LEGACY_ROR
            case 40:
            case 41:
                pSprite->cstat = 32;
                pSprite->xrepeat = pSprite->yrepeat = 64;
                changespritestat(newSprite, STAT_EFFECTOR);
                for (spriteNum=0; spriteNum < MAXSPRITES; spriteNum++)
                    if (sprite[spriteNum].picnum == SECTOREFFECTOR && (sprite[spriteNum].lotag == 40 || sprite[spriteNum].lotag == 41) &&
                            sprite[spriteNum].hitag == pSprite->hitag && newSprite != spriteNum)
                    {
//                        initprintf("found ror match\n");
                        pSprite->yvel = spriteNum;
                        break;
                    }
                goto SPAWN_END;
                break;
            case 46:
                bitmap_set(ror_protectedsectors, pSprite->sectnum);
                /* XXX: fall-through intended? */
                fallthrough__;
#endif
            case SE_49_POINT_LIGHT:
            case SE_50_SPOT_LIGHT:
            {
                int32_t j, nextj;

                for (TRAVERSE_SPRITE_SECT(headspritesect[pSprite->sectnum], j, nextj))
                    if (sprite[j].picnum == ACTIVATOR || sprite[j].picnum == ACTIVATORLOCKED)
                        pActor->flags |= SFLAG_USEACTIVATOR;
            }
            changespritestat(newSprite, pSprite->lotag==46 ? STAT_EFFECTOR : STAT_LIGHT);
            goto SPAWN_END;
            break;
            }

            pSprite->yvel = sector[sectNum].extra;

            switch (pSprite->lotag)
            {
            case SE_28_LIGHTNING:
                T6(newSprite) = 65;// Delay for lightning
                break;
            case SE_7_TELEPORT: // Transporters!!!!
            case SE_23_ONE_WAY_TELEPORT:// XPTR END
                if (pSprite->lotag != SE_23_ONE_WAY_TELEPORT)
                {
                    for (spriteNum=0; spriteNum<MAXSPRITES; spriteNum++)
                        if (sprite[spriteNum].statnum < MAXSTATUS && sprite[spriteNum].picnum == SECTOREFFECTOR &&
                                (sprite[spriteNum].lotag == SE_7_TELEPORT || sprite[spriteNum].lotag == SE_23_ONE_WAY_TELEPORT) && newSprite != spriteNum && sprite[spriteNum].hitag == SHT(newSprite))
                        {
                            OW(newSprite) = spriteNum;
                            break;
                        }
                }
                else OW(newSprite) = newSprite;

                T5(newSprite) = (sector[sectNum].floorz == SZ(newSprite));  // ONFLOORZ
                pSprite->cstat = 0;
                changespritestat(newSprite, STAT_TRANSPORT);
                goto SPAWN_END;
            case SE_1_PIVOT:
                pSprite->owner = -1;
                T1(newSprite) = 1;
                break;
            case SE_18_INCREMENTAL_SECTOR_RISE_FALL:

                if (pSprite->ang == 512)
                {
                    T2(newSprite) = sector[sectNum].ceilingz;
                    if (pSprite->pal)
                        sector[sectNum].ceilingz = pSprite->z;
                }
                else
                {
                    T2(newSprite) = sector[sectNum].floorz;
                    if (pSprite->pal)
                        sector[sectNum].floorz = pSprite->z;
                }

                pSprite->hitag <<= 2;
                break;

            case SE_19_EXPLOSION_LOWERS_CEILING:
                pSprite->owner = -1;
                break;
            case SE_25_PISTON: // Pistons
                T4(newSprite) = sector[sectNum].ceilingz;
                T5(newSprite) = 1;
                sector[sectNum].ceilingz = pSprite->z;
                G_SetInterpolation(&sector[sectNum].ceilingz);
                break;
            case SE_35:
                sector[sectNum].ceilingz = pSprite->z;
                break;
            case SE_27_DEMO_CAM:
                T1(newSprite) = 0;
                if (ud.recstat == 1)
                {
                    pSprite->xrepeat=pSprite->yrepeat=64;
                    pSprite->cstat &= 32768;
                }
                break;
            case SE_12_LIGHT_SWITCH:

                T2(newSprite) = sector[sectNum].floorshade;
                T3(newSprite) = sector[sectNum].ceilingshade;
                break;

            case SE_13_EXPLOSIVE:

                T1(newSprite) = sector[sectNum].ceilingz;
                T2(newSprite) = sector[sectNum].floorz;

                if (klabs(T1(newSprite)-pSprite->z) < klabs(T2(newSprite)-pSprite->z))
                    pSprite->owner = 1;
                else pSprite->owner = 0;

                if (pSprite->ang == 512)
                {
                    if (pSprite->owner)
                        sector[sectNum].ceilingz = pSprite->z;
                    else
                        sector[sectNum].floorz = pSprite->z;
#ifdef YAX_ENABLE
                    {
                        int16_t cf=!pSprite->owner, bn=yax_getbunch(sectNum, cf);
                        int32_t jj, daz=SECTORFLD(sectNum,z, cf);

                        if (bn >= 0)
                        {
                            for (SECTORS_OF_BUNCH(bn, cf, jj))
                            {
                                SECTORFLD(jj,z, cf) = daz;
                                SECTORFLD(jj,stat, cf) &= ~256;
                                SECTORFLD(jj,stat, cf) |= 128 + 512+2048;
                            }
                            for (SECTORS_OF_BUNCH(bn, !cf, jj))
                            {
                                SECTORFLD(jj,z, !cf) = daz;
                                SECTORFLD(jj,stat, !cf) &= ~256;
                                SECTORFLD(jj,stat, !cf) |= 128 + 512+2048;
                            }
                        }
                    }
#endif
                }
                else
                    sector[sectNum].ceilingz = sector[sectNum].floorz = pSprite->z;

                if (sector[sectNum].ceilingstat&1)
                {
                    sector[sectNum].ceilingstat ^= 1;
                    T4(newSprite) = 1;

                    if (!pSprite->owner && pSprite->ang==512)
                    {
                        sector[sectNum].ceilingstat ^= 1;
                        T4(newSprite) = 0;
                    }

                    sector[sectNum].ceilingshade =
                        sector[sectNum].floorshade;

                    if (pSprite->ang==512)
                    {
                        int const startwall = sector[sectNum].wallptr;
                        int const endwall   = startwall + sector[sectNum].wallnum;
                        for (bssize_t j = startwall; j < endwall; j++)
                        {
                            int const nextSect = wall[j].nextsector;

                            if (nextSect >= 0)
                            {
                                if (!(sector[nextSect].ceilingstat & 1))
                                {
                                    sector[sectNum].ceilingpicnum = sector[nextSect].ceilingpicnum;
                                    sector[sectNum].ceilingshade  = sector[nextSect].ceilingshade;
                                    break;  // Leave earily
                                }
                            }
                        }
                    }
                }

                break;

            case SE_17_WARP_ELEVATOR:
            {
                T3(newSprite) = sector[sectNum].floorz;  // Stopping loc

                int nextSectNum = nextsectorneighborz(sectNum, sector[sectNum].floorz, -1, -1);

                if (EDUKE32_PREDICT_TRUE(nextSectNum >= 0))
                    T4(newSprite) = sector[nextSectNum].ceilingz;
                else
                {
                    // use elevator sector's ceiling as heuristic
                    T4(newSprite) = sector[sectNum].ceilingz;

                    LOG_F(WARNING, "SE17 sprite %d using own sector's ceilingz to "
                                   "determine when to warp. Sector %d adjacent to a door?",
                                   newSprite, sectNum);
                }

                nextSectNum = nextsectorneighborz(sectNum, sector[sectNum].ceilingz, 1, 1);

                if (EDUKE32_PREDICT_TRUE(nextSectNum >= 0))
                    T5(newSprite) = sector[nextSectNum].floorz;
                else
                {
                    // heuristic
                    T5(newSprite) = sector[sectNum].floorz;
                    LOG_F(WARNING, "SE17 sprite %d using own sector %d's floorz.", newSprite, sectNum);
                }

                if (numplayers < 2 && !g_netServer)
                {
                    G_SetInterpolation(&sector[sectNum].floorz);
                    G_SetInterpolation(&sector[sectNum].ceilingz);
                }
            }
            break;

            case SE_24_CONVEYOR:
                pSprite->yvel <<= 1;
            case SE_36_PROJ_SHOOTER:
                break;

            case SE_20_STRETCH_BRIDGE:
            {
                int       closestDist = INT32_MAX;
                int       closestWall = 0;
                int const startWall   = sector[sectNum].wallptr;
                int const endWall     = startWall + sector[sectNum].wallnum;

                for (bssize_t findWall=startWall; findWall<endWall; findWall++)
                {
                    int const x = wall[findWall].x;
                    int const y = wall[findWall].y;
                    int const d = FindDistance2D(pSprite->x - x, pSprite->y - y);

                    if (d < closestDist)
                    {
                        closestDist = d;
                        closestWall = findWall;
                    }
                }

                T2(newSprite) = closestWall;

                closestDist = INT32_MAX;

                for (bssize_t findWall=startWall; findWall<endWall; findWall++)
                {
                    int const x = wall[findWall].x;
                    int const y = wall[findWall].y;
                    int const d = FindDistance2D(pSprite->x - x, pSprite->y - y);

                    if (d < closestDist && findWall != T2(newSprite))
                    {
                        closestDist = d;
                        closestWall = findWall;
                    }
                }

                T3(newSprite) = closestWall;
            }

            break;

            case SE_3_RANDOM_LIGHTS_AFTER_SHOT_OUT:
            {

                T4(newSprite)=sector[sectNum].floorshade;

                sector[sectNum].floorshade   = pSprite->shade;
                sector[sectNum].ceilingshade = pSprite->shade;

                pSprite->owner = sector[sectNum].ceilingpal << 8;
                pSprite->owner |= sector[sectNum].floorpal;

                //fix all the walls;

                int const startWall = sector[sectNum].wallptr;
                int const endWall = startWall+sector[sectNum].wallnum;

                for (bssize_t w=startWall; w<endWall; ++w)
                {
                    if (!(wall[w].hitag & 1))
                        wall[w].shade = pSprite->shade;

                    if ((wall[w].cstat & 2) && wall[w].nextwall >= 0)
                        wall[wall[w].nextwall].shade = pSprite->shade;
                }
                break;
            }

            case SE_31_FLOOR_RISE_FALL:
            {
                T2(newSprite) = sector[sectNum].floorz;

                if (pSprite->ang != 1536)
                {
                    sector[sectNum].floorz = pSprite->z;
                    Yax_SetBunchZs(sectNum, YAX_FLOOR, pSprite->z);
                }

                int const startWall = sector[sectNum].wallptr;
                int const endWall   = startWall + sector[sectNum].wallnum;

                for (bssize_t w = startWall; w < endWall; ++w)
                    if (wall[w].hitag == 0)
                        wall[w].hitag = 9999;

                G_SetInterpolation(&sector[sectNum].floorz);
                Yax_SetBunchInterpolation(sectNum, YAX_FLOOR);
            }
            break;

            case SE_32_CEILING_RISE_FALL:
            {
                T2(newSprite) = sector[sectNum].ceilingz;
                T3(newSprite) = pSprite->hitag;

                if (pSprite->ang != 1536)
                {
                    sector[sectNum].ceilingz = pSprite->z;
                    Yax_SetBunchZs(sectNum, YAX_CEILING, pSprite->z);
                }

                int const startWall = sector[sectNum].wallptr;
                int const endWall   = startWall + sector[sectNum].wallnum;

                for (bssize_t w = startWall; w < endWall; ++w)
                    if (wall[w].hitag == 0)
                        wall[w].hitag = 9999;

                G_SetInterpolation(&sector[sectNum].ceilingz);
                Yax_SetBunchInterpolation(sectNum, YAX_CEILING);
            }
            break;

            case SE_4_RANDOM_LIGHTS: //Flashing lights
            {
                T3(newSprite) = sector[sectNum].floorshade;

                int const startWall = sector[sectNum].wallptr;
                int const endWall   = startWall + sector[sectNum].wallnum;

                pSprite->owner = sector[sectNum].ceilingpal << 8;
                pSprite->owner |= sector[sectNum].floorpal;

                for (bssize_t w = startWall; w < endWall; ++w)
                    if (wall[w].shade > T4(newSprite))
                        T4(newSprite) = wall[w].shade;
            }
            break;

            case SE_9_DOWN_OPEN_DOOR_LIGHTS:
                if (sector[sectNum].lotag &&
                        labs(sector[sectNum].ceilingz-pSprite->z) > 1024)
                    sector[sectNum].lotag |= 32768u; //If its open
                fallthrough__;
            case SE_8_UP_OPEN_DOOR_LIGHTS:
                //First, get the ceiling-floor shade
                {
                    T1(newSprite) = sector[sectNum].floorshade;
                    T2(newSprite) = sector[sectNum].ceilingshade;

                    int const startWall = sector[sectNum].wallptr;
                    int const endWall   = startWall + sector[sectNum].wallnum;

                    for (bssize_t w = startWall; w < endWall; ++w)
                        if (wall[w].shade > T3(newSprite))
                            T3(newSprite) = wall[w].shade;

                    T4(newSprite) = 1;  // Take Out;
                }
                break;

            case SE_11_SWINGING_DOOR:  // Pivitor rotater
                T4(newSprite) = (pSprite->ang > 1024) ? 2 : -2;
                fallthrough__;
            case SE_0_ROTATING_SECTOR:
            case SE_2_EARTHQUAKE:      // Earthquakemakers
            case SE_5:                 // Boss Creature
            case SE_6_SUBWAY:          // Subway
            case SE_14_SUBWAY_CAR:     // Caboos
            case SE_15_SLIDING_DOOR:   // Subwaytype sliding door
            case SE_16_REACTOR:        // That rotating blocker reactor thing
            case SE_26_ESCALATOR:      // ESCELATOR
            case SE_30_TWO_WAY_TRAIN:  // No rotational subways
                if (pSprite->lotag == SE_0_ROTATING_SECTOR)
                {
                    if (sector[sectNum].lotag == ST_30_ROTATE_RISE_BRIDGE)
                    {
                        sprite[newSprite].clipdist = (pSprite->pal) ? 1 : 0;
                        T4(newSprite) = sector[sectNum].floorz;
                        sector[sectNum].hitag = newSprite;
                    }

                    for (spriteNum = MAXSPRITES-1; spriteNum>=0; spriteNum--)
                    {
                        if (sprite[spriteNum].statnum < MAXSTATUS)
                            if (sprite[spriteNum].picnum == SECTOREFFECTOR &&
                                    sprite[spriteNum].lotag == SE_1_PIVOT &&
                                    sprite[spriteNum].hitag == pSprite->hitag)
                            {
                                if (pSprite->ang == 512)
                                {
                                    pSprite->x = sprite[spriteNum].x;
                                    pSprite->y = sprite[spriteNum].y;
                                }
                                break;
                            }
                    }
                    if (EDUKE32_PREDICT_FALSE(spriteNum == -1))
                    {
                        LOG_F(ERROR, "Found lonely Sector Effector (lotag 0) at (%d,%d)",
                            TrackerCast(pSprite->x),TrackerCast(pSprite->y));
                        changespritestat(newSprite, STAT_ACTOR);
                        goto SPAWN_END;
                    }
                    pSprite->owner = spriteNum;
                }

                {
                    int const startWall = sector[sectNum].wallptr;
                    int const endWall = startWall+sector[sectNum].wallnum;

                    T2(newSprite) = tempwallptr;
                    for (bssize_t w = startWall; w < endWall; ++w)
                    {
                        g_origins[tempwallptr].x = wall[w].x - pSprite->x;
                        g_origins[tempwallptr].y = wall[w].y - pSprite->y;

                        tempwallptr++;
                        if (EDUKE32_PREDICT_FALSE(tempwallptr >= MAXANIMPOINTS))
                        {
                            Bsprintf(tempbuf, "Too many moving sectors at (%d,%d).",
                                TrackerCast(wall[w].x), TrackerCast(wall[w].y));
                            G_GameExit(tempbuf);
                        }
                    }
                }

                if (pSprite->lotag == SE_5 || pSprite->lotag == SE_30_TWO_WAY_TRAIN ||
                        pSprite->lotag == SE_6_SUBWAY || pSprite->lotag == SE_14_SUBWAY_CAR)
                {
#ifdef YAX_ENABLE
                    int outerWall = -1;
#endif
                    int const startWall = sector[sectNum].wallptr;
                    int const endWall   = startWall + sector[sectNum].wallnum;

                    pSprite->extra = ((uint16_t)sector[sectNum].hitag != UINT16_MAX);

                    // TRAIN_SECTOR_TO_SE_INDEX
                    sector[sectNum].hitag = newSprite;

                    spriteNum = 0;

                    int foundWall = startWall;

                    for (; foundWall<endWall; foundWall++)
                    {
                        if (wall[ foundWall ].nextsector >= 0 &&
                                sector[ wall[ foundWall ].nextsector].hitag == 0 &&
                                (int16_t)sector[ wall[ foundWall ].nextsector].lotag < 3)
                        {
#ifdef YAX_ENABLE
                            outerWall = wall[foundWall].nextwall;
#endif
                            foundWall = wall[foundWall].nextsector;
                            spriteNum = 1;
                            break;
                        }
                    }

#ifdef YAX_ENABLE
                    pActor->t_data[9] = -1;

                    if (outerWall >= 0)
                    {
                        int upperSect = yax_vnextsec(outerWall, YAX_CEILING);

                        if (upperSect >= 0)
                        {
                            int foundEffector = headspritesect[upperSect];

                            for (; foundEffector >= 0; foundEffector = nextspritesect[foundEffector])
                                if (sprite[foundEffector].picnum == SECTOREFFECTOR && sprite[foundEffector].lotag == pSprite->lotag)
                                    break;

                            if (foundEffector < 0)
                            {
                                Sect_SetInterpolation(upperSect);
                                pActor->t_data[9] = upperSect;
                            }
                        }
                    }
#endif
                    if (spriteNum == 0)
                    {
                        Bsprintf(tempbuf,"Subway found no zero'd sectors with locators at (%d,%d).",
                            TrackerCast(pSprite->x),TrackerCast(pSprite->y));
                        G_GameExit(tempbuf);
                    }

                    pSprite->owner = -1;
                    T1(newSprite) = foundWall;

                    if (pSprite->lotag != SE_30_TWO_WAY_TRAIN)
                        T4(newSprite) = pSprite->hitag;
                }
                else if (pSprite->lotag == SE_16_REACTOR)
                    T4(newSprite) = sector[sectNum].ceilingz;
                else if (pSprite->lotag == SE_26_ESCALATOR)
                {
                    T4(newSprite)  = pSprite->x;
                    T5(newSprite)  = pSprite->y;
                    pSprite->zvel  = (pSprite->shade == sector[sectNum].floorshade) ? -256 : 256;  // UP
                    pSprite->shade = 0;
                }
                else if (pSprite->lotag == SE_2_EARTHQUAKE)
                {
                    T6(newSprite) = sector[pSprite->sectnum].floorheinum;
                    sector[pSprite->sectnum].floorheinum = 0;
                }
            }

            switch (pSprite->lotag)
            {
                case SE_6_SUBWAY:
                case SE_14_SUBWAY_CAR:
                    S_FindMusicSFX(sectNum, &spriteNum);
                    // XXX: uh.. what?
                    if (spriteNum == -1)
                        spriteNum = SUBWAY;
                    pActor->lastv.x = spriteNum;
                    fallthrough__;
                case SE_30_TWO_WAY_TRAIN:
                    if (g_netServer || numplayers > 1)
                        break;
                    fallthrough__;
                case SE_0_ROTATING_SECTOR:
                case SE_1_PIVOT:
                case SE_5:
                case SE_11_SWINGING_DOOR:
                case SE_15_SLIDING_DOOR:
                case SE_16_REACTOR:
                case SE_26_ESCALATOR: Sect_SetInterpolation(sprite[newSprite].sectnum); break;
            }

            changespritestat(newSprite, STAT_EFFECTOR);
            goto SPAWN_END;

        case SEENINE__:
        case OOZFILTER__:
            pSprite->shade = -16;
            if (pSprite->xrepeat <= 8)
            {
                pSprite->cstat   = 32768;
                pSprite->xrepeat = 0;
                pSprite->yrepeat = 0;
            }
            else pSprite->cstat = 1+256;

            pSprite->extra = g_impactDamage << 2;
            pSprite->owner = newSprite;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case CRACK1__:
        case CRACK2__:
        case CRACK3__:
        case CRACK4__:
        case FIREEXT__:
            if (pSprite->picnum == FIREEXT)
            {
                pSprite->cstat = 257;
                pSprite->extra = g_impactDamage<<2;
            }
            else
            {
                pSprite->cstat |= (pSprite->cstat & CSTAT_SPRITE_ALIGNMENT) ? 1 : (1 | CSTAT_SPRITE_ALIGNMENT_WALL);
                pSprite->extra = 1;
            }

            if ((!g_netServer && ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->pal   = 0;
            pSprite->owner = newSprite;
            pSprite->xvel  = 8;

            changespritestat(newSprite, STAT_STANDABLE);
            A_SetSprite(newSprite,CLIPMASK0);
            goto SPAWN_END;

        case LAVAPOOLBUBBLE__:
            if (!WORLDTOUR)
                break;
            if (sprite[spriteNum].xrepeat >= 30)
            {
                pSprite->owner = spriteNum;
                changespritestat(newSprite, STAT_MISC);
                pSprite->xrepeat = pSprite->yrepeat = 1;
                pSprite->x += (krand()%512)-256;
                pSprite->y += (krand()%512)-256;
            }
            goto SPAWN_END;
        case WHISPYSMOKE__:
            if (!WORLDTOUR)
                break;
            pActor->bpos.x = pSprite->x += (krand()%256)-128;
            pActor->bpos.y = pSprite->y += (krand()%256)-128;
            pSprite->xrepeat = pSprite->yrepeat = 20;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;
        case FIREFLYFLYINGEFFECT__:
            if (!WORLDTOUR)
                break;
            pSprite->owner = spriteNum;
            changespritestat(newSprite, STAT_MISC);
            pSprite->xrepeat = pSprite->yrepeat = 1;
            goto SPAWN_END;
        case E32_TILE5846__:
            if (!WORLDTOUR)
                break;
            pSprite->extra = 150;
            pSprite->cstat |= 257;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        default:
            break; // NOT goto
        }

    // implementation of the default case
    if (G_TileHasActor(pSprite->picnum))
    {
        if (spriteNum == -1 && pSprite->lotag > ud.player_skill)
        {
            pSprite->xrepeat = pSprite->yrepeat = 0;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;
        }

        //  Init the size
        if (pSprite->xrepeat == 0 || pSprite->yrepeat == 0)
            pSprite->xrepeat = pSprite->yrepeat = 1;

        if (A_CheckSpriteFlags(newSprite, SFLAG_BADGUY))
        {
            if (ud.monsters_off == 1)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            A_Fall(newSprite);

            if (A_CheckSpriteFlags(newSprite, SFLAG_BADGUYSTAYPUT))
                pActor->stayput = pSprite->sectnum;

            g_player[myconnectindex].ps->max_actors_killed++;
            pSprite->clipdist = 80;

            if (spriteNum >= 0)
            {
                if (sprite[spriteNum].picnum == RESPAWN)
                    pActor->tempang = sprite[newSprite].pal = sprite[spriteNum].pal;

                A_PlayAlertSound(newSprite);
                changespritestat(newSprite, STAT_ACTOR);
            }
            else
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
        }
        else
        {
            pSprite->clipdist = 40;
            pSprite->owner    = newSprite;
            changespritestat(newSprite, STAT_ACTOR);
        }

        pActor->timetosleep = 0;

        if (spriteNum >= 0)
            pSprite->ang = sprite[spriteNum].ang;
    }

SPAWN_END:
    if (VM_HaveEvent(EVENT_SPAWN))
    {
        int32_t p;
        int32_t pl=A_FindPlayer(&sprite[newSprite],&p);
        VM_ExecuteEvent(EVENT_SPAWN,newSprite, pl, p);
    }

    return newSprite;
}

static int G_MaybeTakeOnFloorPal(tspriteptr_t pSprite, int sectNum)
{
    int const floorPal = sector[sectNum].floorpal;

    if (floorPal && !g_noFloorPal[floorPal] && !A_CheckSpriteFlags(pSprite->owner, SFLAG_NOPAL))
    {
        pSprite->pal = floorPal;
        return 1;
    }

    return 0;
}

template <int rotations>
static int getofs_viewtype(int angDiff)
{
    return ((((angDiff + 3072) & 2047) * rotations + 1024) >> 11) % rotations;
}

template <int rotations>
static int viewtype_mirror(uint16_t & cstat, int frameOffset)
{
    if (frameOffset > rotations / 2)
    {
        cstat |= 4;
        return rotations - frameOffset;
    }

    cstat &= ~4;
    return frameOffset;
}

template <int mirrored_rotations>
static int getofs_viewtype_mirrored(uint16_t & cstat, int angDiff)
{
    return viewtype_mirror<mirrored_rotations*2-2>(cstat, getofs_viewtype<mirrored_rotations*2-2>(angDiff));
}

// XXX: this fucking sucks and needs to be replaced with a SFLAG
#ifndef EDUKE32_STANDALONE
static int G_CheckAdultTile(uint16_t tileNum)
{
    switch (tileGetMapping(tileNum))
    {
        case FEM1__:
        case FEM2__:
        case FEM3__:
        case FEM4__:
        case FEM5__:
        case FEM6__:
        case FEM7__:
        case FEM8__:
        case FEM9__:
        case FEM10__:
        case MAN__:
        case MAN2__:
        case WOMAN__:
        case NAKED1__:
        case PODFEM1__:
        case FEMMAG1__:
        case FEMMAG2__:
        case FEMPIC1__:
        case FEMPIC2__:
        case FEMPIC3__:
        case FEMPIC4__:
        case FEMPIC5__:
        case FEMPIC6__:
        case FEMPIC7__:
        case BLOODYPOLE__:
        case FEM6PAD__:
        case STATUE__:
        case STATUEFLASH__:
        case OOZ__:
        case OOZ2__:
        case WALLBLOOD1__:
        case WALLBLOOD2__:
        case WALLBLOOD3__:
        case WALLBLOOD4__:
        case WALLBLOOD5__:
        case WALLBLOOD7__:
        case WALLBLOOD8__:
        case SUSHIPLATE1__:
        case SUSHIPLATE2__:
        case SUSHIPLATE3__:
        case SUSHIPLATE4__:
        case FETUS__:
        case FETUSJIB__:
        case FETUSBROKE__:
        case HOTMEAT__:
        case FOODOBJECT16__:
        case DOLPHIN1__:
        case DOLPHIN2__:
        case TOUGHGAL__:
        case TAMPON__:
        case XXXSTACY__:
        case 4946:
        case 4947:
        case 693:
        case 2254:
        case 4560:
        case 4561:
        case 4562:
        case 4498:
        case 4957:
            return 1;
    }
    return 0;
}
#endif

static inline void G_DoEventAnimSprites(int tspriteNum)
{
    int const tsprOwner = tsprite[tspriteNum].owner;

    if ((((unsigned)tsprOwner >= MAXSPRITES || (spriteext[tsprOwner].flags & SPREXT_TSPRACCESS) != SPREXT_TSPRACCESS))
        && tsprite[tspriteNum].statnum != TSPR_TEMP)
        return;

    spriteext[tsprOwner].tspr = &tsprite[tspriteNum];
    VM_ExecuteEvent(EVENT_ANIMATESPRITES, tsprOwner, screenpeek);
    spriteext[tsprOwner].tspr = NULL;
}

void G_DoSpriteAnimations(int32_t ourx, int32_t oury, int32_t ourz, int32_t oura, int32_t smoothratio)
{
    MICROPROFILE_SCOPEI("Game", EDUKE32_FUNCTION, MP_YELLOWGREEN);

    UNREFERENCED_PARAMETER(ourz);
    int32_t j, frameOffset, playerNum;
    intptr_t l;

#ifdef LEGACY_ROR
    ror_sprite = -1;
#endif

    if (spritesortcnt == 0)
    {
#if 1//def DEBUGGINGAIDS
        g_spriteStat.numonscreen = 0;
#endif
        return;
    }

    for (j=spritesortcnt-1; j>=0; j--)
    {
        auto const t = &tsprite[j];
        const int32_t i = t->owner;
        auto const s = &sprite[i];

        Duke_ApplySpritePropertiesToTSprite(t, (uspriteptr_t)s);

        switch (tileGetMapping(s->picnum))
        {
        case SECTOREFFECTOR__:
            if (s->lotag == 40 || s->lotag == 41)
            {
                t->cstat = 32768;
#ifdef LEGACY_ROR
                if (ror_sprite == -1)
                    ror_sprite = i;
#endif
            }

            if (t->lotag == SE_27_DEMO_CAM && ud.recstat == 1)
            {
                t->picnum = 11+(((int) totalclock>>3)&1);
                t->cstat |= 128;
            }
            else
                t->xrepeat = t->yrepeat = 0;
            break;
        }
    }

    for (j=spritesortcnt-1; j>=0; j--)
    {
        auto const t = &tsprite[j];
        const int32_t i = t->owner;
        auto const s = (uspriteptr_t)&sprite[i];

        if (t->picnum < GREENSLIME || t->picnum > GREENSLIME+7)
            switch (tileGetMapping(t->picnum))
            {
            case BLOODPOOL__:
            case PUKE__:
            case FOOTPRINTS__:
            case FOOTPRINTS2__:
            case FOOTPRINTS3__:
            case FOOTPRINTS4__:
                if (t->shade == 127) continue;
                break;
            case RESPAWNMARKERRED__:
            case RESPAWNMARKERYELLOW__:
            case RESPAWNMARKERGREEN__:
                if (ud.marker == 0)
                    t->xrepeat = t->yrepeat = 0;
                continue;
            case CHAIR3__:
                if (tilehasmodelorvoxel(t->picnum,t->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
                {
                    t->cstat &= ~4;
                    break;
                }
                frameOffset = getofs_viewtype_mirrored<5>(t->cstat, t->ang - oura);
                t->picnum = s->picnum+frameOffset;
                break;
            case BLOODSPLAT1__:
            case BLOODSPLAT2__:
            case BLOODSPLAT3__:
            case BLOODSPLAT4__:
                if (ud.lockout) t->xrepeat = t->yrepeat = 0;
                else if (t->pal == 6)
                {
                    t->shade = -127;
                    continue;
                }
                fallthrough__;
            case BULLETHOLE__:
            case CRACK1__:
            case CRACK2__:
            case CRACK3__:
            case CRACK4__:
                t->shade = 16;
                continue;
            case NEON1__:
            case NEON2__:
            case NEON3__:
            case NEON4__:
            case NEON5__:
            case NEON6__:
                continue;
            default:
                // NOTE: wall-aligned sprites will never take on ceiling/floor shade...
                if ((t->cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_WALL || (A_CheckEnemySprite(t) &&
                    (unsigned)t->owner < MAXSPRITES && sprite[t->owner].extra > 0) || t->statnum == STAT_PLAYER)
                    continue;
            }

        // ... since this is not reached:
        if (A_CheckSpriteFlags(t->owner, SFLAG_NOSHADE) || (t->cstat&CSTAT_SPRITE_NOSHADE))
            l = sprite[t->owner].shade;
        else
        {
            if (sector[t->sectnum].ceilingstat&1)
                l = sector[t->sectnum].ceilingshade;
            else
                l = sector[t->sectnum].floorshade;

            if (l < -127)
                l = -127;
        }

        t->shade = l;
    }

    for (j=spritesortcnt-1; j>=0; j--) //Between drawrooms() and drawmasks()
    {
        int32_t switchpic;
        int32_t curframe;
        int32_t scrofs_action;
        //is the perfect time to animate sprites
        auto const t = &tsprite[j];
        const int32_t i = t->owner;
        // XXX: what's up with the (i < 0) check?
        // NOTE: not const spritetype because set at SET_SPRITE_NOT_TSPRITE (see below).
        EDUKE32_STATIC_ASSERT(sizeof(uspritetype) == sizeof(tspritetype)); // see TSPRITE_SIZE
        auto const pSprite = (i < 0) ? (uspriteptr_t)&tsprite[j] : (uspriteptr_t)&sprite[i];

#ifndef EDUKE32_STANDALONE
        if (ud.lockout && G_CheckAdultTile(pSprite->picnum))
        {
            t->xrepeat = t->yrepeat = 0;
            continue;
        }

        if (pSprite->picnum == NATURALLIGHTNING)
        {
            t->shade = -127;
            t->clipdist |= TSPR_FLAGS_NO_SHADOW;
        }
#endif
        if (t->statnum == TSPR_TEMP)
            continue;

        Bassert(i >= 0);

        auto const ps = (pSprite->statnum != STAT_ACTOR && pSprite->picnum == APLAYER && pSprite->owner >= 0) ? g_player[P_GetP(pSprite)].ps : NULL;
        if (ps && ps->newowner == -1)
        {
            t->x -= mulscale16(65536-smoothratio,ps->pos.x-ps->opos.x);
            t->y -= mulscale16(65536-smoothratio,ps->pos.y-ps->opos.y);
            // dirty hack
            if (ps->dead_flag || sprite[ps->i].extra <= 0) t->z = ps->opos.z;
            t->z += mulscale16(smoothratio,ps->pos.z-ps->opos.z) -
                ((ps->dead_flag || sprite[ps->i].extra <= 0) ? 0 : ps->spritezoffset) + ps->spritezoffset;
        }
        else if (pSprite->picnum != CRANEPOLE)
        {
            t->x -= mulscale16(65536-smoothratio,pSprite->x-actor[i].bpos.x);
            t->y -= mulscale16(65536-smoothratio,pSprite->y-actor[i].bpos.y);
            t->z -= mulscale16(65536-smoothratio,pSprite->z-actor[i].bpos.z);
        }

        const int32_t sect = pSprite->sectnum;

        curframe = AC_CURFRAME(actor[i].t_data);
        scrofs_action = AC_ACTION_ID(actor[i].t_data);
        switchpic = pSprite->picnum;
        // Some special cases because dynamictostatic system can't handle
        // addition to constants.
        if ((pSprite->picnum >= SCRAP6) && (pSprite->picnum<=SCRAP6+7))
            switchpic = SCRAP5;
        else if ((pSprite->picnum==MONEY+1) || (pSprite->picnum==MAIL+1) || (pSprite->picnum==PAPER+1))
            switchpic--;

        switch (tileGetMapping(switchpic))
        {
#ifndef EDUKE32_STANDALONE
        case DUKELYINGDEAD__:
            t->z += (24<<8);
            break;
        case BLOODPOOL__:
        case FOOTPRINTS__:
        case FOOTPRINTS2__:
        case FOOTPRINTS3__:
        case FOOTPRINTS4__:
            if (t->pal == 6)
                t->shade = -127;
            fallthrough__;
        case PUKE__:
        case MONEY__:
            //case MONEY+1__:
        case MAIL__:
            //case MAIL+1__:
        case PAPER__:
            //case PAPER+1__:
            if (ud.lockout && pSprite->pal == 2)
            {
                t->xrepeat = t->yrepeat = 0;
                continue;
            }
            break;
        case TRIPBOMB__:
            continue;
        case FORCESPHERE__:
            if (t->statnum == STAT_MISC)
            {
                int16_t const sqa = getangle(sprite[pSprite->owner].x - g_player[screenpeek].ps->pos.x,
                                       sprite[pSprite->owner].y - g_player[screenpeek].ps->pos.y);
                int16_t const sqb = getangle(sprite[pSprite->owner].x - t->x, sprite[pSprite->owner].y - t->y);

                if (klabs(G_GetAngleDelta(sqa,sqb)) > 512)
                    if (ldist(&sprite[pSprite->owner],t) < ldist(&sprite[g_player[screenpeek].ps->i],&sprite[pSprite->owner]))
                        t->xrepeat = t->yrepeat = 0;
            }
            continue;
        case BURNING__:
        case BURNING2__:
            if (sprite[pSprite->owner].statnum == STAT_PLAYER)
            {
                int const playerNum = P_Get(pSprite->owner);

                if (display_mirror == 0 && playerNum == screenpeek && g_player[playerNum].ps->over_shoulder_on == 0)
                    t->xrepeat = 0;
                else
                {
                    t->ang = getangle(ourx - t->x, oury - t->y);
                    t->x   = sprite[pSprite->owner].x + (sintable[(t->ang + 512) & 2047] >> 10);
                    t->y   = sprite[pSprite->owner].y + (sintable[t->ang & 2047] >> 10);
                }
            }
            break;

        case ATOMICHEALTH__:
            t->z -= ZOFFSET6;
            break;
        case CRYSTALAMMO__:
            t->shade = (sintable[((int32_t) totalclock<<4)&2047]>>10);
            continue;
#endif
        case VIEWSCREEN__:
        case VIEWSCREEN2__:
        {
            // invalid index, skip applying a tile
            if (T2(i) < 0 || T2(i) >= MAX_ACTIVE_VIEWSCREENS)
                break;

            uint16_t const viewscrTile = g_activeVscrTile[T2(i)];
            int const viewscrShift = G_GetViewscreenSizeShift(t);

            if (actor[OW(i)].t_data[0] == 1)
            {
                t->picnum = STATIC;
                t->cstat |= (wrand()&12);
                t->xrepeat += 10;
                t->yrepeat += 9;
            }
            else if (viewscrTile < MAXTILES && display_mirror != 3 && waloff[viewscrTile])
            {
                // this exposes a sprite sorting issue which needs to be debugged further...
#if 0
                if (spritesortcnt < maxspritesonscreen)
                {
                    auto const newt = &tsprite[spritesortcnt++];

                    *newt = *t;

                    newt->cstat |= 2|512;
                    newt->x += (sintable[(newt->ang+512)&2047]>>12);
                    newt->y += (sintable[newt->ang&2047]>>12);
                    updatesector(newt->x, newt->y, &newt->sectnum);
                }
#endif
                t->picnum = viewscrTile;
#if VIEWSCREENFACTOR > 0
                t->xrepeat >>= viewscrShift;
                t->yrepeat >>= viewscrShift;
#endif
            }

            break;
        }
#ifndef EDUKE32_STANDALONE
        case SHRINKSPARK__:
            t->picnum = SHRINKSPARK+(((int32_t) totalclock>>4)&3);
            break;
        case GROWSPARK__:
            t->picnum = GROWSPARK+(((int32_t) totalclock>>4)&3);
            break;
        case RPG__:
            if (tilehasmodelorvoxel(t->picnum,t->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
            {
                int32_t v = getangle(t->xvel, t->zvel>>4);

                spriteext[i].mdpitch = (v > 1023 ? v-2048 : v);
                t->cstat &= ~4;
                break;
            }
            frameOffset = getofs_viewtype_mirrored<7>(t->cstat, pSprite->ang - getangle(pSprite->x-ourx, pSprite->y-oury));
            t->picnum = RPG+frameOffset;
            break;

        case RECON__:
            if (tilehasmodelorvoxel(t->picnum,t->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
            {
                t->cstat &= ~4;
                break;
            }
            frameOffset = getofs_viewtype_mirrored<7>(t->cstat, pSprite->ang - getangle(pSprite->x-ourx, pSprite->y-oury));

            // RECON_T4
            if (klabs(curframe) > 64)
                frameOffset += 7;  // tilted recon car

            t->picnum = RECON+frameOffset;

            break;
#endif
        case APLAYER__:
            playerNum = P_GetP(pSprite);

            if (t->pal == 1)
                t->z -= ZOFFSET7;

            if (g_player[playerNum].ps->over_shoulder_on > 0 && g_player[playerNum].ps->newowner < 0)
            {
                t->ang = fix16_to_int(
                g_player[playerNum].ps->q16ang
                + mulscale16((((g_player[playerNum].ps->q16ang + 1024 - g_player[playerNum].ps->oq16ang) & 2047) - 1024), smoothratio));
                if (tilehasmodelorvoxel(t->picnum, t->pal))
                {
                    static int32_t targetang = 0;
                    uint32_t const extBits = g_player[playerNum].input.extbits;

                    if (extBits&BIT(EK_MOVE_BACKWARD))
                    {
                        if (extBits&BIT(EK_STRAFE_LEFT)) targetang += 16;
                        else if (extBits&BIT(EK_STRAFE_RIGHT)) targetang -= 16;
                        else if (targetang > 0) targetang -= targetang>>2;
                        else if (targetang < 0) targetang += (-targetang)>>2;
                    }
                    else
                    {
                        if (extBits&BIT(EK_STRAFE_LEFT)) targetang -= 16;
                        else if (extBits&BIT(EK_STRAFE_RIGHT)) targetang += 16;
                        else if (targetang > 0) targetang -= targetang>>2;
                        else if (targetang < 0) targetang += (-targetang)>>2;
                    }

                    targetang = clamp(targetang, -128, 128);
                    t->ang += targetang;
                }
                else if (!display_mirror)
                    t->cstat |= 2;
            }

            if ((g_netServer || ud.multimode > 1) && (display_mirror || screenpeek != playerNum || pSprite->owner == -1))
            {
                if (ud.showweapons && sprite[g_player[playerNum].ps->i].extra > 0 && g_player[playerNum].ps->curr_weapon > 0
                        && spritesortcnt < maxspritesonscreen)
                {
                    auto const newTspr       = &tsprite[spritesortcnt];
                    int const  currentWeapon = g_player[playerNum].ps->curr_weapon;

                    *newTspr         = *t;
                    newTspr->statnum = TSPR_TEMP;
                    newTspr->cstat   = 0;
                    newTspr->pal     = 0;
                    newTspr->picnum  = (currentWeapon == GROW_WEAPON ? GROWSPRITEICON : WeaponPickupSprites[currentWeapon]);
                    newTspr->z       = (pSprite->owner >= 0) ? g_player[playerNum].ps->pos.z - ZOFFSET4 : pSprite->z - (51 << 8);
                    newTspr->xrepeat = (newTspr->picnum == HEAVYHBOMB) ? 10 : 16;
                    newTspr->yrepeat = newTspr->xrepeat;

                    spritesortcnt++;
                }

                if (g_player[playerNum].input.extbits & BIT(EK_CHAT_MODE) && !ud.pause_on && spritesortcnt < maxspritesonscreen)
                {
                    auto const playerTyping = &tsprite[spritesortcnt];

                    *playerTyping = *t;
                    playerTyping->statnum = TSPR_TEMP;
                    playerTyping->cstat   = 0;
                    playerTyping->picnum  = RESPAWNMARKERGREEN;
                    playerTyping->z       = (pSprite->owner >= 0) ? (g_player[playerNum].ps->pos.z - (20 << 8)) : (pSprite->z - (96 << 8));
                    playerTyping->xrepeat = 32;
                    playerTyping->yrepeat = 32;
                    playerTyping->pal     = 20;

                    spritesortcnt++;
                }
            }

            if (pSprite->owner == -1)
            {
                if (tilehasmodelorvoxel(pSprite->picnum,t->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
                {
                    frameOffset = 0;
                    t->cstat &= ~4;
                }
                else
                    frameOffset = getofs_viewtype_mirrored<5>(t->cstat, pSprite->ang - oura);

                if (sector[pSprite->sectnum].lotag == ST_2_UNDERWATER) frameOffset += 1795-1405;
                else if ((actor[i].floorz-pSprite->z) > (64<<8)) frameOffset += 60;

                t->picnum += frameOffset;
                t->pal = g_player[playerNum].ps->palookup;

                goto PALONLY;
            }

            if (g_player[playerNum].ps->on_crane == -1 && (sector[pSprite->sectnum].lotag&0x7ff) != 1)  // ST_1_ABOVE_WATER ?
            {
                l = pSprite->z-actor[g_player[playerNum].ps->i].floorz+(3<<8);
                // SET_SPRITE_NOT_TSPRITE
                if (l > 1024 && pSprite->yrepeat > 32 && pSprite->extra > 0)
                    t->yoffset = (int8_t)tabledivide32_noinline(l, pSprite->yrepeat<<2);
                else t->yoffset=0;
            }

#ifndef EDUKE32_STANDALONE
            if (!FURY && g_player[playerNum].ps->newowner > -1)
            {
                // Display APLAYER sprites with action PSTAND when viewed through
                // a camera.  Not implemented for Lunatic.
                const intptr_t *aplayer_scr = g_tile[APLAYER].execPtr;
                // [0]=strength, [1]=actionofs, [2]=moveofs

                scrofs_action = aplayer_scr[1];
                curframe = 0;
            }
#endif
            if (ud.camerasprite == -1 && g_player[playerNum].ps->newowner == -1)
            {
                if (pSprite->owner >= 0 && display_mirror == 0 && g_player[playerNum].ps->over_shoulder_on == 0)
                {
                    if ((!g_netServer && ud.multimode < 2) || ((g_netServer || ud.multimode > 1) && playerNum == screenpeek))
                    {
                        if (videoGetRenderMode() == REND_POLYMER)
                            t->clipdist |= TSPR_FLAGS_INVISIBLE_WITH_SHADOW;
                        else
                        {
                            t->owner = -1;
                            t->xrepeat = t->yrepeat = 0;
                            continue;
                        }

                        if (tilehasmodelorvoxel(pSprite->picnum, t->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
                        {
                            frameOffset = 0;
                            t->cstat &= ~4;
                        }
                        else
                            frameOffset = getofs_viewtype_mirrored<5>(t->cstat, pSprite->ang - oura);

                        if (sector[t->sectnum].lotag == ST_2_UNDERWATER) frameOffset += 1795-1405;
                        else if ((actor[i].floorz-pSprite->z) > (64<<8)) frameOffset += 60;

                        t->picnum += frameOffset;
                        t->pal = g_player[playerNum].ps->palookup;
                    }
                }
            }
PALONLY:
            G_MaybeTakeOnFloorPal(t, sect);

            if (pSprite->owner == -1) continue;

            if (t->z > actor[i].floorz && t->xrepeat < 32)
                t->z = actor[i].floorz;

            break;
#ifndef EDUKE32_STANDALONE
        case JIBS1__:
        case JIBS2__:
        case JIBS3__:
        case JIBS4__:
        case JIBS5__:
        case JIBS6__:
        case HEADJIB1__:
        case LEGJIB1__:
        case ARMJIB1__:
        case LIZMANHEAD1__:
        case LIZMANARM1__:
        case LIZMANLEG1__:
        case DUKELEG__:
        case DUKEGUN__:
        case DUKETORSO__:
            if (ud.lockout)
            {
                t->xrepeat = t->yrepeat = 0;
                continue;
            }
            if (t->pal == 6)
                t->shade = -120;
            fallthrough__;
        case SCRAP1__:
        case SCRAP2__:
        case SCRAP3__:
        case SCRAP4__:
        case SCRAP5__:
            if (actor[i].htpicnum == BLIMP && t->picnum == SCRAP1 && pSprite->yvel >= 0)
                t->picnum = pSprite->yvel < MAXUSERTILES ? pSprite->yvel : 0;
            else t->picnum += T1(i);
            t->shade = -128+6 < t->shade ? t->shade-6 : -128; // effectively max(t->shade-6, -128) while avoiding (signed!) underflow

            G_MaybeTakeOnFloorPal(t, sect);
            break;
        case WATERBUBBLE__:
            if (sector[t->sectnum].floorpicnum == FLOORSLIME)
            {
                t->pal = 7;
                break;
            }
            fallthrough__;
#endif
        default:
            G_MaybeTakeOnFloorPal(t, sect);
            break;
        }

        if (G_TileHasActor(pSprite->picnum))
        {
            if ((unsigned)scrofs_action + ACTION_PARAM_COUNT > (unsigned)g_scriptSize || apScript[scrofs_action + ACTION_PARAM_COUNT] != CON_END)
            {
                if (scrofs_action)
                    LOG_F(ERROR, "Sprite %d tile %d: invalid action at offset %d", i, pSprite->picnum, scrofs_action);

                goto skip;
            }

            int32_t viewtype = apScript[scrofs_action + ACTION_VIEWTYPE];
            uint16_t const action_flags = apScript[scrofs_action + ACTION_FLAGS];

            int const invertp = viewtype < 0;
            l = klabs(viewtype);

            if (tilehasmodelorvoxel(pSprite->picnum,t->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
            {
                frameOffset = 0;
                t->cstat &= ~4;
            }
            else
            {
                int const viewAng = ((l > 4 && l != 8) || action_flags & AF_VIEWPOINT) ? getangle(pSprite->x-ourx, pSprite->y-oury) : oura;
                int const angDiff = invertp ? viewAng - pSprite->ang : pSprite->ang - viewAng;

                switch (l)
                {
                case 2:
                    frameOffset = getofs_viewtype<8>(angDiff) & 1;
                    break;

                case 3:
                case 4:
                    frameOffset = viewtype_mirror<7>(t->cstat, getofs_viewtype<16>(angDiff) & 7);
                    break;

                case 5:
                    frameOffset = getofs_viewtype_mirrored<5>(t->cstat, angDiff);
                    break;
                case 7:
                    frameOffset = getofs_viewtype_mirrored<7>(t->cstat, angDiff);
                    break;
                case 8:
                    frameOffset = getofs_viewtype<8>(angDiff);
                    t->cstat &= ~4;
                    break;
                case 9:
                    frameOffset = getofs_viewtype_mirrored<9>(t->cstat, angDiff);
                    break;
                case 12:
                    frameOffset = getofs_viewtype<12>(angDiff);
                    t->cstat &= ~4;
                    break;
                case 16:
                    frameOffset = getofs_viewtype<16>(angDiff);
                    t->cstat &= ~4;
                    break;
                default:
                    frameOffset = 0;
                    break;
                }
            }

            t->picnum += frameOffset + apScript[scrofs_action + ACTION_STARTFRAME] + viewtype*curframe;
            // XXX: t->picnum can be out-of-bounds by bad user code.

            Bassert((unsigned)t->picnum < MAXTILES);

            if (viewtype > 0)
                while (tilesiz[t->picnum].x == 0 && t->picnum > 0 && t->picnum < MAXTILES)
                    t->picnum -= l;       //Hack, for actors

            if (actor[i].dispicnum < MAXTILES)
                actor[i].dispicnum = t->picnum;
        }
//        else if (display_mirror == 1)
//            t->cstat |= 4;
        /* completemirror() already reverses the drawn frame, so the above isn't necessary.
         * Even Polymost's and Polymer's mirror seems to function correctly this way. */

skip:
        // Night vision goggles tsprite tinting.
        // XXX: Currently, for the splitscreen mod, sprites will be pal6-colored iff the first
        // player has nightvision on.  We should pass stuff like "from which player is this view
        // supposed to be" as parameters ("drawing context") instead of relying on globals.
        if (g_player[screenpeek].ps->inv_amount[GET_HEATS] > 0 && g_player[screenpeek].ps->heat_on &&
                (A_CheckEnemySprite(pSprite) || A_CheckSpriteFlags(t->owner,SFLAG_NVG) || pSprite->picnum == APLAYER || pSprite->statnum == STAT_DUMMYPLAYER))
        {
            t->pal = 6;
            t->shade = 0;
        }

        // Fake floor shadow, implemented by inserting a new tsprite.
        if (pSprite->statnum == STAT_DUMMYPLAYER || A_CheckEnemySprite(pSprite) || A_CheckSpriteFlags(t->owner,SFLAG_SHADOW) || (pSprite->picnum == APLAYER && pSprite->owner >= 0))
            if (t->statnum != TSPR_TEMP && pSprite->picnum != EXPLOSION2 && pSprite->picnum != HANGLIGHT && pSprite->picnum != DOMELITE && pSprite->picnum != HOTMEAT)
            {
                if (actor[i].dispicnum >= MAXTILES)
                {
#ifdef DEBUGGINGAIDS
                    // A negative actor[i].dispicnum used to mean 'no floor shadow please', but
                    // that was a bad hack since the value could propagate to sprite[].picnum.
                    LOG_F(ERROR, "actor[%d].dispicnum = %d", i, actor[i].dispicnum);
#endif
                    actor[i].dispicnum=0;
                    continue;
                }

                if (actor[i].flags & SFLAG_NOFLOORSHADOW)
                    continue;

                if (ud.shadows && spritesortcnt < (maxspritesonscreen-2)
#ifdef POLYMER
                    && !(videoGetRenderMode() == REND_POLYMER && pr_lighting != 0)
#endif
                    )
                {
                    int const shadowZ = ((sector[sect].lotag & 0xff) > 2 || pSprite->statnum == STAT_PROJECTILE ||
                                   pSprite->statnum == STAT_MISC || pSprite->picnum == DRONE || pSprite->picnum == COMMANDER)
#ifdef YAX_ENABLE
                                  ? yax_getflorzofslope(sect, pSprite->xy)
#else
                                  ? getflorzofslope(sect, pSprite->x, pSprite->y)
#endif
                                  : actor[i].floorz;

                    if ((pSprite->z-shadowZ) < ZOFFSET3 && g_player[screenpeek].ps->pos.z < shadowZ)
                    {
                        tspriteptr_t tsprShadow = &tsprite[spritesortcnt];

                        *tsprShadow         = *t;
                        tsprShadow->statnum = TSPR_TEMP;
                        tsprShadow->yrepeat = (t->yrepeat >> 3);

                        if (t->yrepeat < 4)
                            t->yrepeat = 4;

                        tsprShadow->shade   = 127;
                        tsprShadow->cstat  |= 2;
                        tsprShadow->z       = shadowZ;
                        tsprShadow->pal     = ud.shadow_pal;

#ifdef USE_OPENGL
                        if (videoGetRenderMode() >= REND_POLYMOST)
                        {
                            tsprShadow->clipdist |= TSPR_FLAGS_NO_GLOW;
                            if (tilehasmodelorvoxel(t->picnum,t->pal))
                            {
                                tsprShadow->yrepeat = 0;
                                // 512:trans reverse
                                //1024:tell MD2SPRITE.C to use Z-buffer hacks to hide overdraw issues
                                tsprShadow->clipdist |= TSPR_FLAGS_MDHACK;
                                tsprShadow->cstat |= 512;
                            }
                            else
                            {
                                int const camang = display_mirror ? ((2048 - fix16_to_int(CAMERA(q16ang))) & 2047) : fix16_to_int(CAMERA(q16ang));
                                vec2_t const ofs = { sintable[(camang+512)&2047]>>11, sintable[(camang)&2047]>>11};

                                tsprShadow->x += ofs.x;
                                tsprShadow->y += ofs.y;
                            }
                        }
#endif
                        spritesortcnt++;
                    }
                }
            }

        bool const haveAction = scrofs_action != 0 && (unsigned)scrofs_action + ACTION_PARAM_COUNT <= (unsigned)g_scriptSize;

        switch (tileGetMapping(pSprite->picnum))
        {
#ifndef EDUKE32_STANDALONE
        case LASERLINE__:
            if (sector[t->sectnum].lotag == ST_2_UNDERWATER) t->pal = 8;
            t->z = sprite[pSprite->owner].z-(3<<8);
            if (g_tripbombLaserMode == 2 && g_player[screenpeek].ps->heat_on == 0)
                t->yrepeat = 0;
            fallthrough__;
        case EXPLOSION2BOT__:
        case FREEZEBLAST__:
        case ATOMICHEALTH__:
        case FIRELASER__:
        case SHRINKSPARK__:
        case GROWSPARK__:
        case CHAINGUN__:
        case SHRINKEREXPLOSION__:
        case RPG__:
        case FLOORFLAME__:
#endif
        case EXPLOSION2__:
            if (t->picnum == EXPLOSION2)
            {
                g_player[screenpeek].ps->visibility = -127;
                //g_restorePalette = 1;   // JBF 20040101: why?
            }
            t->shade = -127;
            t->clipdist |= TSPR_FLAGS_DRAW_LAST | TSPR_FLAGS_NO_SHADOW;
            break;
#ifndef EDUKE32_STANDALONE
        case FIRE__:
        case FIRE2__:
            t->cstat |= 128;
            fallthrough__;
        case BURNING__:
        case BURNING2__:
            if (sprite[pSprite->owner].picnum != TREE1 && sprite[pSprite->owner].picnum != TREE2)
                t->z = actor[t->owner].floorz;
            t->shade = -127;
            fallthrough__;
        case SMALLSMOKE__:
            t->clipdist |= TSPR_FLAGS_DRAW_LAST | TSPR_FLAGS_NO_SHADOW;
            break;
        case COOLEXPLOSION1__:
            t->shade = -127;
            t->clipdist |= TSPR_FLAGS_DRAW_LAST | TSPR_FLAGS_NO_SHADOW;
            t->picnum += (pSprite->shade>>1);
            break;
        case PLAYERONWATER__:
            t->shade = sprite[pSprite->owner].shade;
            if (haveAction)
                break;
            if (tilehasmodelorvoxel(pSprite->picnum,pSprite->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
            {
                frameOffset = 0;
                t->cstat &= ~4;
            }
            else
                frameOffset = getofs_viewtype_mirrored<5>(t->cstat, t->ang - oura);

            t->picnum = pSprite->picnum+frameOffset+((T1(i)<4)*5);
            break;

        case WATERSPLASH2__:
            // WATERSPLASH_T2
            t->picnum = WATERSPLASH2+T2(i);
            break;
        case SHELL__:
            t->picnum = pSprite->picnum+(T1(i)&1);
            fallthrough__;
        case SHOTGUNSHELL__:
            t->cstat |= 12;
            if (T1(i) > 2) t->cstat &= ~12;
            else if (T1(i) > 1) t->cstat &= ~4;
            break;
        case FRAMEEFFECT1_13__:
            if (PLUTOPAK) break;
            fallthrough__;
#endif
        case FRAMEEFFECT1__:
            if (pSprite->owner >= 0 && sprite[pSprite->owner].statnum < MAXSTATUS)
            {
                if (sprite[pSprite->owner].picnum == APLAYER)
                    if (ud.camerasprite == -1)
                        if (screenpeek == P_Get(pSprite->owner) && display_mirror == 0)
                        {
                            t->owner = -1;
                            break;
                        }
                if ((sprite[pSprite->owner].cstat&32768) == 0)
                {
                    if (!actor[pSprite->owner].dispicnum)
                        t->picnum = actor[i].t_data[1];
                    else t->picnum = actor[pSprite->owner].dispicnum;

                    if (!G_MaybeTakeOnFloorPal(t, sect))
                        t->pal = sprite[pSprite->owner].pal;

                    t->shade = sprite[pSprite->owner].shade;
                    t->ang = sprite[pSprite->owner].ang;
                    t->cstat = 2|sprite[pSprite->owner].cstat;
                }
            }
            break;

        case CAMERA1__:
        case RAT__:
            if (haveAction)
                break;
            if (tilehasmodelorvoxel(pSprite->picnum,pSprite->pal) && !(spriteext[i].flags&SPREXT_NOTMD))
            {
                t->cstat &= ~4;
                break;
            }
            frameOffset = getofs_viewtype_mirrored<5>(t->cstat, t->ang - oura);
            t->picnum = pSprite->picnum+frameOffset;
            break;
        }

        actor[i].dispicnum = t->picnum;
#if 0
        // why?
        if (sector[t->sectnum].floorpicnum == MIRROR)
            t->xrepeat = t->yrepeat = 0;
#endif
    }

    if (VM_HaveEvent(EVENT_ANIMATESPRITES))
    {
        for (j = spritesortcnt-1; j>=0; j--)
            G_DoEventAnimSprites(j);
    }

#if 1//def DEBUGGINGAIDS
    g_spriteStat.numonscreen = spritesortcnt;
#endif
}

void G_SetViewportShrink(int32_t dir)
{
    if (dir!=0)
    {
        if (dir > 0) // shrinking
        {
            if (ud.screen_size < 4 && (!(ud.statusbarflags & STATUSBAR_NOMINI) || !(ud.statusbarflags & STATUSBAR_NOMODERN)))
                ud.screen_size = 4;
            else if (ud.screen_size == 4 && ud.althud == 1 && !(ud.statusbarflags & STATUSBAR_NOMINI))
                ud.althud = 0;
            else if (ud.screen_size == 4 && ud.statusbarcustom < ud.statusbarrange && !(ud.statusbarflags & STATUSBAR_NOMINI))
                ud.statusbarcustom += 1;
            else if (ud.screen_size < 8 && (!(ud.statusbarflags & STATUSBAR_NOFULL) || !(ud.statusbarflags & STATUSBAR_NOOVERLAY)))
                ud.screen_size = 8;
            else if (ud.screen_size == 8 && ud.statusbarmode == 1 && !(ud.statusbarflags & STATUSBAR_NOFULL))
                ud.statusbarmode = 0;
            else if (ud.screen_size < 64 && !(ud.statusbarflags & STATUSBAR_NOSHRINK))
                ud.screen_size += dir;
        }
        else // enlarging
        {
            if (ud.screen_size > 12)
               ud.screen_size += dir;
            else if (ud.screen_size > 8 && (!(ud.statusbarflags & STATUSBAR_NOFULL) || !(ud.statusbarflags & STATUSBAR_NOOVERLAY)))
                ud.screen_size = 8;
            else if (ud.screen_size == 8 && ud.statusbarmode == 0 && !(ud.statusbarflags & STATUSBAR_NOOVERLAY))
                ud.statusbarmode = 1;
            else if (ud.screen_size > 4 && (!(ud.statusbarflags & STATUSBAR_NOMINI) || !(ud.statusbarflags & STATUSBAR_NOMODERN)))
                ud.screen_size = 4;
            else if (ud.screen_size == 4 && ud.statusbarcustom > 0)
                ud.statusbarcustom -= 1;
            else if (ud.screen_size == 4 && ud.althud == 0 && !(ud.statusbarflags & STATUSBAR_NOMODERN))
                ud.althud = 1;
            else if (ud.screen_size > 0 && !(ud.statusbarflags & STATUSBAR_NONONE))
                ud.screen_size = 0;
        }
    }
    G_UpdateScreenArea();
}

void G_InitTimer(int32_t ticspersec)
{
    if (g_timerTicsPerSecond != ticspersec)
    {
        timerUninit();
        timerInit(ticspersec);
        timerSetCallback(gameTimerHandler);
        g_timerTicsPerSecond = ticspersec;

        g_lastInputTicks = 0;
        for (bssize_t TRAVERSE_CONNECT(playerNum))
            g_player[playerNum].lastViewUpdate = 0;
    }
}


static int32_t g_RTSPlaying;

// Returns: started playing?
extern int G_StartRTS(int lumpNum, int localPlayer)
{
    if (!ud.lockout && ud.config.SoundToggle &&
        RTS_IsInitialized() && g_RTSPlaying == 0 && (ud.config.VoiceToggle & (localPlayer ? 1 : 4)))
    {
        char *const pData = (char *)RTS_GetSound(lumpNum - 1);

        if (pData != NULL)
        {
            FX_Play3D(pData, RTS_SoundLength(lumpNum - 1), FX_ONESHOT, 0, 0, 1, 255, fix16_one, -lumpNum);
            g_RTSPlaying = 7;
            return 1;
        }
    }

    return 0;
}

void G_PrintCurrentMusic(void)
{
    char *fn = g_mapInfo[g_musicIndex].musicfn;
    if (fn[0] == '/') fn++;
    g_player[myconnectindex].ps->ftq = 0;
    Bsnprintf(apStrings[QUOTE_MUSIC], MAXQUOTELEN, "Playing %s", fn);
    P_DoQuote(QUOTE_MUSIC, g_player[myconnectindex].ps);
}

void G_HandleLocalKeys(void)
{
//    CONTROL_ProcessBinds();
    auto &myplayer = *g_player[myconnectindex].ps;

    if (ud.recstat == 2 || (myplayer.gm & MODE_MENU) == MODE_MENU)
    {
        ControlInfo noshareinfo;
        CONTROL_GetInput(&noshareinfo);
    }

    if (g_player[myconnectindex].gotvote == 0 && voting != -1 && voting != myconnectindex)
    {
        if (KB_UnBoundKeyPressed(sc_F1) || KB_UnBoundKeyPressed(sc_F2) || ud.autovote)
        {
            G_AddUserQuote("Vote Cast");
            Net_SendMapVote(KB_UnBoundKeyPressed(sc_F1) || ud.autovote ? ud.autovote-1 : 0);
            KB_ClearKeyDown(sc_F1);
            KB_ClearKeyDown(sc_F2);
        }
    }

    if (!ALT_IS_PRESSED && ud.overhead_on == 0 && (myplayer.gm & MODE_TYPE) == 0)
    {
        if (BUTTON(gamefunc_Enlarge_Screen))
        {
            CONTROL_ClearButton(gamefunc_Enlarge_Screen);

            if (!SHIFTS_IS_PRESSED)
            {
                // conditions copied from G_SetViewportShrink
                if ((ud.screen_size > 12) ||
                    (ud.screen_size > 8 && (!(ud.statusbarflags & STATUSBAR_NOFULL) || !(ud.statusbarflags & STATUSBAR_NOOVERLAY))) ||
                    (ud.screen_size == 8 && ud.statusbarmode == 0 && !(ud.statusbarflags & STATUSBAR_NOOVERLAY)) ||
                    (ud.screen_size > 4 && (!(ud.statusbarflags & STATUSBAR_NOMINI) || !(ud.statusbarflags & STATUSBAR_NOMODERN))) ||
                    (ud.screen_size == 4 && ud.statusbarcustom > 0) ||
                    (ud.screen_size == 4 && ud.althud == 0 && !(ud.statusbarflags & STATUSBAR_NOMODERN)) ||
                    (ud.screen_size > 0 && !(ud.statusbarflags & STATUSBAR_NONONE)))
                {
                    S_PlaySound(THUD);
                    G_SetViewportShrink(-4);
                }
            }
            else
            {
                G_SetStatusBarScale(ud.statusbarscale+5);
            }

            G_UpdateScreenArea();
        }

        if (BUTTON(gamefunc_Shrink_Screen))
        {
            CONTROL_ClearButton(gamefunc_Shrink_Screen);

            if (!SHIFTS_IS_PRESSED)
            {
                // conditions copied from G_SetViewportShrink
                if ((ud.screen_size < 4 && (!(ud.statusbarflags & STATUSBAR_NOMINI) || !(ud.statusbarflags & STATUSBAR_NOMODERN))) ||
                    (ud.screen_size == 4 && ud.althud == 1 && !(ud.statusbarflags & STATUSBAR_NOMINI)) ||
                    (ud.screen_size == 4 && ud.statusbarcustom < ud.statusbarrange && !(ud.statusbarflags & STATUSBAR_NOMINI)) ||
                    (ud.screen_size < 8 && (!(ud.statusbarflags & STATUSBAR_NOFULL) || !(ud.statusbarflags & STATUSBAR_NOOVERLAY))) ||
                    (ud.screen_size == 8 && ud.statusbarmode == 1 && !(ud.statusbarflags & STATUSBAR_NOFULL)) ||
                    (ud.screen_size < 64 && !(ud.statusbarflags & STATUSBAR_NOSHRINK)))
                {
                    S_PlaySound(THUD);
                    G_SetViewportShrink(+4);
                }
            }
            else
            {
                G_SetStatusBarScale(ud.statusbarscale-5);
            }

            G_UpdateScreenArea();
        }
    }

    if (myplayer.cheat_phase == 1 || (myplayer.gm & (MODE_MENU|MODE_TYPE)))
        return;

    if (BUTTON(gamefunc_See_Coop_View) && (GTFLAGS(GAMETYPE_COOPVIEW) || ud.recstat == 2))
    {
        CONTROL_ClearButton(gamefunc_See_Coop_View);
        screenpeek = connectpoint2[screenpeek];
        if (screenpeek == -1) screenpeek = 0;
        g_restorePalette = -1;
    }

    if ((g_netServer || ud.multimode > 1) && BUTTON(gamefunc_Show_Opponents_Weapon))
    {
        CONTROL_ClearButton(gamefunc_Show_Opponents_Weapon);
        ud.config.ShowWeapons = ud.showweapons = 1-ud.showweapons;
        P_DoQuote(QUOTE_WEAPON_MODE_OFF-ud.showweapons, &myplayer);
    }

    if (BUTTON(gamefunc_Toggle_Crosshair))
    {
        CONTROL_ClearButton(gamefunc_Toggle_Crosshair);
        ud.crosshair = !ud.crosshair;
        P_DoQuote(QUOTE_CROSSHAIR_OFF-ud.crosshair, &myplayer);
    }

    if (ud.overhead_on && BUTTON(gamefunc_Map_Follow_Mode))
    {
        CONTROL_ClearButton(gamefunc_Map_Follow_Mode);
        ud.scrollmode = 1-ud.scrollmode;
        if (ud.scrollmode)
        {
            ud.folx = g_player[screenpeek].ps->opos.x;
            ud.foly = g_player[screenpeek].ps->opos.y;
            ud.fola = fix16_to_int(g_player[screenpeek].ps->oq16ang);
        }
        P_DoQuote(QUOTE_MAP_FOLLOW_OFF+ud.scrollmode, &myplayer);
    }

    if (KB_UnBoundKeyPressed(sc_ScrollLock))
    {
        KB_ClearKeyDown(sc_ScrollLock);

        switch (ud.recstat)
        {
        case 0:
            if (SHIFTS_IS_PRESSED)
                G_OpenDemoWrite();
            break;
        case 1:
            G_CloseDemoWrite();
            break;
        }
    }

    if (ud.recstat == 2)
    {
        if (KB_KeyPressed(sc_Space))
        {
            KB_ClearKeyDown(sc_Space);

            g_demo_paused = !g_demo_paused;
            g_demo_rewind = 0;

            S_PauseSounds(g_demo_paused || ud.pause_on);
        }

        if (KB_KeyPressed(sc_Tab))
        {
            KB_ClearKeyDown(sc_Tab);
            g_demo_showStats = !g_demo_showStats;
        }

#if 0
        if (KB_KeyPressed(sc_kpad_Plus))
        {
            G_InitTimer(240);
        }
        else if (KB_KeyPressed(sc_kpad_Minus))
        {
            G_InitTimer(60);
        }
        else if (g_timerTicsPerSecond != 120)
        {
            G_InitTimer(120);
        }
#endif

        if (KB_KeyPressed(sc_kpad_6))
        {
            KB_ClearKeyDown(sc_kpad_6);

            int const fwdTics = (15 << (int)ALT_IS_PRESSED) << (2 * (int)SHIFTS_IS_PRESSED);
            g_demo_goalCnt    = g_demo_paused ? g_demo_cnt + 1 : g_demo_cnt + REALGAMETICSPERSEC * fwdTics;
            g_demo_rewind     = 0;

            if (g_demo_goalCnt > g_demo_totalCnt)
                g_demo_goalCnt = 0;
            else
                Demo_PrepareWarp();
        }
        else if (KB_KeyPressed(sc_kpad_4))
        {
            KB_ClearKeyDown(sc_kpad_4);

            int const rewindTics = (15 << (int)ALT_IS_PRESSED) << (2 * (int)SHIFTS_IS_PRESSED);
            g_demo_goalCnt       = g_demo_paused ? g_demo_cnt - 1 : g_demo_cnt - REALGAMETICSPERSEC * rewindTics;
            g_demo_rewind        = 1;

            if (g_demo_goalCnt <= 0)
                g_demo_goalCnt = 1;

            Demo_PrepareWarp();
        }

#if 0
        // Enter a game from within a demo.
        if (KB_KeyPressed(sc_Return) && ud.multimode==1)
        {
            KB_ClearKeyDown(sc_Return);
            g_demo_cnt = g_demo_goalCnt = ud.reccnt = ud.pause_on = ud.recstat = ud.m_recstat = 0;
            // XXX: probably redundant; this stuff needs an API anyway:
            kclose(g_demo_recFilePtr); g_demo_recFilePtr = buildvfs_kfd_invalid;
            myplayer.gm = MODE_GAME;
            ready2send=1;  // TODO: research this weird variable
            screenpeek=myconnectindex;
//            g_demo_paused=0;
        }
#endif
    }

    if (SHIFTS_IS_PRESSED || ALT_IS_PRESSED || WIN_IS_PRESSED)
    {
        int ridiculeNum = 0;

        // NOTE: sc_F1 .. sc_F10 are contiguous. sc_F11 is not sc_F10+1.
        for (bssize_t j=sc_F1; j<=sc_F10; j++)
            if (KB_UnBoundKeyPressed(j))
            {
                KB_ClearKeyDown(j);
                ridiculeNum = j - sc_F1 + 1;
                break;
            }

        if (ridiculeNum)
        {
            if (SHIFTS_IS_PRESSED)
            {
                if (ridiculeNum == 5 && myplayer.fta > 0 && myplayer.ftq == QUOTE_MUSIC)
                {
                    const unsigned int maxi = VOLUMEALL ? MUS_FIRST_SPECIAL : 6;

                    unsigned int const oldMusicIndex = g_musicIndex;
                    unsigned int MyMusicIndex = g_musicIndex;
                    do
                    {
                        ++MyMusicIndex;
                        if (MyMusicIndex >= maxi)
                            MyMusicIndex = 0;
                    }
                    while (S_TryPlayLevelMusic(MyMusicIndex) && MyMusicIndex != oldMusicIndex);

                    G_PrintCurrentMusic();

                    return;
                }

                G_AddUserQuote(ud.ridecule[ridiculeNum-1]);

#ifndef NETCODE_DISABLE
                tempbuf[0] = PACKET_MESSAGE;
                tempbuf[1] = 255;
                tempbuf[2] = 0;
                Bstrcat(tempbuf+2,ud.ridecule[ridiculeNum-1]);

                ridiculeNum = 2+strlen(ud.ridecule[ridiculeNum-1]);

                tempbuf[ridiculeNum++] = myconnectindex;

                if (g_netClient)
                    enet_peer_send(g_netClientPeer, CHAN_CHAT, enet_packet_create(&tempbuf[0], ridiculeNum, 0));
                else if (g_netServer)
                    enet_host_broadcast(g_netServer, CHAN_CHAT, enet_packet_create(&tempbuf[0], ridiculeNum, 0));
#endif
                pus = NUMPAGES;
                pub = NUMPAGES;

                return;
            }

            // Not SHIFT -- that is, either some ALT or WIN.
            if (G_StartRTS(ridiculeNum, 1))
            {
#ifndef NETCODE_DISABLE
                if ((g_netServer || ud.multimode > 1))
                {
                    tempbuf[0] = PACKET_RTS;
                    tempbuf[1] = ridiculeNum;
                    tempbuf[2] = myconnectindex;

                    if (g_netClient)
                        enet_peer_send(g_netClientPeer, CHAN_CHAT, enet_packet_create(&tempbuf[0], 3, 0));
                    else if (g_netServer)
                        enet_host_broadcast(g_netServer, CHAN_CHAT, enet_packet_create(&tempbuf[0], 3, 0));
                }
#endif
                pus = NUMPAGES;
                pub = NUMPAGES;

                return;
            }
        }
    }

    if (!ALT_IS_PRESSED && !SHIFTS_IS_PRESSED && !WIN_IS_PRESSED)
    {
        if ((g_netServer || ud.multimode > 1) && BUTTON(gamefunc_SendMessage))
        {
            KB_FlushKeyboardQueue();
            CONTROL_ClearButton(gamefunc_SendMessage);
            myplayer.gm |= MODE_TYPE;
            typebuf[0] = 0;
        }

        if (KB_UnBoundKeyPressed(sc_F1) && !(G_GetLogoFlags() & LOGO_NOHELP)/* || (ud.show_help && I_AdvanceTrigger())*/)
        {
            KB_ClearKeyDown(sc_F1);

            Menu_Change(MENU_STORY);
            S_PauseSounds(true);
            Menu_Open(myconnectindex);

            if ((!g_netServer && ud.multimode < 2))
            {
                ready2send = 0;
                totalclock = ototalclock;
                screenpeek = myconnectindex;
            }
        }

        //        if((!net_server && ud.multimode < 2))
        {
            if (ud.recstat != 2 && KB_UnBoundKeyPressed(sc_F2))
            {
                KB_ClearKeyDown(sc_F2);

FAKE_F2:
                if (sprite[myplayer.i].extra <= 0)
                {
                    P_DoQuote(QUOTE_SAVE_DEAD, &myplayer);
                    return;
                }

                Menu_Change(MENU_SAVE);

                S_PauseSounds(true);
                Menu_Open(myconnectindex);

                if ((!g_netServer && ud.multimode < 2))
                {
                    ready2send = 0;
                    totalclock = ototalclock;
                    screenpeek = myconnectindex;
                }
            }

            if (KB_UnBoundKeyPressed(sc_F3))
            {
                KB_ClearKeyDown(sc_F3);

FAKE_F3:
                Menu_Change(MENU_LOAD);
                S_PauseSounds(true);
                Menu_Open(myconnectindex);

                if ((!g_netServer && ud.multimode < 2) && ud.recstat != 2)
                {
                    ready2send = 0;
                    totalclock = ototalclock;
                }

                screenpeek = myconnectindex;
            }
        }

        if (KB_UnBoundKeyPressed(sc_F4))
        {
            KB_ClearKeyDown(sc_F4);

            S_PauseSounds(true);
            Menu_Open(myconnectindex);

            if ((!g_netServer && ud.multimode < 2) && ud.recstat != 2)
            {
                ready2send = 0;
                totalclock = ototalclock;
            }

            Menu_Change(MENU_SOUND_INGAME);
        }

#ifndef EDUKE32_STANDALONE // FIXME?
        if (!FURY && KB_UnBoundKeyPressed(sc_F5) && ud.config.MusicToggle)
        {
            map_t *const pMapInfo    = &g_mapInfo[g_musicIndex];
            char *const  musicString = apStrings[QUOTE_MUSIC];

            KB_ClearKeyDown(sc_F5);

            if (pMapInfo->musicfn != NULL)
            {
                char *fn = pMapInfo->musicfn;
                if (fn[0] == '/') fn++;
                Bsnprintf(musicString, MAXQUOTELEN, "%s. SHIFT-F5 to change.", fn);
            }
            else
                musicString[0] = '\0';

            P_DoQuote(QUOTE_MUSIC, g_player[myconnectindex].ps);
        }
#endif

        if ((BUTTON(gamefunc_Quick_Save) || g_doQuickSave == 1) && (myplayer.gm & MODE_GAME))
        {
            CONTROL_ClearButton(gamefunc_Quick_Save);

            g_doQuickSave = 0;

            if (!g_lastusersave.isValid())
                goto FAKE_F2;

            KB_FlushKeyboardQueue();

            if (sprite[myplayer.i].extra <= 0)
            {
                P_DoQuote(QUOTE_SAVE_DEAD, &myplayer);
                return;
            }

            g_screenCapture = 1;
            G_DrawRooms(myconnectindex,65536);
            g_screenCapture = 0;

            if (g_lastusersave.isValid())
            {
                savebrief_t & sv = g_lastusersave;

                // dirty hack... char 127 in last position indicates an auto-filled name
                if (sv.name[MAXSAVEGAMENAME] == 127)
                {
                    strncpy(sv.name, g_mapInfo[ud.volume_number * MAXLEVELS + ud.level_number].name, MAXSAVEGAMENAME);
                    sv.name[MAXSAVEGAMENAME] = 127;
                }

                g_quickload = &sv;
                G_SavePlayerMaybeMulti(sv);
            }

            walock[TILE_SAVESHOT] = CACHE1D_UNLOCKED;
        }

        if (BUTTON(gamefunc_Third_Person_View))
        {
            CONTROL_ClearButton(gamefunc_Third_Person_View);

            myplayer.over_shoulder_on = !myplayer.over_shoulder_on;

            CAMERADIST  = 0;
            CAMERACLOCK = (int32_t) totalclock;

            P_DoQuote(QUOTE_VIEW_MODE_OFF + myplayer.over_shoulder_on, &myplayer);
        }

        if (KB_UnBoundKeyPressed(sc_F8))
        {
            KB_ClearKeyDown(sc_F8);

            int const fta = !ud.fta_on;
            ud.fta_on     = 1;
            P_DoQuote(fta ? QUOTE_MESSAGES_ON : QUOTE_MESSAGES_OFF, &myplayer);
            ud.fta_on     = fta;
        }

        if ((BUTTON(gamefunc_Quick_Load) || g_doQuickSave == 2) && (myplayer.gm & MODE_GAME))
        {
            CONTROL_ClearButton(gamefunc_Quick_Load);

            g_doQuickSave = 0;

            if (g_quickload == nullptr || !g_quickload->isValid())
                goto FAKE_F3;
            else if (g_quickload->isValid())
            {
                KB_FlushKeyboardQueue();
                KB_ClearKeysDown();
                S_PauseSounds(true);
                if (G_LoadPlayerMaybeMulti(*g_quickload) != 0)
                    g_quickload->reset();
            }
        }

        if (KB_UnBoundKeyPressed(sc_F10))
        {
            KB_ClearKeyDown(sc_F10);

            Menu_Change(MENU_QUIT_INGAME);
            S_PauseSounds(true);
            Menu_Open(myconnectindex);

            if ((!g_netServer && ud.multimode < 2) && ud.recstat != 2)
            {
                ready2send = 0;
                totalclock = ototalclock;
            }
        }

        if (KB_UnBoundKeyPressed(sc_F11))
        {
            KB_ClearKeyDown(sc_F11);

            Menu_Change(MENU_COLCORR_INGAME);
            S_PauseSounds(true);
            Menu_Open(myconnectindex);

            if ((!g_netServer && ud.multimode < 2) && ud.recstat != 2)
            {
                ready2send = 0;
                totalclock = ototalclock;
            }
        }

        if (ud.overhead_on != 0)
        {
            int const timerOffset = ((int) totalclock - nonsharedtimer);
            nonsharedtimer += timerOffset;

            if (BUTTON(gamefunc_Enlarge_Screen))
                myplayer.zoom += mulscale6(timerOffset, max<int>(myplayer.zoom, 256));

            if (BUTTON(gamefunc_Shrink_Screen))
                myplayer.zoom -= mulscale6(timerOffset, max<int>(myplayer.zoom, 256));

            myplayer.zoom = clamp(myplayer.zoom, 48, 2048);
        }
    }

    if (I_EscapeTrigger() && ud.overhead_on && myplayer.newowner == -1)
    {
        I_EscapeTriggerClear();
        ud.last_overhead = ud.overhead_on;
        ud.overhead_on   = 0;
        ud.scrollmode    = 0;
        G_UpdateScreenArea();
    }

    if (BUTTON(gamefunc_AutoRun))
    {
        CONTROL_ClearButton(gamefunc_AutoRun);
        ud.auto_run = 1-ud.auto_run;
        P_DoQuote(QUOTE_RUN_MODE_OFF + ud.auto_run, &myplayer);
    }

    if (BUTTON(gamefunc_Map))
    {
        CONTROL_ClearButton(gamefunc_Map);
        if (ud.last_overhead != ud.overhead_on && ud.last_overhead)
        {
            ud.overhead_on = ud.last_overhead;
            ud.last_overhead = 0;
        }
        else
        {
            ud.overhead_on++;
            if (ud.overhead_on == 3) ud.overhead_on = 0;
            ud.last_overhead = ud.overhead_on;
        }

#ifdef __ANDROID__
        if (ud.overhead_on == 1)
            ud.scrollmode = 0;
        else if (ud.overhead_on == 2)
        {
            ud.scrollmode = 1;
            ud.folx = g_player[screenpeek].ps->opos.x;
            ud.foly = g_player[screenpeek].ps->opos.y;
            ud.fola = g_player[screenpeek].ps->oang;
        }
#endif
        g_restorePalette = 1;
        G_UpdateScreenArea();
    }
}

static int parsedefinitions_game(scriptfile *, int);

static void parsedefinitions_game_include(const char *fileName, scriptfile * /*pScript*/, const char * /*cmdtokptr*/, int const firstPass)
{
    scriptfile *included = scriptfile_fromfile(fileName);

    if (included)
    {
        parsedefinitions_game(included, firstPass);
        scriptfile_close(included);
    }
}

static void parsedefinitions_game_animsounds(scriptfile *pScript, const char * blockEnd, char const * fileName, dukeanim_t * animPtr)
{
    Xfree(animPtr->sounds);

    size_t numPairs = 0, allocSize = 4;

    animPtr->sounds = (animsound_t *)Xmalloc(allocSize * sizeof(animsound_t));
    animPtr->numsounds = 0;

    int defError = 1;
    uint16_t lastFrameNum = 1;

    while (pScript->textptr < blockEnd)
    {
        int32_t frameNum;
        int32_t soundNum;

        // HACK: we've reached the end of the list
        //  (hack because it relies on knowledge of
        //   how scriptfile_* preprocesses the text)
        if (blockEnd - pScript->textptr == 1)
            break;

        // would produce error when it encounters the closing '}'
        // without the above hack
        if (scriptfile_getnumber(pScript, &frameNum))
            break;

        defError = 1;

        if (scriptfile_getsymbol(pScript, &soundNum))
            break;

        // frame numbers start at 1 for us
        if (frameNum <= 0)
        {
            LOG_F(ERROR, "%s:%d: error: frame number must be greater than zero",
                         pScript->filename, scriptfile_getlinum(pScript, pScript->ltextptr));
            break;
        }

        if (frameNum < lastFrameNum)
        {
            LOG_F(ERROR, "%s:%d: error: frame numbers must be in (not necessarily strictly) ascending order",
                         pScript->filename, scriptfile_getlinum(pScript, pScript->ltextptr));
            break;
        }

        lastFrameNum = frameNum;

        if ((unsigned)soundNum >= MAXSOUNDS && soundNum != -1)
        {
            LOG_F(ERROR, "%s:%d: error: sound number #%d invalid",
                         pScript->filename, scriptfile_getlinum(pScript, pScript->ltextptr),
                         soundNum);
            break;
        }

        if (numPairs >= allocSize)
        {
            allocSize *= 2;
            animPtr->sounds = (animsound_t *)Xrealloc(animPtr->sounds, allocSize * sizeof(animsound_t));
        }

        defError = 0;

        animsound_t & sound = animPtr->sounds[numPairs];
        sound.frame = frameNum;
        sound.sound = soundNum;

        ++numPairs;
    }

    if (!defError)
    {
        animPtr->numsounds = numPairs;
        // initprintf("Defined sound sequence for hi-anim '%s' with %d frame/sound pairs\n",
        //           hardcoded_anim_tokens[animnum].text, numpairs);
    }
    else
    {
        DO_FREE_AND_NULL(animPtr->sounds);
        LOG_F(ERROR, "Failed defining sound sequence for anim '%s'.", fileName);
    }
}

static const tokenlist newGameTokens[] =
{
    { "choice",        T_CHOICE },
};
static const tokenlist newGameChoiceTokens[] =
{
    { "name",          T_NAME },
    { "locked",        T_LOCKED },
    { "hidden",        T_HIDDEN },
    { "choice",        T_CHOICE },
    { "usercontent",   T_USERCONTENT },
};

static int newgamesubchoice_recursive(scriptfile *pScript, MenuGameplayEntry entry)
{

    char * subChoicePtr = pScript->ltextptr;
    char * subChoiceEnd;
    int32_t subChoiceID;
    if (scriptfile_getsymbol(pScript,&subChoiceID))
        return -1;
    if (scriptfile_getbraces(pScript,&subChoiceEnd))
        return -1;

    if ((unsigned)subChoiceID >= MAXMENUGAMEPLAYENTRIES)
    {
        LOG_F(ERROR, "%s:%d: error: maximum subchoices exceeded",
                     pScript->filename, scriptfile_getlinum(pScript, subChoicePtr));
        pScript->textptr = subChoiceEnd+1;
        return -1;
    }

    MenuGameplayEntry & subentry = entry.subentries[subChoiceID];
    subentry = MenuGameplayEntry{};
    subentry.subentries = (MenuGameplayEntry *) Xcalloc(MAXMENUGAMEPLAYENTRIES, sizeof(MenuGameplayEntry));

    while (pScript->textptr < subChoiceEnd)
    {
        switch (getatoken(pScript, newGameChoiceTokens, ARRAY_SIZE(newGameChoiceTokens)))
        {
            case T_CHOICE:
            {
                newgamesubchoice_recursive(pScript,subentry);
                break;
            }
            case T_NAME:
            {
                char *name = NULL;
                if (scriptfile_getstring(pScript, &name))
                    break;

                memset(subentry.name, 0, ARRAY_SIZE(subentry.name));
                strncpy(subentry.name, name, ARRAY_SIZE(subentry.name)-1);
                break;
            }
            case T_LOCKED:
            {
                subentry.flags |= MGE_Locked;
                break;
            }
            case T_HIDDEN:
            {
                subentry.flags |= MGE_Hidden;
                break;
            }
            case T_USERCONTENT:
            {
                subentry.flags |= MGE_UserContent;
                break;
            }
        }
    }

    return 0;
}

static void newgamechoices_recursive_free(MenuGameplayEntry* parent)
{
    MenuGameplayEntry* entries = parent->subentries;
    for (int i = 0; i < MAXMENUGAMEPLAYENTRIES; i++)
        if (entries[i].subentries)
            newgamechoices_recursive_free(&entries[i]);
    DO_FREE_AND_NULL(parent->subentries);
}

static int parsedefinitions_game(scriptfile *pScript, int firstPass)
{
    int   token;
    char *pToken;

    static const tokenlist tokens[] =
    {
        { "include",         T_INCLUDE          },
        { "#include",        T_INCLUDE          },
        { "includedefault",  T_INCLUDEDEFAULT   },
        { "#includedefault", T_INCLUDEDEFAULT   },
        { "define",          T_DEFINE           },
        { "#define",         T_DEFINE           },
        { "loadgrp",         T_LOADGRP          },
        { "cachesize",       T_CACHESIZE        },
        { "noautoload",      T_NOAUTOLOAD       },
        { "music",           T_MUSIC            },
        { "sound",           T_SOUND            },
        { "cutscene",        T_CUTSCENE         },
        { "animsounds",      T_ANIMSOUNDS       },
        { "globalgameflags", T_GLOBALGAMEFLAGS  },
        { "newgamechoices",  T_NEWGAMECHOICES   },
        { "localization"  ,  T_LOCALIZATION     },
        { "keyconfig"  ,     T_KEYCONFIG        },
        { "customsettings",  T_CUSTOMSETTINGS   },
    };

    static const tokenlist soundTokens[] =
    {
        { "id",       T_ID },
        { "file",     T_FILE },
        { "minpitch", T_MINPITCH },
        { "maxpitch", T_MAXPITCH },
        { "priority", T_PRIORITY },
        { "type",     T_TYPE },
        { "distance", T_DISTANCE },
        { "volume",   T_VOLUME },
    };

    static const tokenlist animTokens [] =
    {
        { "delay",         T_DELAY },
        { "aspect",        T_ASPECT },
        { "sounds",        T_SOUND },
        { "forcefilter",   T_FORCEFILTER },
        { "forcenofilter", T_FORCENOFILTER },
        { "texturefilter", T_TEXTUREFILTER },
    };

    static const tokenlist settingsListTokens [] =
    {
        { "title",           T_NAME },
        { "entry",           T_CHOICE },
        { "link",            T_LINK},
    };

    static const tokenlist settingsEntryTokens [] =
    {
        { "name",          T_NAME },
        { "index",         T_INDEX },
        { "font",          T_FONT },
        { "type",          T_TYPE },
        { "vstrings",      T_VSTRINGS },
        { "values",        T_VALUES },
    };

    for (int f = 0; f < NUMGAMEFUNCTIONS; f++)
       scriptfile_addsymbolvalue(gamefunc_symbol_names[f], f);

#ifndef EDUKE32_STANDALONE
    scriptfile_addsymbolvalue("gamefunc_Holo_Duke", gamefunc_Holo_Duke);
    scriptfile_addsymbolvalue("gamefunc_Jetpack", gamefunc_Jetpack);
    scriptfile_addsymbolvalue("gamefunc_NightVision", gamefunc_NightVision);
    scriptfile_addsymbolvalue("gamefunc_MedKit", gamefunc_MedKit);
    scriptfile_addsymbolvalue("gamefunc_Steroids", gamefunc_Steroids);
    scriptfile_addsymbolvalue("gamefunc_Quick_Kick", gamefunc_Quick_Kick);
#endif

    do
    {
        token  = getatoken(pScript, tokens, ARRAY_SIZE(tokens));
        pToken = pScript->ltextptr;

        switch (token)
        {
        case T_LOADGRP:
        {
            char *fileName;

            int32_t bakpathsearchmode = pathsearchmode;
            pathsearchmode = 1;
            if (!scriptfile_getstring(pScript,&fileName) && firstPass)
            {
                if (initgroupfile(fileName) == -1)
                    LOG_F(ERROR, "Could not find file '%s'.", fileName);
                else
                {
                    LOG_F(INFO, "Using file '%s' as game data.", fileName);
                    if (!g_noAutoLoad && !ud.setup.noautoload)
                        G_DoAutoload(fileName);
                }
            }

            pathsearchmode = bakpathsearchmode;
        }
        break;
        case T_CACHESIZE:
        {
            int32_t cacheSize;

            if (scriptfile_getnumber(pScript, &cacheSize) || !firstPass)
                break;

            if (cacheSize > 0)
                MAXCACHE1DSIZE = cacheSize << 10;
        }
        break;
        case T_INCLUDE:
        {
            char *fileName;

            if (!scriptfile_getstring(pScript, &fileName))
                parsedefinitions_game_include(fileName, pScript, pToken, firstPass);

            break;
        }
        case T_INCLUDEDEFAULT:
        {
            parsedefinitions_game_include(G_DefaultDefFile(), pScript, pToken, firstPass);
            break;
        }
        case T_DEFINE:
        {
            char *name;
            int32_t number;

            if (scriptfile_getstring(pScript, &name)) break;
            if (scriptfile_getsymbol(pScript, &number)) break;

            if (EDUKE32_PREDICT_FALSE(scriptfile_addsymbolvalue(name, number) < 0))
                LOG_F(WARNING, "%s:%d: warning: symbol %s unable to be redefined to %d",
                               pScript->filename, scriptfile_getlinum(pScript, pToken),
                               name, number);
            break;
        }
        case T_NOAUTOLOAD:
            if (firstPass)
                g_noAutoLoad = 1;
            break;
        case T_MUSIC:
        {
            char *tokenPtr = pScript->ltextptr;
            char *musicID  = NULL;
            char *fileName = NULL;
            char *musicEnd;

            if (scriptfile_getbraces(pScript, &musicEnd))
                break;

            while (pScript->textptr < musicEnd)
            {
                switch (getatoken(pScript, soundTokens, ARRAY_SIZE(soundTokens)))
                {
                    case T_ID: scriptfile_getstring(pScript, &musicID); break;
                    case T_FILE: scriptfile_getstring(pScript, &fileName); break;
                }
            }

            if (!firstPass)
            {
                if (musicID==NULL)
                {
                    LOG_F(ERROR, "%s:%d: error: missing ID for music definition",
                                 pScript->filename, scriptfile_getlinum(pScript,tokenPtr));
                    break;
                }

                if (fileName == NULL || check_file_exist(fileName))
                    break;

                if (S_DefineMusic(musicID, fileName) == -1)
                    LOG_F(ERROR, "%s:%d: error: invalid music ID",
                                 pScript->filename, scriptfile_getlinum(pScript, tokenPtr));
            }
        }
        break;

        case T_CUTSCENE:
        {
            char *fileName = NULL;

            scriptfile_getstring(pScript, &fileName);

            char *animEnd;

            if (scriptfile_getbraces(pScript, &animEnd))
                break;

            if (!firstPass)
            {
                dukeanim_t *animPtr = Anim_Find(fileName);

                if (!animPtr)
                {
                    animPtr = Anim_Create(fileName);
                    animPtr->framedelay = 10;
                    animPtr->frameflags = 0;
                }

                int32_t temp;

                while (pScript->textptr < animEnd)
                {
                    switch (getatoken(pScript, animTokens, ARRAY_SIZE(animTokens)))
                    {
                        case T_DELAY:
                            scriptfile_getnumber(pScript, &temp);
                            animPtr->framedelay = temp;
                            break;
                        case T_ASPECT:
                        {
                            double dtemp, dtemp2;
                            scriptfile_getdouble(pScript, &dtemp);
                            scriptfile_getdouble(pScript, &dtemp2);
                            animPtr->frameaspect1 = dtemp;
                            animPtr->frameaspect2 = dtemp2;
                            break;
                        }
                        case T_SOUND:
                        {
                            char *animSoundsEnd = NULL;
                            if (scriptfile_getbraces(pScript, &animSoundsEnd))
                                break;
                            parsedefinitions_game_animsounds(pScript, animSoundsEnd, fileName, animPtr);
                            break;
                        }
                        case T_FORCEFILTER:
                            animPtr->frameflags |= CUTSCENE_FORCEFILTER;
                            break;
                        case T_FORCENOFILTER:
                            animPtr->frameflags |= CUTSCENE_FORCENOFILTER;
                            break;
                        case T_TEXTUREFILTER:
                            animPtr->frameflags |= CUTSCENE_TEXTUREFILTER;
                            break;
                    }
                }
            }
            else
                pScript->textptr = animEnd+1;
        }
        break;
        case T_ANIMSOUNDS:
        {
            char *tokenPtr     = pScript->ltextptr;
            char *fileName     = NULL;

            scriptfile_getstring(pScript, &fileName);
            if (!fileName)
                break;

            char *animSoundsEnd = NULL;

            if (scriptfile_getbraces(pScript, &animSoundsEnd))
                break;

            if (firstPass)
            {
                pScript->textptr = animSoundsEnd+1;
                break;
            }

            dukeanim_t *animPtr = Anim_Find(fileName);

            if (!animPtr)
            {
                LOG_F(ERROR, "%s:%d: error: expected animation filename",
                             pScript->filename, scriptfile_getlinum(pScript, tokenPtr));
                break;
            }

            parsedefinitions_game_animsounds(pScript, animSoundsEnd, fileName, animPtr);
        }
        break;

        case T_SOUND:
        {
            char *tokenPtr = pScript->ltextptr;
            char *fileName = NULL;
            char *soundEnd;

            double volume = 1.0;

            int32_t soundNum = -1;
            int32_t maxpitch = 0;
            int32_t minpitch = 0;
            int32_t priority = 0;
            int32_t type     = 0;
            int32_t distance = 0;

            if (scriptfile_getbraces(pScript, &soundEnd))
                break;

            while (pScript->textptr < soundEnd)
            {
                switch (getatoken(pScript, soundTokens, ARRAY_SIZE(soundTokens)))
                {
                    case T_ID:       scriptfile_getsymbol(pScript, &soundNum); break;
                    case T_FILE:     scriptfile_getstring(pScript, &fileName); break;
                    case T_MINPITCH: scriptfile_getsymbol(pScript, &minpitch); break;
                    case T_MAXPITCH: scriptfile_getsymbol(pScript, &maxpitch); break;
                    case T_PRIORITY: scriptfile_getsymbol(pScript, &priority); break;
                    case T_TYPE:     scriptfile_getsymbol(pScript, &type);     break;
                    case T_DISTANCE: scriptfile_getsymbol(pScript, &distance); break;
                    case T_VOLUME:   scriptfile_getdouble(pScript, &volume);   break;
                }
            }

            if (!firstPass)
            {
                if (soundNum==-1)
                {
                    LOG_F(ERROR, "%s:%d: error: missing ID for sound definition",
                                 pScript->filename, scriptfile_getlinum(pScript,tokenPtr));
                    break;
                }

                if (fileName == NULL || check_file_exist(fileName))
                    break;

                // maybe I should have just packed this into a sound_t and passed a reference...
                if (S_DefineSound(soundNum, fileName, minpitch, maxpitch, priority, type, distance, volume) == -1)
                    LOG_F(ERROR, "%s:%d: error: invalid sound ID",
                                 pScript->filename, scriptfile_getlinum(pScript,tokenPtr));
            }
        }
        break;
        case T_GLOBALGAMEFLAGS: scriptfile_getnumber(pScript, &duke3d_globalflags); break;
        case T_NEWGAMECHOICES:
        {
            char * newGameChoicesEnd;
            if (scriptfile_getbraces(pScript,&newGameChoicesEnd))
                break;
            if (firstPass)
            {
                pScript->textptr = newGameChoicesEnd+1;
                break;
            }

            while (pScript->textptr < newGameChoicesEnd)
            {
                switch (getatoken(pScript, newGameTokens, ARRAY_SIZE(newGameTokens)))
                {
                    case T_CHOICE:
                    {
                        char * choicePtr = pScript->ltextptr;
                        char * choiceEnd;
                        int32_t choiceID;
                        if (scriptfile_getsymbol(pScript,&choiceID))
                            break;
                        if (scriptfile_getbraces(pScript,&choiceEnd))
                            break;

                        if ((unsigned)choiceID >= MAXMENUGAMEPLAYENTRIES)
                        {
                            LOG_F(ERROR, "%s:%d: error: maximum choices exceeded",
                                         pScript->filename, scriptfile_getlinum(pScript, choicePtr));
                            pScript->textptr = choiceEnd+1;
                            break;
                        }

                        MenuGameplayEntry & entry = g_MenuGameplayEntries[choiceID];
                        if (entry.subentries)
                            newgamechoices_recursive_free(&entry);

                        entry = MenuGameplayEntry{};
                        entry.subentries = (MenuGameplayEntry *) Xcalloc(MAXMENUGAMEPLAYENTRIES, sizeof(MenuGameplayEntry));

                        while (pScript->textptr < choiceEnd)
                        {
                            switch (getatoken(pScript, newGameChoiceTokens, ARRAY_SIZE(newGameChoiceTokens)))
                            {
                                case T_CHOICE:
                                {
                                    newgamesubchoice_recursive(pScript, entry);
                                    break;
                                }
                                case T_NAME:
                                {
                                    char *name = NULL;
                                    if (scriptfile_getstring(pScript, &name))
                                        break;

                                    memset(entry.name, 0, ARRAY_SIZE(entry.name));
                                    strncpy(entry.name, name, ARRAY_SIZE(entry.name)-1);
                                    break;
                                }
                                case T_LOCKED:
                                {
                                    entry.flags |= MGE_Locked;
                                    break;
                                }
                                case T_HIDDEN:
                                {
                                    entry.flags |= MGE_Hidden;
                                    break;
                                }
                                case T_USERCONTENT:
                                {
                                    entry.flags |= MGE_UserContent;
                                    break;
                                }
                            }
                        }

                        break;
                    }
                }
            }

            break;
        }

        case T_LOCALIZATION: // silence game-side warnings due to strings like "Music"
        {
            char * localeName;
            if (scriptfile_getstring(pScript, &localeName))
                break;

            char * blockend;
            if (scriptfile_getbraces(pScript, &blockend))
                break;

            pScript->textptr = blockend+1;
            break;
        }

        case T_KEYCONFIG:
        {
            char *keyRemapEnd;
            int32_t currSlot = 0, keyIndex;

            // will require a longer bitmap if additional gamefuncs are added
            uint64_t gamefunc_bitmap = 0ULL;
            Bassert(NUMGAMEFUNCTIONS <= 64);

            if (scriptfile_getbraces(pScript, &keyRemapEnd))
                break;

            if (firstPass)
            {
                pScript->textptr = keyRemapEnd+1;
                break;
            }

            while (pScript->textptr < keyRemapEnd - 1)
            {
                char * mapPtr = pScript->ltextptr;

                if (currSlot >= NUMGAMEFUNCTIONS)
                {
                    LOG_F(ERROR, "%s:%d: error: key remap exceeds number of valid gamefunctions %d",
                                 pScript->filename, scriptfile_getlinum(pScript, mapPtr),
                                 NUMGAMEFUNCTIONS);
                    pScript->textptr = keyRemapEnd+1;
                    break;
                }

                if (scriptfile_getsymbol(pScript, &keyIndex))
                    continue;

                if (keyIndex < 0 || keyIndex >= NUMGAMEFUNCTIONS)
                {
                    LOG_F(ERROR, "%s:%d: error: invalid key index %d",
                                 pScript->filename, scriptfile_getlinum(pScript, mapPtr),
                                 keyIndex);
                    continue;
                }
                else if (gamefunc_bitmap & (1ULL << keyIndex))
                {
                    LOG_F(WARNING, "%s:%d: warning: duplicate listing of key '%s'",
                                   pScript->filename, scriptfile_getlinum(pScript, mapPtr),
                                   gamefunc_symbol_names[keyIndex]);
                    continue;
                }

                keybind_order_custom[currSlot] = keyIndex;
                gamefunc_bitmap |= (1ULL << keyIndex);
                currSlot++;
            }

            // fill up remaining slots
            while(currSlot < NUMGAMEFUNCTIONS)
            {
                keybind_order_custom[currSlot] = -1;
                currSlot++;
            }

            // undefine gamefuncs that are no longer listed
            for(keyIndex = 0; keyIndex < NUMGAMEFUNCTIONS; keyIndex++)
            {
                if (!(gamefunc_bitmap & (1ULL << keyIndex)))
                {
                    hash_delete(&h_gamefuncs, gamefunctions[keyIndex]);
                    gamefunctions[keyIndex][0] = '\0';
                }
            }

            break;
        }

        case T_CUSTOMSETTINGS:
        {
            char * customSettingsEnd;
            if (scriptfile_getbraces(pScript,&customSettingsEnd))
                break;
            if (firstPass)
            {
                pScript->textptr = customSettingsEnd+1;
                break;
            }

            int32_t entryCount = 0;
            while (pScript->textptr < customSettingsEnd)
            {
                switch (getatoken(pScript, settingsListTokens, ARRAY_SIZE(settingsListTokens)))
                {
                    case T_NAME:
                    {
                        char *name = NULL;
                        if (scriptfile_getstring(pScript, &name))
                            break;
                        memset(s_CustomSettings, 0, ARRAY_SIZE(s_CustomSettings));
                        strncpy(s_CustomSettings, name, ARRAY_SIZE(s_CustomSettings)-1);
                        break;
                    }
                    case T_CHOICE:
                    {
                        char * entryPtr = pScript->ltextptr;
                        char * entryEnd;
                        if (scriptfile_getbraces(pScript,&entryEnd))
                            break;

                        if ((unsigned)entryCount >= MAXCUSTOMSETTINGSENTRIES)
                        {
                            initprintf("Error: Maximum entries exceeded near line %s:%d\n",
                                pScript->filename, scriptfile_getlinum(pScript, entryPtr));
                            pScript->textptr = entryEnd+1;
                            break;
                        }

                        MenuSettingsEntry & entry = g_MenuSettingsEntries[entryCount];
                        entry = MenuSettingsEntry{};

                        // Default Settings
                        entry.font = &MF_Redfont;
                        entry.type = EmptyAction;
                        int32_t multiCheckVal = -1;

                        while (pScript->textptr < entryEnd)
                        {
                            switch (getatoken(pScript, settingsEntryTokens, ARRAY_SIZE(settingsEntryTokens)))
                            {
                                case T_NAME:
                                {
                                    char *name = NULL;
                                    if (scriptfile_getstring(pScript, &name))
                                        break;
                                    memset(entry.name, 0, ARRAY_SIZE(entry.name));
                                    strncpy(entry.name, name, ARRAY_SIZE(entry.name)-1);
                                    break;
                                }
                                case T_INDEX:
                                    scriptfile_getnumber(pScript, &entry.storeidx);
                                    break;
                                case T_FONT:
                                {
                                    char *fontstr = NULL;
                                    if (scriptfile_getstring(pScript, &fontstr))
                                        break;

                                    if (!Bstrcasecmp(fontstr, "big") || !Bstrcasecmp(fontstr, "bigfont") || !Bstrcasecmp(fontstr, "redfont"))
                                        entry.font = &MF_Redfont;
                                    else if (!Bstrcasecmp(fontstr, "small") || !Bstrcasecmp(fontstr, "smallfont") || !Bstrcasecmp(fontstr, "bluefont"))
                                        entry.font = &MF_Bluefont;
                                    else if (!Bstrcasecmp(fontstr, "mini") || !Bstrcasecmp(fontstr, "minifont"))
                                        entry.font = &MF_Minifont;
                                    else
                                        initprintf("Error: invalid font '%s' specified near line %s:%d\n",
                                                    fontstr, pScript->filename, scriptfile_getlinum(pScript, entryPtr));
                                    break;
                                }
                                case T_TYPE:
                                {
                                    char *typestr = NULL;
                                    if (scriptfile_getstring(pScript, &typestr))
                                        break;

                                    if (!Bstrcasecmp(typestr, "button"))
                                        entry.type = ButtonAction;
                                    else if (!Bstrcasecmp(typestr, "yes/no") || !Bstrcasecmp(typestr, "no/yes"))
                                        entry.type = YesNoAction;
                                    else if (!Bstrcasecmp(typestr, "on/off") || !Bstrcasecmp(typestr, "off/on") || !Bstrcasecmp(typestr, "toggle"))
                                        entry.type = OnOffAction;
                                    else if (!Bstrcasecmp(typestr, "multi") || !Bstrcasecmp(typestr, "choices"))
                                        entry.type = MultiChoiceAction;
                                    else if (!Bstrcasecmp(typestr, "range") || !Bstrcasecmp(typestr, "slider"))
                                        entry.type = SliderAction;
                                    else if (!Bstrcasecmp(typestr, "spacer2"))
                                        entry.type = Spacer2Action;
                                    else if (!Bstrcasecmp(typestr, "spacer4"))
                                        entry.type = Spacer4Action;
                                    else if (!Bstrcasecmp(typestr, "spacer6"))
                                        entry.type = Spacer6Action;
                                    else if (!Bstrcasecmp(typestr, "spacer8"))
                                        entry.type = Spacer8Action;
                                    break;
                                }
                                case T_VSTRINGS:
                                {
                                    char * vstringPtr = pScript->ltextptr;
                                    char * vstringListEnd;
                                    int32_t vindex = 0;
                                    if (scriptfile_getbraces(pScript,&vstringListEnd))
                                        break;
                                    while (pScript->textptr < vstringListEnd - 1)
                                    {
                                        char* valueName = NULL;
                                        if (vindex >= MAXVALUECHOICES)
                                        {
                                            initprintf("Error: Maximum value name entries exceeded near line %s:%d\n",
                                            pScript->filename, scriptfile_getlinum(pScript, vstringPtr));
                                            pScript->textptr = vstringListEnd+1;
                                            break;
                                        }

                                        if (scriptfile_getstring(pScript, &valueName))
                                        {
                                            pScript->textptr = vstringListEnd+1;
                                            break;
                                        }
                                        memset(entry.valueNames[vindex], 0, ARRAY_SIZE(entry.valueNames[vindex]));
                                        strncpy(entry.valueNames[vindex], valueName, ARRAY_SIZE(entry.valueNames[vindex])-1);
                                        vindex += 1;
                                    }
                                    multiCheckVal = (multiCheckVal == -1) ? vindex : min(multiCheckVal, vindex);
                                    break;
                                }
                                case T_VALUES:
                                {
                                    char * valuePtr = pScript->ltextptr;
                                    char * valueListEnd;
                                    entry.values = (int32_t *) Xcalloc(MAXVALUECHOICES, sizeof(int32_t));
                                    int32_t vindex = 0;
                                    if (scriptfile_getbraces(pScript,&valueListEnd))
                                        break;
                                    while (pScript->textptr < valueListEnd - 1)
                                    {
                                        int32_t valueChoice;
                                        if (vindex >= MAXVALUECHOICES)
                                        {
                                            initprintf("Error: Maximum value entries exceeded near line %s:%d\n",
                                            pScript->filename, scriptfile_getlinum(pScript, valuePtr));
                                            pScript->textptr = valueListEnd+1;
                                            break;
                                        }

                                        if (scriptfile_getnumber(pScript,&valueChoice))
                                        {
                                            pScript->textptr = valueListEnd+1;
                                            break;
                                        }
                                        //snprintf(entry.valueNames[vindex], ARRAY_SIZE(entry.valueNames[vindex]), "%d", valueChoice);
                                        entry.values[vindex] = valueChoice;
                                        vindex += 1;
                                    }
                                    multiCheckVal = (multiCheckVal == -1) ? vindex : min(multiCheckVal, vindex);
                                    break;
                                }
                            }
                        }
                        entry.optionCount = (multiCheckVal <= -1) ? 0 : multiCheckVal;
                        entryCount++;
                        break;
                    }
                    case T_LINK:
                        //TODO: Single sublayer of pages
                        initprintf("Error: Link token not yet implemented, line %s:%d\n",
                                    pScript->filename, scriptfile_getlinum(pScript, pScript->ltextptr));
                        break;
                }
            }
            break;
        }
        case T_EOF: return 0;
        default: break;
        }
    }
    while (1);

    return 0;
}

int loaddefinitions_game(const char *fileName, int32_t firstPass)
{
    scriptfile *pScript = scriptfile_fromfile(fileName);

    if (pScript)
        parsedefinitions_game(pScript, firstPass);

    for (char const * m : g_defModules)
        parsedefinitions_game_include(m, NULL, "null", firstPass);

    if (pScript)
        scriptfile_close(pScript);

    scriptfile_clearsymbols();

    return 0;
}

void G_UpdateAppTitle(char const * const name /*= nullptr*/)
{
    Bsprintf(tempbuf, APPNAME " %s", s_buildRev);

    if (name != nullptr)
    {
        if (g_gameNamePtr)
#ifdef EDUKE32_STANDALONE
            Bsnprintf(apptitle, sizeof(apptitle), "%s - %s", name, g_gameNamePtr);
#else
            Bsnprintf(apptitle, sizeof(apptitle), "%s - %s - %s", name, g_gameNamePtr, tempbuf);
#endif
        else
            Bsnprintf(apptitle, sizeof(apptitle), "%s - %s", name, tempbuf);
    }
    else if (g_gameNamePtr)
    {
#ifdef EDUKE32_STANDALONE
        Bstrncpyz(apptitle, g_gameNamePtr, sizeof(apptitle));
#else
        Bsnprintf(apptitle, sizeof(apptitle), "%s - %s", g_gameNamePtr, tempbuf);
#endif
    }
    else
    {
#ifdef EDUKE32_STANDALONE
        Bstrncpyz(apptitle, APPNAME, sizeof(apptitle));
#else
        Bstrncpyz(apptitle, tempbuf, sizeof(apptitle));
#endif
    }

    wm_setapptitle(apptitle);
    communityapiSetRichPresence("status", name ? name : "In menu");
}

static void G_FreeHashAnim(const char * /*string*/, intptr_t key)
{
    Xfree((void *)key);
}

static void G_Cleanup(void)
{
    int32_t i;

    for (i=(MAXLEVELS*(MAXVOLUMES+1))-1; i>=0; i--) // +1 volume for "intro", "briefing" music
    {
        Xfree(g_mapInfo[i].name);
        Xfree(g_mapInfo[i].filename);
        Xfree(g_mapInfo[i].musicfn);

        G_FreeMapState(i);
    }

    for (i=MAXQUOTES-1; i>=0; i--)
    {
        Xfree(apStrings[i]);
        Xfree(apXStrings[i]);
    }

    for (i=MAXPLAYERS-1; i>=0; i--)
        Xfree(g_player[i].ps);

    for (i=0;i<=g_highestSoundIdx;i++)
    {
        if (g_sounds[i] != &nullsound)
        {
            DO_FREE_AND_NULL(g_sounds[i]->filename);

            if (g_sounds[i]->voices != &nullvoice)
                DO_FREE_AND_NULL(g_sounds[i]->voices);

            DO_FREE_AND_NULL(g_sounds[i]);
        }
    }

    DO_FREE_AND_NULL(g_sounds);

    Xfree(label);
    Xfree(labelcode);
    Xfree(apScript);
    Xfree(bitptr);
    for (i=0; i < MAXMENUGAMEPLAYENTRIES; i++)
        if (g_MenuGameplayEntries[i].subentries)
            newgamechoices_recursive_free(&g_MenuGameplayEntries[i]);

    auto ofs = vmoffset;

    while (ofs)
    {
        auto next = ofs->next;
        Xfree(ofs->fn);
        Xfree(ofs);
        ofs = next;
    }

//    Xfree(MusicPtr);

    Gv_Clear();

    hash_free(&h_gamevars);
    hash_free(&h_arrays);
    hash_free(&h_labels);
    hash_free(&h_gamefuncs);

    hash_loop(&h_dukeanim, G_FreeHashAnim);
    hash_free(&h_dukeanim);
    inthash_free(&h_dsound);

    inthash_free(&h_dynamictilemap);

    Duke_CommonCleanup();
}

/*
===================
=
= ShutDown
=
===================
*/

void G_Shutdown(void)
{
    CONFIG_WriteSetup(0);
    S_SoundShutdown();
    S_MusicShutdown();
    CONTROL_Shutdown();
    KB_Shutdown();
    engineUnInit();
    G_Cleanup();
    FreeGroups();
    OSD_Cleanup();
    uninitgroupfile();
    Bfflush(NULL);
}

/*
===================
=
= G_Startup
=
===================
*/

static void G_CompileScripts(void)
{
    int32_t psm = pathsearchmode;

    label     = (char *) Xmalloc(MAXLABELS << 6);
    labelcode = (int32_t *) Xmalloc(MAXLABELS * sizeof(int32_t));
    labeltype = (uint8_t *) Xmalloc(MAXLABELS * sizeof(uint8_t));

    if (g_scriptNamePtr != NULL)
        Bcorrectfilename(g_scriptNamePtr,0);

    pathsearchmode = 1;

    C_Compile(G_ConFile());

    if (g_loadFromGroupOnly) // g_loadFromGroupOnly is true only when compiling fails and internal defaults are utilized
        C_Compile(G_ConFile());

    // for safety
    if ((uint32_t)g_labelCnt >= MAXLABELS)
        G_GameExit("Error: too many labels defined!");


    label     = (char *) Xrealloc(label, g_labelCnt << 6);
    labelcode = (int32_t *) Xrealloc(labelcode, g_labelCnt * sizeof(int32_t));
    labeltype = (uint8_t *) Xrealloc(labeltype, g_labelCnt * sizeof(uint8_t));

    VM_OnEvent(EVENT_INIT);
    pathsearchmode = psm;
}

static inline void G_CheckGametype(void)
{
    ud.m_coop = clamp(ud.m_coop, 0, g_gametypeCnt-1);
    LOG_F(INFO, "%s",g_gametypeNames[ud.m_coop]);
    if (g_gametypeFlags[ud.m_coop] & GAMETYPE_ITEMRESPAWN)
        ud.m_respawn_items = ud.m_respawn_inventory = 1;
}

static void G_PostLoadPalette(void)
{
    if (!(duke3d_globalflags & DUKE3D_NO_PALETTE_CHANGES))
    {
        // Make color index 255 of default/water/slime palette black.
        if (basepaltable[BASEPAL] != NULL)
            Bmemset(&basepaltable[BASEPAL][255*3], 0, 3);
        if (basepaltable[WATERPAL] != NULL)
            Bmemset(&basepaltable[WATERPAL][255*3], 0, 3);
        if (basepaltable[SLIMEPAL] != NULL)
            Bmemset(&basepaltable[SLIMEPAL][255*3], 0, 3);
    }

    if (!(duke3d_globalflags & DUKE3D_NO_HARDCODED_FOGPALS))
        paletteSetupDefaultFog();

    if (!(duke3d_globalflags & DUKE3D_NO_PALETTE_CHANGES))
        paletteFixTranslucencyMask();

    palettePostLoadLookups();
}

#define SETFLAG(Tilenum, Flag) g_tile[Tilenum].flags |= Flag

// Has to be after setting the dynamic names (e.g. SHARK).
static void A_InitEnemyFlags(void)
{
#ifndef EDUKE32_STANDALONE
    int DukeEnemies[] = {
        SHARK, RECON, DRONE,
        LIZTROOPONTOILET, LIZTROOPJUSTSIT, LIZTROOPSTAYPUT, LIZTROOPSHOOT,
        LIZTROOPJETPACK, LIZTROOPSHOOT, LIZTROOPDUCKING, LIZTROOPRUNNING, LIZTROOP,
        OCTABRAIN, COMMANDER, COMMANDERSTAYPUT, PIGCOP, PIGCOPSTAYPUT, PIGCOPDIVE, EGG,
        LIZMAN, LIZMANSPITTING, LIZMANJUMP, ORGANTIC,
        BOSS1, BOSS2, BOSS3, BOSS4, RAT, ROTATEGUN };

    int SolidEnemies[] = { TANK, BOSS1, BOSS2, BOSS3, BOSS4, RECON, ROTATEGUN };
    int NoWaterDipEnemies[] = { OCTABRAIN, COMMANDER, DRONE };
    int GreenSlimeFoodEnemies[] = { LIZTROOP, LIZMAN, PIGCOP, NEWBEAST };

    for (bssize_t i=GREENSLIME; i<=GREENSLIME+7; i++)
        SETFLAG(i, SFLAG_HARDCODED_BADGUY);

    for (bssize_t i=ARRAY_SIZE(DukeEnemies)-1; i>=0; i--)
        SETFLAG(DukeEnemies[i], SFLAG_HARDCODED_BADGUY);

    for (bssize_t i=ARRAY_SIZE(SolidEnemies)-1; i>=0; i--)
        SETFLAG(SolidEnemies[i], SFLAG_NODAMAGEPUSH);

    for (bssize_t i=ARRAY_SIZE(NoWaterDipEnemies)-1; i>=0; i--)
        SETFLAG(NoWaterDipEnemies[i], SFLAG_NOWATERDIP);

    for (bssize_t i=ARRAY_SIZE(GreenSlimeFoodEnemies)-1; i>=0; i--)
        SETFLAG(GreenSlimeFoodEnemies[i], SFLAG_GREENSLIMEFOOD);

    if (WORLDTOUR)
    {
        SETFLAG(FIREFLY, SFLAG_HARDCODED_BADGUY);
        SETFLAG(BOSS5, SFLAG_NODAMAGEPUSH|SFLAG_HARDCODED_BADGUY);
        SETFLAG(BOSS1STAYPUT, SFLAG_NODAMAGEPUSH|SFLAG_HARDCODED_BADGUY);
        SETFLAG(BOSS2STAYPUT, SFLAG_NODAMAGEPUSH|SFLAG_HARDCODED_BADGUY);
        SETFLAG(BOSS3STAYPUT, SFLAG_NODAMAGEPUSH|SFLAG_HARDCODED_BADGUY);
        SETFLAG(BOSS4STAYPUT, SFLAG_NODAMAGEPUSH|SFLAG_HARDCODED_BADGUY);
        SETFLAG(BOSS5STAYPUT, SFLAG_NODAMAGEPUSH|SFLAG_HARDCODED_BADGUY);
    }
#endif
}
#undef SETFLAG

static void G_SetupGameButtons(void);

// Throw in everything here that needs to be called after a Lua game state
// recreation (or on initial startup in a non-Lunatic build.)
void G_PostCreateGameState(void)
{
    Net_SendClientInfo();
    A_InitEnemyFlags();
}

static void G_HandleMemErr(int32_t bytes, int32_t lineNum, const char *fileName, const char *funcName)
{
#ifdef DEBUGGINGAIDS
    debug_break();
    Bsprintf(tempbuf, "Out of memory: failed allocating %d bytes at %s:%d (%s)!", bytes, fileName, lineNum, funcName);
#else
    UNREFERENCED_PARAMETER(lineNum);
    UNREFERENCED_PARAMETER(fileName);
    UNREFERENCED_PARAMETER(funcName);
    Bsprintf(tempbuf, "Out of memory: failed allocating %d bytes!", bytes);
#endif
    fatal_exit(tempbuf);
}

static void G_FatalEngineInitError(void)
{
#ifdef DEBUGGINGAIDS
    debug_break();
#endif
    G_Cleanup();
    Bsprintf(tempbuf, "There was a problem initializing the engine: %s", engineerrstr);
    LOG_F(ERROR, "%s", tempbuf);
    fatal_exit(tempbuf);
}

static void G_Startup(void)
{
    int32_t i;

    set_memerr_handler(&G_HandleMemErr);

    timerInit(TICRATE);
    timerSetCallback(gameTimerHandler);

    initcrc32table();

    G_CompileScripts();

    if (engineInit())
        G_FatalEngineInitError();

    G_InitDynamicNames();

    // These depend on having the dynamic tile and/or sound mappings set up:
    G_InitMultiPsky(CLOUDYOCEAN, MOONSKY1, BIGORBIT1, LA);
    Gv_FinalizeWeaponDefaults();
    G_PostCreateGameState();
    if (g_netServer || ud.multimode > 1) G_CheckGametype();

    if (g_noSound) ud.config.SoundToggle = 0;
    if (g_noMusic) ud.config.MusicToggle = 0;

    if (CommandName)
    {
        //        Bstrncpy(szPlayerName, CommandName, 9);
        //        szPlayerName[10] = '\0';
        Bstrcpy(tempbuf,CommandName);

        while (Bstrlen(OSD_StripColors(tempbuf,tempbuf)) > 10)
            tempbuf[Bstrlen(tempbuf)-1] = '\0';

        Bstrncpyz(szPlayerName, tempbuf, sizeof(szPlayerName));
    }

    if (CommandMap)
    {
        if (VOLUMEONE)
        {
            LOG_F(WARNING, "The -map option is available in the registered version only.");
            boardfilename[0] = 0;
        }
        else
        {
            char *dot, *slash;

            boardfilename[0] = '/';
            boardfilename[1] = 0;
            Bstrcat(boardfilename, CommandMap);

            dot = Bstrrchr(boardfilename,'.');
            slash = Bstrrchr(boardfilename,'/');
            if (!slash) slash = Bstrrchr(boardfilename,'\\');

            if ((!slash && !dot) || (slash && dot < slash))
                Bstrcat(boardfilename,".map");

            Bcorrectfilename(boardfilename,0);

            buildvfs_kfd ii = kopen4loadfrommod(boardfilename, 0);
            if (ii != buildvfs_kfd_invalid)
            {
                LOG_F(INFO, "Using level: '%s'.",boardfilename);
                kclose(ii);
            }
            else
            {
                LOG_F(INFO, "Level '%s' not found.",boardfilename);
                boardfilename[0] = 0;
            }
        }
    }

    for (i=0; i<MAXPLAYERS; i++)
        g_player[i].pingcnt = 0;

    if (quitevent)
    {
        G_Shutdown();
        return;
    }

    Net_GetPackets();

    if (numplayers > 1)
        VLOG_F(LOG_NET, "Multiplayer initialized.");

    LOG_F(INFO, "Initializing ART files...");
    if (artLoadFiles("tiles000.art",MAXCACHE1DSIZE) < 0)
        G_GameExit("Failed loading art.");

    // Make the fullscreen nuke logo background non-fullbright.  Has to be
    // after dynamic tile remapping (from C_Compile) and loading tiles.
    picanm[LOADSCREEN].sf |= PICANM_NOFULLBRIGHT_BIT;

    // LOG_F(INFO, "Loading palette/lookups...");
    G_LoadLookups();

    screenpeek = myconnectindex;

    Bfflush(NULL);
}

void G_UpdatePlayerFromMenu(void)
{
    if (ud.recstat != 0)
        return;

    auto &p = *g_player[myconnectindex].ps;

    if (numplayers > 1)
    {
        Net_SendClientInfo();
        if (sprite[p.i].picnum == APLAYER && sprite[p.i].pal != 1)
            sprite[p.i].pal = g_player[myconnectindex].pcolor;
    }
    else
    {
        /*int32_t j = p.team;*/

        P_SetupMiscInputSettings();
        p.palookup = g_player[myconnectindex].pcolor = ud.color;

        g_player[myconnectindex].pteam = ud.team;

        if (sprite[p.i].picnum == APLAYER && sprite[p.i].pal != 1)
            sprite[p.i].pal = g_player[myconnectindex].pcolor;
    }
}

void G_BackToMenu(void)
{
    boardfilename[0] = 0;
    if (ud.recstat == 1) G_CloseDemoWrite();
    ud.warp_on = 0;
    g_player[myconnectindex].ps->gm = 0;
    Menu_Open(myconnectindex);
    Menu_Change(MENU_MAIN);
    KB_FlushKeyboardQueue();
    G_UpdateAppTitle();
}

static int G_EndOfLevel(void)
{
    auto &p = *g_player[myconnectindex].ps;

    P_SetGamePalette(&p, BASEPAL, 0);
    P_UpdateScreenPal(&p);

    if (p.gm & MODE_EOL)
    {
        G_CloseDemoWrite();

        ready2send = 0;

        if (p.player_par > 0 && (p.player_par < ud.playerbest || ud.playerbest < 0) && ud.display_bonus_screen == 1)
            CONFIG_SetMapBestTime(g_loadedMapHack.md4, p.player_par);

        if ((VM_OnEventWithReturn(EVENT_ENDLEVELSCREEN, p.i, myconnectindex, 0)) == 0 && ud.display_bonus_screen == 1)
        {
            int const ssize = ud.screen_size;
            ud.screen_size = 0;
            G_UpdateScreenArea();
            ud.screen_size = ssize;
            G_BonusScreen(0);
        }

        // Clear potentially loaded per-map ART only after the bonus screens.
        artClearMapArt();

        if (ud.eog || G_HaveUserMap())
        {
            ud.eog = 0;
            if ((!g_netServer && ud.multimode < 2))
            {
#ifndef EDUKE32_STANDALONE
                if (!VOLUMEALL)
                    G_DoOrderScreen();
#endif
                p.gm = 0;
                Menu_Open(myconnectindex);
                Menu_Change(MENU_MAIN);
                return 2;
            }
            else
            {
                ud.m_level_number = 0;
                ud.level_number = 0;
            }
        }
    }

    ud.display_bonus_screen = 1;
    ready2send = 0;

    if (numplayers > 1)
        p.gm = MODE_GAME;

    if (G_EnterLevel(p.gm))
    {
        G_BackToMenu();
        return 2;
    }

    Net_WaitForServer();
    return 1;
}

void app_crashhandler(void)
{
    G_CloseDemoWrite();
    VM_ScriptInfo(insptr, 64);
    abort();
}

#if defined(_WIN32) && defined(DEBUGGINGAIDS)
// See FILENAME_CASE_CHECK in cache1d.c
static int32_t check_filename_casing(void)
{
    return !(g_player[myconnectindex].ps->gm & MODE_GAME);
}
#endif

void G_MaybeAllocPlayer(int32_t pnum)
{
    if (g_player[pnum].ps == NULL)
        g_player[pnum].ps = (DukePlayer_t *)Xcalloc(1, sizeof(DukePlayer_t));
}

// TODO: reorder (net)actor_t to eliminate slop and update assertion
EDUKE32_STATIC_ASSERT(sizeof(actor_t)%4 == 0);
EDUKE32_STATIC_ASSERT(sizeof(DukePlayer_t)%4 == 0);

#ifndef NETCODE_DISABLE
void Net_DedicatedServerStdin(void)
{
# ifndef _WIN32
    // stdin -> OSD input for dedicated server
    if (g_networkMode == NET_DEDICATED_SERVER)
    {
        int32_t nb;
        char ch;
        static uint32_t bufpos = 0;
        static char buf[128];
# ifndef GEKKO
        int32_t flag = 1;
        ioctl(0, FIONBIO, &flag);
# endif
        if ((nb = read(0, &ch, 1)) > 0 && bufpos < sizeof(buf))
        {
            if (ch != '\n')
                buf[bufpos++] = ch;

            if (ch == '\n' || bufpos >= sizeof(buf)-1)
            {
                buf[bufpos] = 0;
                OSD_Dispatch(buf);
                bufpos = 0;
            }
        }
    }
# endif
}
#endif

void drawframe_do(void)
{
    MICROPROFILE_SCOPEI("Game", EDUKE32_FUNCTION, MP_YELLOWGREEN);

    g_lastFrameStartTime = timerGetNanoTicks();

    if (!g_saveRequested)
    {
        // only allow binds to function if the player is actually in a game (not in a menu, typing, et cetera) or demo
        CONTROL_BindsEnabled = !!(g_player[myconnectindex].ps->gm & (MODE_GAME | MODE_DEMO));

        G_HandleLocalKeys();
        OSD_DispatchQueued();
        P_GetInput(myconnectindex);
    }
    else
    {
        localInput = {};
        localInput.bits = (((int32_t)g_gameQuit) << SK_GAMEQUIT);
        localInput.extbits = BIT(EK_CHAT_MODE);
    }

    int const smoothratio = calc_smoothratio(totalclock, ototalclock);

    G_DrawRooms(screenpeek, smoothratio);

    if (videoGetRenderMode() >= REND_POLYMOST)
        G_DrawBackground();

    G_DisplayRest(smoothratio);

    g_frameJustDrawn = true;
    g_lastFrameEndTime = timerGetNanoTicks();
    g_lastFrameDuration = g_lastFrameEndTime - g_lastFrameStartTime;
    g_frameCounter++;

    videoNextPage();
    S_Update();
    g_lastFrameEndTime2 = timerGetNanoTicks();
    g_lastFrameDuration2 = g_lastFrameEndTime2 - g_lastFrameStartTime;
}

//static void drawframe_entry(mco_coro *co)
//{
//    do
//    {
//        drawframe_do();
//        mco_yield(co);
//    } while (1);
//}

void dukeFillInputForTic(void)
{
    // this is where we fill the input_t struct that is actually processed by P_ProcessInput()
    auto const pPlayer = g_player[myconnectindex].ps;
    auto const q16ang  = fix16_to_int(pPlayer->q16ang);
    auto& input   = inputfifo[0][myconnectindex];

    input = localInput;
    input.fvel = mulscale9(localInput.fvel, sintable[(q16ang + 2560) & 2047]) +
        mulscale9(localInput.svel, sintable[(q16ang + 2048) & 2047]);
    input.svel = mulscale9(localInput.fvel, sintable[(q16ang + 2048) & 2047]) +
        mulscale9(localInput.svel, sintable[(q16ang + 1536) & 2047]);

    if (!FURY)
    {
        input.fvel += pPlayer->fric.x;
        input.svel += pPlayer->fric.y;
    }

    localInput = {};
}

//void dukeCreateFrameRoutine(void)
//{
//    static mco_desc co_drawframe_desc;
//    mco_result res;
//
//    if (co_drawframe)
//    {
//        res = mco_destroy(co_drawframe);
//        Bassert(res == MCO_SUCCESS);
//        if (res != MCO_SUCCESS)
//            fatal_exit(mco_result_description(res));
//    }
//
//    co_drawframe_desc = mco_desc_init(drawframe_entry, g_frameStackSize);
//    co_drawframe_desc.user_data = NULL;
//
//    res = mco_create(&co_drawframe, &co_drawframe_desc);
//    Bassert(res == MCO_SUCCESS);
//    if (res != MCO_SUCCESS)
//        fatal_exit(mco_result_description(res));
//
//    if (g_frameStackSize != DRAWFRAME_DEFAULT_STACK_SIZE)
//        LOG_F(INFO, "Draw routine created with %d byte stack.", g_frameStackSize);
//}

static const char* dukeVerbosityCallback(loguru::Verbosity verbosity)
{
    switch (verbosity)
    {
        default: return nullptr;
        case LOG_VM: return "VM";
        case LOG_CON: return "CON";
    }
}

int app_main(int argc, char const* const* argv)
{
#ifdef _WIN32
#ifndef DEBUGGINGAIDS
    if (!G_CheckCmdSwitch(argc, argv, "-noinstancechecking") && !windowsCheckAlreadyRunning())
    {
        if (!wm_ynbox(APPNAME, "It looks like " APPNAME " is already running.\n\n"
                      "Are you sure you want to start another copy?"))
            app_exit(3);
    }
#endif

#ifndef USE_PHYSFS
#ifdef DEBUGGINGAIDS
    extern int32_t (*check_filename_casing_fn)(void);
    check_filename_casing_fn = check_filename_casing;
#endif
#endif
#endif

    G_ExtPreInit(argc, argv);

    engineSetLogFile(APPBASENAME ".log", LOG_GAME_MAX);
    engineSetLogVerbosityCallback(dukeVerbosityCallback);

#ifdef __APPLE__
    if (!g_useCwd)
    {
        char cwd[BMAX_PATH];
        char *homedir = Bgethomedir();
        if (homedir)
            Bsnprintf(cwd, sizeof(cwd), "%s/Library/Logs/" APPBASENAME ".log", homedir);
        else
            Bstrcpy(cwd, APPBASENAME ".log");
        OSD_SetLogFile(cwd);
        Xfree(homedir);
    }
#endif

#ifndef NETCODE_DISABLE
    if (enet_initialize() != 0)
        LOG_F(ERROR, "An error occurred while initializing ENet.");
#endif

    osdcallbacks_t callbacks = {};

    callbacks.drawchar        = dukeConsolePrintChar;
    callbacks.drawstr         = dukeConsolePrintString;
    callbacks.drawcursor      = dukeConsolePrintCursor;
    callbacks.getcolumnwidth  = dukeConsoleGetColumnWidth;
    callbacks.getrowheight    = dukeConsoleGetRowHeight;
    callbacks.clear = dukeConsoleClearBackground;
    callbacks.gettime         = BGetTime;
    callbacks.onshowosd       = dukeConsoleOnShowCallback;

    OSD_SetCallbacks(callbacks);

    G_UpdateAppTitle();

    LOG_F(INFO, HEAD2 " %s", s_buildRev);

    PrintBuildInfo();

    if (!g_useCwd)
        G_AddSearchPaths();

    g_maxDefinedSkill = 4;
    ud.multimode = 1;

    // This needs to happen before G_CheckCommandLine() because G_GameExit()
    // accesses g_player[0].
    G_MaybeAllocPlayer(0);

    G_CheckCommandLine(argc,argv);

    // This needs to happen afterwards, as G_CheckCommandLine() is where we set
    // up the command-line-provided search paths (duh).
    G_ExtInit();

#if defined(RENDERTYPEWIN) && defined(USE_OPENGL)
    if (forcegl) VLOG_F(LOG_GL, "OpenGL driver blacklist disabled.");
#endif

    // used with binds for fast function lookup
    hash_init(&h_gamefuncs);
    for (bssize_t i=NUMGAMEFUNCTIONS-1; i>=0; i--)
    {
        if (gamefunctions[i][0] == '\0')
            continue;

        hash_add(&h_gamefuncs,gamefunctions[i],i,0);
    }

#ifdef STARTUP_SETUP_WINDOW
    int const readSetup =
#endif
    CONFIG_ReadSetup();

#if 0
#if defined(_WIN32) && !defined (EDUKE32_STANDALONE)
    if (ud.config.CheckForUpdates == 1)
    {
        if (time(NULL) - ud.config.LastUpdateCheck > UPDATEINTERVAL)
        {
            LOG_F(INFO, "Checking for updates...");

            ud.config.LastUpdateCheck = time(NULL);

            if (windowsCheckForUpdates(tempbuf))
            {
                LOG_F(INFO, "Current version is %d",Batoi(tempbuf));

                if (Batoi(tempbuf) > atoi(s_buildDate))
                {
                    if (wm_ynbox("EDuke32","A new version of EDuke32 is available. "
                                 "Browse to http://www.eduke32.com now?"))
                    {
                        SHELLEXECUTEINFOA sinfo;
                        char const *p = "http://www.eduke32.com";

                        Bmemset(&sinfo, 0, sizeof(sinfo));
                        sinfo.cbSize  = sizeof(sinfo);
                        sinfo.fMask   = SEE_MASK_CLASSNAME;
                        sinfo.lpVerb  = "open";
                        sinfo.lpFile  = p;
                        sinfo.nShow   = SW_SHOWNORMAL;
                        sinfo.lpClass = "http";

                        if (!ShellExecuteExA(&sinfo))
                            LOG_F(ERROR, "ShellExecuteEx error launching browser!");
                    }
                }
                else LOG_F(INFO, "No updates available.");
            }
            else LOG_F(WARNING, "Failed to check for updates!");
        }
    }
#endif
#endif

#ifdef EDUKE32_STANDALONE
    if (!G_CheckCmdSwitch(argc, argv, "-nosteam"))
        communityapiInit();
#endif

    if (enginePreInit())
        G_FatalEngineInitError();

    if (Bstrcmp(g_setupFileName, SETUPFILENAME))
        LOG_F(INFO, "Using config file '%s'.",g_setupFileName);

    G_ScanGroups();

#ifdef STARTUP_SETUP_WINDOW
    if (g_commandSetup || (!Bgetenv("SteamTenfoot") && (readSetup < 0 || (!g_noSetup && (ud.configversion != BYTEVERSION_EDUKE32 || ud.setup.forcesetup)))))
    {
        if (quitevent || !startwin_run())
        {
            engineUnInit();
            app_exit(EXIT_SUCCESS);
        }
    }
#endif

    G_LoadGroups(!g_noAutoLoad && !ud.setup.noautoload);

    if (!g_useCwd)
        G_CleanupSearchPaths();

#ifndef EDUKE32_STANDALONE
    G_SetupCheats();

    if (SHAREWARE)
        g_Shareware = 1;
    else
    {
        buildvfs_kfd const kFile = kopen4load("DUKESW.BIN",1); // JBF 20030810

        if (kFile != buildvfs_kfd_invalid)
        {
            g_Shareware = 1;
            kclose(kFile);
        }
    }
#endif

    // gotta set the proper title after we compile the CONs if this is the full version

    G_UpdateAppTitle();

    if (g_scriptDebug)
        LOG_F(INFO, "CON compiler debug mode enabled (level %d).",g_scriptDebug);

#ifndef NETCODE_DISABLE
    if (g_networkMode == NET_SERVER || g_networkMode == NET_DEDICATED_SERVER)
    {
        ENetAddress address = { ENET_HOST_ANY, g_netPort, 0 };
        g_netServer = enet_host_create(&address, MAXPLAYERS, CHAN_MAX, 0, 0);

        if (g_netServer == NULL)
            VLOG_F(LOG_NET, "Unable to create ENet server host.");
        else VLOG_F(LOG_NET, "Multiplayer server initialized.");
    }
#endif
    numplayers = 1;
    g_mostConcurrentPlayers = ud.multimode;  // Lunatic needs this (player[] bound)

    if (!g_fakeMultiMode)
    {
        connectpoint2[0] = -1;
    }
    else
    {
        for (int i=0; i<ud.multimode-1; i++)
            connectpoint2[i] = i+1;
        connectpoint2[ud.multimode-1] = -1;

        for (int i=1; i<ud.multimode; i++)
            g_player[i].playerquitflag = 1;
    }

    Net_GetPackets();

    // NOTE: Allocating the DukePlayer_t structs has to be before compiling scripts,
    // because in Lunatic, the {pipe,trip}bomb* members are initialized.
    for (int i=0; i<MAXPLAYERS; i++)
        G_MaybeAllocPlayer(i);

    G_Startup(); // a bunch of stuff including compiling cons

    g_player[0].playerquitflag = 1;

    auto &myplayer = *g_player[myconnectindex].ps;

    myplayer.palette = BASEPAL;

    for (int i=1, j=numplayers; j<ud.multimode; j++)
    {
        Bsprintf(g_player[j].user_name,"PLAYER %d",j+1);
        g_player[j].ps->team = g_player[j].pteam = i;
        g_player[j].ps->weaponswitch = 3;
        g_player[j].ps->auto_aim = 0;
        i = 1-i;
    }

    Anim_Init();

    char const * const deffile = G_DefFile();
    uint32_t stime = timerGetTicks();
    if (!loaddefinitionsfile(deffile))
    {
        uint32_t etime = timerGetTicks();
        LOG_F(INFO, "Definitions file '%s' loaded in %d ms.", deffile, etime-stime);
    }
    loaddefinitions_game(deffile, FALSE);

    for (char * m : g_defModules)
        Xfree(m);
    g_defModules.clear();

    if (enginePostInit())
        G_FatalEngineInitError();

    G_PostLoadPalette();

    tileDelete(MIRROR);

    Gv_ResetSystemDefaults(); // called here to populate our fake tilesizx and tilesizy arrays presented to CON with sizes generated by dummytiles

    if (numplayers == 1 && boardfilename[0] != 0)
    {
        ud.m_level_number  = 7;
        ud.m_volume_number = 0;
        ud.warp_on         = 1;
    }

    // getnames();

    if (g_netServer || ud.multimode > 1)
    {
        if (ud.warp_on == 0)
        {
            ud.m_monsters_off = 1;
            ud.m_player_skill = 0;
        }
    }

    g_mostConcurrentPlayers = ud.multimode;  // XXX: redundant?

    ++ud.executions;
    CONFIG_WriteSetup(1);
    CONFIG_ReadSetup();

    char const * rtsname = g_rtsNamePtr ? g_rtsNamePtr : ud.rtsname;
    RTS_Init(rtsname);

    ud.last_level = -1;

    LOG_F(INFO, "Initializing console...");

    Bsprintf(tempbuf, HEAD2 " %s", s_buildRev);
    OSD_SetVersion(tempbuf, 10,0);
    OSD_SetParameters(0, 0, 0, 12, 2, 12, OSD_ERROR, OSDTEXT_RED, OSDTEXT_DARKRED, gamefunctions[gamefunc_Show_Console][0] == '\0' ? OSD_PROTECTED : 0);
    registerosdcommands();

    if (g_networkMode != NET_DEDICATED_SERVER)
    {
        if (CONTROL_Startup(controltype_keyboardandmouse, &BGetTime, TICRATE))
        {
            engineUnInit();
            fatal_exit("There was an error initializing the CONTROL system.");
        }

        G_SetupGameButtons();
        CONFIG_SetupMouse();
        CONFIG_SetupJoystick();

        CONTROL_JoystickEnabled = (ud.setup.usejoystick && CONTROL_JoyPresent);
        CONTROL_MouseEnabled    = (ud.setup.usemouse && CONTROL_MousePresent);
    }

#ifdef HAVE_CLIPSHAPE_FEATURE
    int const clipMapError = engineLoadClipMaps();
    if (clipMapError > 0)
        LOG_F(ERROR, "Error %d loading sprite clip map!", clipMapError);

    for (char * m : g_clipMapFiles)
        Xfree(m);
    g_clipMapFiles.clear();
#endif

    CONFIG_ReadSettings();

    OSD_Exec("autoexec.cfg");

    CONFIG_SetDefaultKeys(keydefaults, true);

    system_getcvars();

    if (quitevent) app_exit(4);

    if (g_networkMode != NET_DEDICATED_SERVER && validmodecnt > 0)
    {
        if (videoSetGameMode(ud.setup.fullscreen, ud.setup.xdim, ud.setup.ydim, ud.setup.bpp, ud.detail) < 0)
        {
            LOG_F(ERROR, "Failure setting video mode %dx%dx%d %s! Trying next mode...", ud.setup.xdim, ud.setup.ydim,
                       ud.setup.bpp, ud.setup.fullscreen ? "fullscreen" : "windowed");

            int resIdx = 0;

            for (int i=0; i < validmodecnt; i++)
            {
                if (validmode[i].xdim == ud.setup.xdim && validmode[i].ydim == ud.setup.ydim)
                {
                    resIdx = i;
                    break;
                }
            }

            int const savedIdx = resIdx;
            int bpp = ud.setup.bpp;

            while (videoSetGameMode(0, validmode[resIdx].xdim, validmode[resIdx].ydim, bpp, ud.detail) < 0)
            {
                LOG_F(ERROR, "Failure setting video mode %dx%dx%d windowed! Trying next mode...",
                           validmode[resIdx].xdim, validmode[resIdx].ydim, bpp);

                if (++resIdx >= validmodecnt)
                {
                    if (bpp == 8)
                        G_GameExit("Fatal error: unable to set any video mode!");

                    resIdx = savedIdx;
                    bpp = 8;
                }
            }

            ud.setup.xdim = validmode[resIdx].xdim;
            ud.setup.ydim = validmode[resIdx].ydim;
            ud.setup.bpp  = bpp;
        }

        videoSetPalette(ud.brightness>>2, myplayer.palette, 0);
        S_SoundStartup();
        S_MusicStartup();
    }

    G_InitText();

    if (g_networkMode != NET_DEDICATED_SERVER)
    {
        Menu_Init();
    }

    ReadSaveGameHeaders();

#if 0
    // previously, passing -0 through -9 on the command line would load the save in that slot #
    // this code should be reusable for a new parameter that takes a filename, if desired
    if (/* havesavename */ && (!g_netServer && ud.multimode < 2))
    {
        clearview(0L);
        //psmy.palette = palette;
        //G_FadePalette(0,0,0,0);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308
        rotatesprite_fs(160<<16,100<<16,65536L,0,LOADSCREEN,0,0,2+8+64+BGSTRETCH);
        menutext_center(105,"Loading saved game...");
        nextpage();

        if (G_LoadPlayer(/* savefile */))
            /* havesavename = false; */
    }
#endif

    if (ud.config.UseSoundPrecache)
    {
        LOG_F(INFO, "Precaching sound files...");
        cacheAllSounds();
    }

    FX_StopAllSounds();
    S_ClearSoundLocks();

    //    getpackets();
    //dukeCreateFrameRoutine();

    VM_OnEvent(EVENT_INITCOMPLETE);

MAIN_LOOP_RESTART:
    totalclock = 0;
    ototalclock = 0;
    lockclock = 0;

    myplayer.fta = 0;
    for (int32_t & q : user_quote_time)
        q = 0;

    Menu_Change(MENU_MAIN);

    if(g_netClient)
    {
        VLOG_F(LOG_NET, "Waiting for initial snapshot...");
        Net_WaitForInitialSnapshot();
    }

    if (g_networkMode != NET_DEDICATED_SERVER)
    {
        G_GetCrosshairColor();
        G_SetCrosshairColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);
    }

    if (myplayer.gm & MODE_NEWGAME)
    {
        G_NewGame(ud.m_volume_number, ud.m_level_number, ud.m_player_skill);
        myplayer.gm = MODE_RESTART;
    }
    else
    {
        if (ud.warp_on == 1)
        {
            G_NewGame_EnterLevel();
            // may change ud.warp_on in an error condition
        }

        if (ud.warp_on == 0)
        {
            if ((g_netServer || ud.multimode > 1) && boardfilename[0] != 0)
            {
                ud.m_level_number = 7;
                ud.m_volume_number = 0;
                ud.m_respawn_monsters = !!(ud.m_player_skill == 4);

                for (int TRAVERSE_CONNECT(i))
                {
                    P_ResetWeapons(i);
                    P_ResetInventory(i);
                }

                G_NewGame_EnterLevel();

                Net_WaitForServer();
            }
            else if (g_networkMode != NET_DEDICATED_SERVER)
                G_DisplayLogo();

            if (g_networkMode != NET_DEDICATED_SERVER)
            {
                if (G_PlaybackDemo())
                {
                    FX_StopAllSounds();
                    g_noLogoAnim = 1;
                    goto MAIN_LOOP_RESTART;
                }
            }
        }
        else G_UpdateScreenArea();
    }

//    ud.auto_run = ud.config.RunMode;
    ud.showweapons = ud.config.ShowWeapons;
    P_SetupMiscInputSettings();
    g_player[myconnectindex].pteam = ud.team;

    if (g_gametypeFlags[ud.coop] & GAMETYPE_TDM)
        myplayer.palookup = g_player[myconnectindex].pcolor = G_GetTeamPalette(g_player[myconnectindex].pteam);
    else
    {
        if (ud.color) myplayer.palookup = g_player[myconnectindex].pcolor = ud.color;
        else myplayer.palookup = g_player[myconnectindex].pcolor;
    }

    ud.warp_on = 0;
    KB_KeyDown[sc_Pause] = 0;   // JBF: I hate the pause key

    if(g_netClient)
    {
        ready2send = 1; // TESTING
    }

    do //main loop
    {
        if (gameHandleEvents() && quitevent)
        {
            KB_KeyDown[sc_Escape] = 1;
            quitevent = 0;
        }

        //if (g_restartFrameRoutine)
        //{
        //    dukeCreateFrameRoutine();
        //    g_restartFrameRoutine = 0;
        //}

        double gameUpdateStartTime = timerGetFractionalTicks();
        auto framecnt = g_frameCounter;

        if (((g_netClient || g_netServer) || (myplayer.gm & (MODE_MENU|MODE_DEMO)) == 0) && (int32_t)(totalclock - ototalclock) >= TICSPERFRAME)
        {
            do
            {
                do
                {
                    if (g_frameJustDrawn && g_networkMode != NET_DEDICATED_SERVER && (myplayer.gm & (MODE_MENU | MODE_DEMO)) == 0)
                        dukeFillInputForTic();

                    if (ready2send == 0)
                        break;

                    ototalclock += TICSPERFRAME;

                    if (((ud.show_help == 0 && (myplayer.gm & MODE_MENU) != MODE_MENU) || ud.recstat == 2 || (g_netServer || ud.multimode > 1))
                        && (myplayer.gm & MODE_GAME))
                    {
                        g_frameJustDrawn = false;
                        Net_GetPackets();
                        G_DoMoveThings();
                    }

                }
                while (((g_netClient || g_netServer) || (myplayer.gm & (MODE_MENU | MODE_DEMO)) == 0) && (int32_t)(totalclock - ototalclock) >= TICSPERFRAME && !g_saveRequested);

                g_gameUpdateTime = timerGetFractionalTicks() - gameUpdateStartTime;

                if (g_frameCounter != framecnt)
                    g_gameUpdateTime -= (double)g_lastFrameDuration * (g_frameCounter - framecnt) * 1000.0 / (double)timerGetNanoTickRate();

                if (g_gameUpdateAvgTime <= 0.0)
                    g_gameUpdateAvgTime = g_gameUpdateTime;

                g_gameUpdateAvgTime
                = ((GAMEUPDATEAVGTIMENUMSAMPLES - 1.f) * g_gameUpdateAvgTime + g_gameUpdateTime) / ((float)GAMEUPDATEAVGTIMENUMSAMPLES);
            } while (0);

        }

        g_gameUpdateAndDrawTime = g_gameUpdateTime + (double)g_lastFrameDuration * 1000.0 / (double)timerGetNanoTickRate();

        G_DoCheats();

        if (myplayer.gm & MODE_NEWGAME)
            goto MAIN_LOOP_RESTART;

        if (myplayer.gm & (MODE_EOL|MODE_RESTART))
        {
            switch (G_EndOfLevel())
            {
                case 1: continue;
                case 2: goto MAIN_LOOP_RESTART;
            }
        }

        if (g_networkMode == NET_DEDICATED_SERVER)
        {
            idle();
        }
        else if (engineFPSLimit((myplayer.gm & MODE_MENU) == MODE_MENU) || g_saveRequested)
        {
            if (!g_saveRequested)
            {
                // only allow binds to function if the player is actually in a game (not in a menu, typing, et cetera) or demo
                CONTROL_BindsEnabled = !!(myplayer.gm & (MODE_GAME|MODE_DEMO));
#ifndef NETCODE_DISABLE
                Net_DedicatedServerStdin();
#endif
            }

            //g_switchRoutine(co_drawframe);
            drawframe_do();
        }

        // handle CON_SAVE and CON_SAVENN
        if (g_saveRequested)
        {
            KB_FlushKeyboardQueue();

            g_screenCapture = 1;
            G_DrawRooms(myconnectindex, 65536);
            g_screenCapture = 0;

            G_SavePlayerMaybeMulti(g_lastautosave, true);
            g_quickload = &g_lastautosave;
            g_saveRequested = false;
            walock[TILE_SAVESHOT] = CACHE1D_UNLOCKED;
        }

        if (myplayer.gm & MODE_DEMO)
            goto MAIN_LOOP_RESTART;
    }
    while (1);

    app_exit(EXIT_SUCCESS);  // not reached (duh)
}

int G_DoMoveThings(void)
{
    ud.camerasprite = -1;
    lockclock += TICSPERFRAME;

    // Moved lower so it is restored correctly by demo diffs:
    //if (g_earthquakeTime > 0) g_earthquakeTime--;

    if (g_RTSPlaying > 0)
        g_RTSPlaying--;

    for (int32_t & i : user_quote_time)
    {
        if (i)
        {
            if (--i > ud.msgdisptime)
                i = ud.msgdisptime;
            if (!i) pub = NUMPAGES;
        }
    }
#ifndef NETCODE_DISABLE
    // Name display when aiming at opponents
    if (ud.idplayers && (g_netServer || ud.multimode > 1)
#ifdef SPLITSCREEN_MOD_HACKS
        && !g_fakeMultiMode
#endif
        )
    {
        hitdata_t hitData;
        auto const pPlayer = g_player[screenpeek].ps;

        for (bssize_t TRAVERSE_CONNECT(i))
            if (g_player[i].ps->holoduke_on != -1)
                sprite[g_player[i].ps->holoduke_on].cstat ^= 256;

        hitscan(&pPlayer->pos, pPlayer->cursectnum, sintable[(fix16_to_int(pPlayer->q16ang) + 512) & 2047],
                sintable[fix16_to_int(pPlayer->q16ang) & 2047], fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 11, &hitData,
                0xffff0030);

        for (bssize_t TRAVERSE_CONNECT(i))
            if (g_player[i].ps->holoduke_on != -1)
                sprite[g_player[i].ps->holoduke_on].cstat ^= 256;

        if ((hitData.sprite >= 0) && (g_player[myconnectindex].ps->gm & MODE_MENU) == 0 &&
                sprite[hitData.sprite].picnum == APLAYER)
        {
            int const playerNum = P_Get(hitData.sprite);

            if (playerNum != screenpeek && g_player[playerNum].ps->dead_flag == 0)
            {
                if (pPlayer->fta == 0 || pPlayer->ftq == QUOTE_RESERVED3)
                {
                    if (ldist(&sprite[pPlayer->i], &sprite[hitData.sprite]) < 9216)
                    {
                        Bsprintf(apStrings[QUOTE_RESERVED3], "%s", &g_player[playerNum].user_name[0]);
                        pPlayer->fta = 12, pPlayer->ftq = QUOTE_RESERVED3;
                    }
                }
                else if (pPlayer->fta > 2) pPlayer->fta -= 3;
            }
        }
    }
#endif
    if (g_showShareware > 0)
    {
        g_showShareware--;
        if (g_showShareware == 0)
        {
            pus = NUMPAGES;
            pub = NUMPAGES;
        }
    }

    // Moved lower so it is restored correctly by diffs:
//    everyothertime++;

    if (g_netClient) // [75] The server should not overwrite its own randomseed
        randomseed = ticrandomseed;

    for (bssize_t TRAVERSE_CONNECT(i))
        Bmemcpy(&g_player[i].input, &inputfifo[(g_netServer && myconnectindex == i)][i], sizeof(input_t));

    G_UpdateInterpolations();

    /*
        j = -1;
        for (TRAVERSE_CONNECT(i))
        {
            if (g_player[i].playerquitflag == 0 || TEST_SYNC_KEY(g_player[i].sync->bits,SK_GAMEQUIT) == 0)
            {
                j = i;
                continue;
            }

            G_CloseDemoWrite();

            g_player[i].playerquitflag = 0;
        }
    */

    g_moveThingsCount++;

    if (ud.recstat == 1) G_DemoRecord();

    everyothertime++;
    if (g_earthquakeTime > 0) g_earthquakeTime--;

    if (ud.pause_on == 0)
    {
        g_globalRandom = krand();
        A_MoveDummyPlayers();//ST 13
    }

    for (bssize_t TRAVERSE_CONNECT(i))
    {
        if (g_player[i].ps->team != g_player[i].pteam && g_gametypeFlags[ud.coop] & GAMETYPE_TDM)
        {
            g_player[i].ps->team = g_player[i].pteam;
            actor[g_player[i].ps->i].htpicnum = APLAYERTOP;
            P_QuickKill(g_player[i].ps);
        }

        if (g_gametypeFlags[ud.coop] & GAMETYPE_TDM)
            g_player[i].ps->palookup = g_player[i].pcolor = G_GetTeamPalette(g_player[i].ps->team);

        if (sprite[g_player[i].ps->i].pal != 1)
            sprite[g_player[i].ps->i].pal = g_player[i].pcolor;

        P_HandleSharedKeys(i);

        if (ud.pause_on == 0)
        {
            P_ProcessInput(i);
            P_CheckSectors(i);
        }
    }

    if (ud.pause_on == 0)
        G_MoveWorld();

//    Net_CorrectPrediction();

    if (g_netServer)
        Net_SendServerUpdates();

    if ((everyothertime&1) == 0)
    {
        G_AnimateWalls();
        A_MoveCyclers();

        if ((everyothertime % 10) == 0)
        {
            if(g_netServer)
            {
                Net_SendMapUpdate();
            }
            else if(g_netClient)
            {
                Net_StoreClientState();
            }
        }
    }

    if (g_netClient)   //Slave
        Net_SendClientUpdate();

    return 0;
}

#ifndef EDUKE32_STANDALONE
void A_SpawnWallGlass(int spriteNum, int wallNum, int glassCnt)
{
    if (wallNum < 0)
    {
        for (bssize_t j = glassCnt - 1; j >= 0; --j)
        {
            int const a = SA(spriteNum) - 256 + (krand() & 511) + 1024;
            A_InsertSprite(SECT(spriteNum), SX(spriteNum), SY(spriteNum), SZ(spriteNum), GLASSPIECES + (j % 3), -32, 36, 36, a,
                           32 + (krand() & 63), 1024 - (krand() & 1023), spriteNum, 5);
        }
        return;
    }

    vec2_t v1 = { wall[wallNum].x, wall[wallNum].y };
    vec2_t v  = { wall[wall[wallNum].point2].x - v1.x, wall[wall[wallNum].point2].y - v1.y };

    v1.x -= ksgn(v.y);
    v1.y += ksgn(v.x);

    v.x = tabledivide32_noinline(v.x, glassCnt+1);
    v.y = tabledivide32_noinline(v.y, glassCnt+1);

    int16_t sect = -1;

    for (int j = glassCnt; j > 0; --j)
    {
        v1.x += v.x;
        v1.y += v.y;

        updatesector(v1.x,v1.y,&sect);
        if (sect >= 0)
        {
            int z = sector[sect].floorz - (krand() & (klabs(sector[sect].ceilingz - sector[sect].floorz)));

            if (z < -ZOFFSET5 || z > ZOFFSET5)
                z = SZ(spriteNum) - ZOFFSET5 + (krand() & ((64 << 8) - 1));

            A_InsertSprite(SECT(spriteNum), v1.x, v1.y, z, GLASSPIECES + (j % 3), -32, 36, 36, SA(spriteNum) - 1024, 32 + (krand() & 63),
                           -(krand() & 1023), spriteNum, 5);
        }
    }
}

void A_SpawnGlass(int spriteNum, int glassCnt)
{
    for (; glassCnt>0; glassCnt--)
    {
        int const k
        = A_InsertSprite(SECT(spriteNum), SX(spriteNum), SY(spriteNum), SZ(spriteNum) - ((krand() & 16) << 8), GLASSPIECES + (glassCnt % 3),
                         krand() & 15, 36, 36, krand() & 2047, 32 + (krand() & 63), -512 - (krand() & 2047), spriteNum, 5);
        sprite[k].pal = sprite[spriteNum].pal;
    }
}

void A_SpawnCeilingGlass(int spriteNum, int sectNum, int glassCnt)
{
    int const startWall = sector[sectNum].wallptr;
    int const endWall = startWall+sector[sectNum].wallnum;

    for (bssize_t wallNum = startWall; wallNum < (endWall - 1); wallNum++)
    {
        vec2_t v1 = { wall[wallNum].x, wall[wallNum].y };
        vec2_t v  = { tabledivide32_noinline(wall[wallNum + 1].x - v1.x, glassCnt + 1),
                     tabledivide32_noinline(wall[wallNum + 1].y - v1.y, glassCnt + 1) };

        for (int j = glassCnt; j > 0; j--)
        {
            v1.x += v.x;
            v1.y += v.y;
            A_InsertSprite(sectNum, v1.x, v1.y, sector[sectNum].ceilingz + ((krand() & 15) << 8), GLASSPIECES + (j % 3), -32, 36, 36,
                           krand() & 2047, (krand() & 31), 0, spriteNum, 5);
        }
    }
}

void A_SpawnRandomGlass(int spriteNum, int wallNum, int glassCnt)
{
    if (wallNum < 0)
    {
        for (bssize_t j = glassCnt - 1; j >= 0; j--)
        {
            int const k
            = A_InsertSprite(SECT(spriteNum), SX(spriteNum), SY(spriteNum), SZ(spriteNum) - (krand() & (63 << 8)), GLASSPIECES + (j % 3),
                             -32, 36, 36, krand() & 2047, 32 + (krand() & 63), 1024 - (krand() & 2047), spriteNum, 5);
            sprite[k].pal = krand() & 15;
        }
        return;
    }

    vec2_t v1 = { wall[wallNum].x, wall[wallNum].y };
    vec2_t v  = { tabledivide32_noinline(wall[wall[wallNum].point2].x - wall[wallNum].x, glassCnt + 1),
                 tabledivide32_noinline(wall[wall[wallNum].point2].y - wall[wallNum].y, glassCnt + 1) };
    int16_t sectNum = sprite[spriteNum].sectnum;

    for (int j = glassCnt; j > 0; j--)
    {
        v1.x += v.x;
        v1.y += v.y;

        updatesector(v1.x, v1.y, &sectNum);

        int z = sector[sectNum].floorz - (krand() & (klabs(sector[sectNum].ceilingz - sector[sectNum].floorz)));

        if (z < -ZOFFSET5 || z > ZOFFSET5)
            z       = SZ(spriteNum) - ZOFFSET5 + (krand() & ((64 << 8) - 1));

        int const k = A_InsertSprite(SECT(spriteNum), v1.x, v1.y, z, GLASSPIECES + (j % 3), -32, 36, 36, SA(spriteNum) - 1024,
                                     32 + (krand() & 63), -(krand() & 2047), spriteNum, 5);
        sprite[k].pal = krand() & 7;
    }
}
#endif

static void G_SetupGameButtons(void)
{
    CONTROL_DefineFlag(gamefunc_Move_Forward,FALSE);
    CONTROL_DefineFlag(gamefunc_Move_Backward,FALSE);
    CONTROL_DefineFlag(gamefunc_Turn_Left,FALSE);
    CONTROL_DefineFlag(gamefunc_Turn_Right,FALSE);
    CONTROL_DefineFlag(gamefunc_Strafe,FALSE);
    CONTROL_DefineFlag(gamefunc_Fire,FALSE);
    CONTROL_DefineFlag(gamefunc_Open,FALSE);
    CONTROL_DefineFlag(gamefunc_Run,FALSE);
    CONTROL_DefineFlag(gamefunc_AutoRun,FALSE);
    CONTROL_DefineFlag(gamefunc_Jump,FALSE);
    CONTROL_DefineFlag(gamefunc_Crouch,FALSE);
    CONTROL_DefineFlag(gamefunc_Look_Up,FALSE);
    CONTROL_DefineFlag(gamefunc_Look_Down,FALSE);
    CONTROL_DefineFlag(gamefunc_Look_Left,FALSE);
    CONTROL_DefineFlag(gamefunc_Look_Right,FALSE);
    CONTROL_DefineFlag(gamefunc_Strafe_Left,FALSE);
    CONTROL_DefineFlag(gamefunc_Strafe_Right,FALSE);
    CONTROL_DefineFlag(gamefunc_Aim_Up,FALSE);
    CONTROL_DefineFlag(gamefunc_Aim_Down,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_1,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_2,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_3,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_4,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_5,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_6,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_7,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_8,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_9,FALSE);
    CONTROL_DefineFlag(gamefunc_Weapon_10,FALSE);
    CONTROL_DefineFlag(gamefunc_Inventory,FALSE);
    CONTROL_DefineFlag(gamefunc_Inventory_Left,FALSE);
    CONTROL_DefineFlag(gamefunc_Inventory_Right,FALSE);
    CONTROL_DefineFlag(gamefunc_Holo_Duke,FALSE);
    CONTROL_DefineFlag(gamefunc_Jetpack,FALSE);
    CONTROL_DefineFlag(gamefunc_NightVision,FALSE);
    CONTROL_DefineFlag(gamefunc_MedKit,FALSE);
    CONTROL_DefineFlag(gamefunc_TurnAround,FALSE);
    CONTROL_DefineFlag(gamefunc_SendMessage,FALSE);
    CONTROL_DefineFlag(gamefunc_Map,FALSE);
    CONTROL_DefineFlag(gamefunc_Shrink_Screen,FALSE);
    CONTROL_DefineFlag(gamefunc_Enlarge_Screen,FALSE);
    CONTROL_DefineFlag(gamefunc_Center_View,FALSE);
    CONTROL_DefineFlag(gamefunc_Holster_Weapon,FALSE);
    CONTROL_DefineFlag(gamefunc_Show_Opponents_Weapon,FALSE);
    CONTROL_DefineFlag(gamefunc_Map_Follow_Mode,FALSE);
    CONTROL_DefineFlag(gamefunc_See_Coop_View,FALSE);
    CONTROL_DefineFlag(gamefunc_Mouse_Aiming,FALSE);
    CONTROL_DefineFlag(gamefunc_Toggle_Crosshair,FALSE);
    CONTROL_DefineFlag(gamefunc_Steroids,FALSE);
    CONTROL_DefineFlag(gamefunc_Quick_Kick,FALSE);
    CONTROL_DefineFlag(gamefunc_Next_Weapon,FALSE);
    CONTROL_DefineFlag(gamefunc_Previous_Weapon,FALSE);
    CONTROL_DefineFlag(gamefunc_Alt_Fire,FALSE);
    CONTROL_DefineFlag(gamefunc_Last_Weapon,FALSE);
    CONTROL_DefineFlag(gamefunc_Quick_Save, FALSE);
    CONTROL_DefineFlag(gamefunc_Quick_Load, FALSE);
    CONTROL_DefineFlag(gamefunc_Alt_Weapon,FALSE);
    CONTROL_DefineFlag(gamefunc_Third_Person_View, FALSE);
    CONTROL_DefineFlag(gamefunc_Toggle_Crouch, FALSE);
}
