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

#include "anim.h"
#include "cmdline.h"
#include "demo.h"
#include "duke3d.h"
#include "menus.h"
#include "savegame.h"
#include "sbar.h"

#include "vfs.h"

static uint8_t precachehightile[2][bitmap_size(MAXTILES)];
static int32_t g_precacheCount;


static int32_t NET_75_CHECK = 0;

static void flag_precache(uint16_t tile, int32_t type)
{
    if (!bitmap_test(gotpic, tile))
        g_precacheCount++;

    bitmap_set(gotpic, tile);
    bitmap_set(precachehightile[type], tile);
}

static void tloadtile(uint16_t tilenume, int type)
{
    uint16_t firstTile, lastTile;

    if ((picanm[tilenume].sf & PICANM_ANIMTYPE_MASK) == PICANM_ANIMTYPE_BACK)
    {
        firstTile = tilenume - picanm[tilenume].num;
        lastTile  = tilenume;
    }
    else
    {
        firstTile = tilenume;
        lastTile  = tilenume + picanm[tilenume].num;
    }

    for (; firstTile <= lastTile; firstTile++)
        flag_precache(firstTile, type);
}

static void cacheTilesForSprite(int spriteNum)
{
    if (ud.monsters_off && A_CheckEnemySprite(&sprite[spriteNum])) return;

    uint16_t const picnum = sprite[spriteNum].picnum == RESPAWN ? sprite[spriteNum].hitag : sprite[spriteNum].picnum;
    int extraTiles = 1;

    for (uint16_t j = picnum; j <= g_tile[picnum].cacherange; j++)
        tloadtile(j, 1);

#ifndef EDUKE32_STANDALONE
    switch (tileGetMapping(picnum))
    {
    case HYDRENT__:
        tloadtile(BROKEFIREHYDRENT,1);
        for (int j = TOILETWATER; j < (TOILETWATER+4); j++) tloadtile(j,1);
        break;
    case TOILET__:
        tloadtile(TOILETBROKE,1);
        for (int j = TOILETWATER; j < (TOILETWATER+4); j++) tloadtile(j,1);
        break;
    case STALL__:
        tloadtile(STALLBROKE,1);
        for (int j = TOILETWATER; j < (TOILETWATER+4); j++) tloadtile(j,1);
        break;
    case RUBBERCAN__:
        extraTiles = 2;
        break;
    case TOILETWATER__:
        extraTiles = 4;
        break;
    case FEMPIC1__:
        extraTiles = 44;
        break;
    case LIZTROOP__:
    case LIZTROOPRUNNING__:
    case LIZTROOPSHOOT__:
    case LIZTROOPJETPACK__:
    case LIZTROOPONTOILET__:
    case LIZTROOPDUCKING__:
        for (int j = LIZTROOP; j < (LIZTROOP+72); j++) tloadtile(j,1);
        for (int j=HEADJIB1; j<LEGJIB1+3; j++) tloadtile(j,1);
        extraTiles = 0;
        break;
    case WOODENHORSE__:
        extraTiles = 5;
        for (int j = HORSEONSIDE; j < (HORSEONSIDE+4); j++) tloadtile(j,1);
        break;
    case NEWBEAST__:
    case NEWBEASTSTAYPUT__:
        extraTiles = 90;
        break;
    case BOSS1__:
    case BOSS2__:
    case BOSS3__:
    case SHARK__:
        extraTiles = 30;
        break;
    case OCTABRAIN__:
    case OCTABRAINSTAYPUT__:
    case COMMANDER__:
    case COMMANDERSTAYPUT__:
        extraTiles = 38;
        break;
    case RECON__:
        extraTiles = 13;
        break;
    case PIGCOP__:
    case PIGCOPDIVE__:
        extraTiles = 61;
        break;
    case LIZMAN__:
    case LIZMANSPITTING__:
    case LIZMANFEEDING__:
    case LIZMANJUMP__:
        for (int j=LIZMANHEAD1; j<LIZMANLEG1+3; j++) tloadtile(j,1);
        extraTiles = 80;
        break;
    case APLAYER__:
        extraTiles = 0;
        if ((g_netServer || ud.multimode > 1))
        {
            extraTiles = 5;
            for (int j = 1420; j < 1420+106; j++) tloadtile(j,1);
        }
        break;
    case ATOMICHEALTH__:
        extraTiles = 14;
        break;
    case DRONE__:
        extraTiles = 10;
        break;
    case EXPLODINGBARREL__:
    case SEENINE__:
    case OOZFILTER__:
        extraTiles = 3;
        break;
    case NUKEBARREL__:
    case CAMERA1__:
        extraTiles = 5;
        break;
        // caching of HUD sprites for weapons that may be in the level
    case CHAINGUNSPRITE__:
        for (int j=CHAINGUN; j<=CHAINGUN+7; j++) tloadtile(j,1);
        break;
    case RPGSPRITE__:
        tloadtile(WT_WIDE(RPGGUN), 1);
        for (int j=RPGGUN+1; j<=RPGGUN+2; j++) tloadtile(j,1);
        break;
    case FREEZESPRITE__:
        tloadtile(WT_WIDE(FREEZE), 1);
        tloadtile(WORLDTOUR ? FREEZEFIREWIDE : FREEZE+2, 1);
        for (int j=FREEZE+3; j<=FREEZE+5; j++) tloadtile(j,1);
        break;
    case GROWSPRITEICON__:
    case SHRINKERSPRITE__:
    {
        uint16_t const tile = WT_WIDE(SHRINKER);
        for (int j=tile-2; j<=tile+1; j++) tloadtile(j,1);
        for (int j=SHRINKER+2; j<=SHRINKER+5; j++) tloadtile(j,1);
        break;
    }
    case HBOMBAMMO__:
    case HEAVYHBOMB__:
        for (int j=HANDREMOTE; j<=HANDREMOTE+5; j++) tloadtile(j,1);
        break;
    case TRIPBOMBSPRITE__:
        for (int j=HANDHOLDINGLASER; j<=HANDHOLDINGLASER+4; j++) tloadtile(j,1);
        break;
    case SHOTGUNSPRITE__:
        tloadtile(SHOTGUNSHELL,1);
        for (int j=SHOTGUN; j<=SHOTGUN+6; j++) tloadtile(j,1);
        break;
    case DEVISTATORSPRITE__:
        for (int j=DEVISTATOR; j<=DEVISTATOR+1; j++) tloadtile(j,1);
        break;
    }
#endif

    for (int j = picnum; j < (picnum + extraTiles); j++)
        tloadtile(j, 1);
}

#ifndef EDUKE32_STANDALONE
static void cacheDukeTiles(void)
{
    tloadtile(sbartile(), 1);

    if ((g_netServer || ud.multimode > 1))
        tloadtile(FRAGBAR, 1);

    tloadtile(VIEWSCREEN, 1);

    for (int i = STARTALPHANUM; i < ENDALPHANUM+1; i++)
        tloadtile(i, 1);
    for (int i = BIGALPHANUM-11; i < BIGALPHANUM+82; i++)
        tloadtile(i, 1);
    for (int i = MINIFONT; i < MINIFONT+93; i++)
        tloadtile(i, 1);

    for (int i = FOOTPRINTS; i < FOOTPRINTS+3; i++)
        tloadtile(i, 1);

    for (int i = BURNING; i < BURNING+14; i++)
        tloadtile(i, 1);
    for (int i = BURNING2; i < BURNING2+14; i++)
        tloadtile(i, 1);

    for (int i = CRACKKNUCKLES; i < CRACKKNUCKLES+4; i++)
        tloadtile(i, 1);

    for (int i = FIRSTGUN; i < FIRSTGUN+3; i++)
        tloadtile(i, 1);
    tloadtile(FIRSTGUNRELOAD, 1);
    tloadtile(WORLDTOUR ? FIRSTGUNRELOADWIDE : FIRSTGUNRELOAD+1, 1);
    tloadtile(FIRSTGUNRELOAD+2, 1);
    tloadtile(FIRSTGUNRELOAD+4, 1);
    for (int i = SHELL; i < SHELL+2; i++)
        tloadtile(i, 1);

    for (int i = EXPLOSION2; i < EXPLOSION2+21; i++)
        tloadtile(i, 1);

    for (int i = COOLEXPLOSION1; i < COOLEXPLOSION1+21; i++)
        tloadtile(i, 1);

    tloadtile(BULLETHOLE, 1);
    tloadtile(BLOODPOOL, 1);

    for (int i = TRANSPORTERBEAM; i < (TRANSPORTERBEAM+6); i++)
        tloadtile(i, 1);

    for (int i = SMALLSMOKE; i < (SMALLSMOKE+4); i++)
        tloadtile(i, 1);
    for (int i = SHOTSPARK1; i < (SHOTSPARK1+4); i++)
        tloadtile(i, 1);

    for (int i = BLOOD; i < (BLOOD+4); i++)
        tloadtile(i, 1);
    for (int i = JIBS1; i < (JIBS5+5); i++)
        tloadtile(i, 1);
    for (int i = JIBS6; i < (JIBS6+8); i++)
        tloadtile(i, 1);

    for (int i = SCRAP1; i < (SCRAP1+29); i++)
        tloadtile(i, 1);

    tloadtile(FIRELASER, 1);

    for (int i = TRANSPORTERSTAR; i < TRANSPORTERSTAR+6; i++)
        tloadtile(i, 1);

    for (int i = FORCERIPPLE; i < (FORCERIPPLE+9); i++)
        tloadtile(i, 1);

    for (int i = MENUSCREEN; i < DUKECAR; i++)
        tloadtile(i, 1);

    for (int i = RPG; i < RPG+7; i++)
        tloadtile(i, 1);
    for (int i = FREEZEBLAST; i < FREEZEBLAST+3; i++)
        tloadtile(i, 1);
    for (int i = SHRINKSPARK; i < SHRINKSPARK+4; i++)
        tloadtile(i, 1);
    for (int i = GROWSPARK; i < GROWSPARK+4; i++)
        tloadtile(i, 1);
    for (int i = SHRINKEREXPLOSION; i < SHRINKEREXPLOSION+4; i++)
        tloadtile(i, 1);
    for (int i = MORTER; i < MORTER+4; i++)
        tloadtile(i, 1);
    for (int i = 0; i <= 60; i++)
        tloadtile(i, 1);
}
#endif

static void cacheFlaggedTiles(void)
{
    for (int i = 0; i < MAXTILES; i++)
    {
        if (g_tile[i].flags & SFLAG_PROJECTILE)
            tloadtile(i, 1);

        if (A_CheckSpriteTileFlags(i, SFLAG_CACHE))
            for (int j = i; j <= g_tile[i].cacherange; j++)
                tloadtile(j, 1);
    }

#ifndef EDUKE32_STANDALONE
    cacheDukeTiles();
#endif
}

static void G_DemoLoadScreen(const char *statustext, int const loadScreenTile, int percent)
{
    if (statustext == NULL)
    {
        videoClearScreen(0L);
        // g_player[myconnectindex].ps->palette = palette;
        // G_FadePalette(0,0,0,0);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);  // JBF 20040308
    }

    if ((unsigned)loadScreenTile < (MAXTILES<<1))
    {
        rotatesprite_fs(320<<15, 200<<15, 65536L, 0, loadScreenTile, 0, 0, 2+8+64+BGSTRETCH);
    }
    else
    {
        videoNextPage();
        return;
    }

    menutext_center(105, "Loading...");

    if (statustext)
        gametext_center_number(180, statustext);

    VM_OnEventWithReturn(EVENT_DISPLAYLOADINGSCREEN, g_player[screenpeek].ps->i, screenpeek, percent);
    videoNextPage();
}

static void G_DoLoadScreen(const char *statustext, int percent)
{
    int const loadScreenTile = VM_OnEventWithReturn(EVENT_GETLOADTILE, g_player[screenpeek].ps->i, screenpeek, LOADSCREEN);

    if (ud.recstat == 2)
    {
        G_DemoLoadScreen(statustext, loadScreenTile, percent);
        return;
    }

    int const screenSize = ud.screen_size;

    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);

    if (statustext == NULL)
    {
        ud.screen_size = 0;
        G_UpdateScreenArea();
        videoClearScreen(0L);
    }

    if ((unsigned)loadScreenTile < (MAXTILES<<1))
    {
        videoClearScreen(0);
        rotatesprite_fs(320<<15, 200<<15, 65536L, 0, loadScreenTile, 0, 0, 2+8+64+BGSTRETCH);
    }
    else
    {
        videoNextPage();
        return;
    }

    if (boardfilename[0] != 0 && ud.level_number == 7 && ud.volume_number == 0)
    {
        menutext_center(90, "Loading User Map");
        gametext_center_shade_pal(90+10, boardfilename, 14, 2);
    }
    else
    {
        menutext_center(90, "Loading");

        if (g_mapInfo[(ud.volume_number*MAXLEVELS) + ud.level_number].name != NULL)
            menutext_center(90+16+8, g_mapInfo[(ud.volume_number*MAXLEVELS) + ud.level_number].name);
    }

#ifndef EDUKE32_TOUCH_DEVICES
    if (statustext)
        gametext_center_number(180, statustext);
#endif

    if (percent != -1)
    {
        int const     width = scale(scale(xdim-1, 288, 320), percent, 100);
        int constexpr tile  = 929;
        int constexpr bits  = 2+8+16;

        rotatesprite(31<<16 , 145<<16, 65536, 0, tile, 15, 0, bits, 0, 0, width, ydim-1);
        rotatesprite(159<<16, 145<<16, 65536, 0, tile, 15, 0, bits, 0, 0, width, ydim-1);

        rotatesprite(30<<16 , 144<<16, 65536, 0, tile, 0, 0, bits, 0, 0, width, ydim-1);
        rotatesprite(158<<16, 144<<16, 65536, 0, tile, 0, 0, bits, 0, 0, width, ydim-1);
    }

    VM_OnEventWithReturn(EVENT_DISPLAYLOADINGSCREEN, g_player[screenpeek].ps->i, screenpeek, percent);
    videoNextPage();

    if (!statustext)
    {
        KB_FlushKeyboardQueue();
        ud.screen_size = screenSize;
    }
}

static void cacheExtraTextureMaps(uint16_t tileNum, int type)
{
#ifdef USE_OPENGL
    for (int i = 0; i < MAXPALOOKUPS-RESERVEDPALS-1; i++)
    {
#ifdef POLYMER
        if (videoGetRenderMode() != REND_POLYMER || !polymer_havehighpalookup(0, i))
#endif
            polymost_precache(tileNum, i, type);
    }
#else
    UNREFERENCED_PARAMETER(tileNum);
    UNREFERENCED_PARAMETER(type);
#endif

#ifdef USE_GLEXT
    if (r_detailmapping)
        polymost_precache(tileNum, DETAILPAL, type);

    if (r_glowmapping)
        polymost_precache(tileNum, GLOWPAL, type);
#endif
#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
    {
        if (pr_specularmapping)
            polymost_precache(tileNum, SPECULARPAL, type);

        if (pr_normalmapping)
            polymost_precache(tileNum, NORMALPAL, type);
    }
#endif
}

void G_CacheMapData(void)
{
    if (ud.recstat == 2 || !ud.config.useprecache)
        return;

    cacheAllSounds();

    g_precacheCount = 0;
    Bmemset(gotpic, 0, sizeof(gotpic));
    Bmemset(precachehightile, 0, sizeof(precachehightile));

    S_TryPlaySpecialMusic(MUS_LOADING);

#if defined EDUKE32_TOUCH_DEVICES && defined USE_OPENGL
    polymost_glreset();
#endif

    uint32_t const cacheStartTime = timerGetTicks();

    cacheFlaggedTiles();

    for (int i=0; i<numwalls; i++)
    {
        tloadtile(wall[i].picnum, 0);

        if (wall[i].overpicnum < MAXTILES)
            tloadtile(wall[i].overpicnum, 0);
    }

    for (int i=0; i<numsectors; i++)
    {
        tloadtile(sector[i].floorpicnum, 0);
        tloadtile(sector[i].ceilingpicnum, 0);

        for (int SPRITES_OF_SECT(i, j))
        {
            if (sprite[j].xrepeat != 0 && sprite[j].yrepeat != 0 && (sprite[j].cstat & CSTAT_SPRITE_INVISIBLE) == 0)
                cacheTilesForSprite(j);
        }
    }

    int cnt = 0;
    int cntDisplayed = -1;
    int pctDisplayed = -1;
    uint16_t i = 0;

    while (cnt < g_precacheCount)
    {
        if (bitmap_test(gotpic, i))
        {
            cnt++;

            if (waloff[i] == 0)
                tileLoad(i);

            for (int j = 0; j < 2; j++)
            {
                if (bitmap_test(precachehightile[j], i))
                {
                    tileLoadScaled(i);

                    if (videoGetRenderMode() != REND_CLASSIC)
                        cacheExtraTextureMaps(i, j);
                }
            }

            gameHandleEvents();
            if (KB_KeyPressed(sc_Space))
                break;
        }
        i++;

        if (cntDisplayed+(i&7) < cnt-(i&7) && engineFPSLimit(true))
        {
            int const percentComplete = min(100, tabledivide32(100 * cntDisplayed, g_precacheCount));
            cntDisplayed = logapproach(cntDisplayed, cnt);
            pctDisplayed = logapproach(pctDisplayed, percentComplete);
            Bsprintf(tempbuf, "Loaded %d%% (%d/%d textures)\n", pctDisplayed, cntDisplayed, g_precacheCount);
            G_DoLoadScreen(tempbuf, pctDisplayed);
        }
    }

    Bmemset(gotpic, 0, sizeof(gotpic));

    LOG_F(INFO, "Cache time: %dms.", timerGetTicks() - cacheStartTime);
}

int fragbarheight(void)
{
    if (ud.screen_size > 0 && !(ud.statusbarflags & STATUSBAR_NOFRAGBAR)
#ifdef SPLITSCREEN_MOD_HACKS
        && !g_fakeMultiMode
#endif
        && (g_netServer || ud.multimode > 1) && GTFLAGS(GAMETYPE_FRAGBAR))
    {
        int j = 0;

        for (int TRAVERSE_CONNECT(i))
            if (i > j)
                j = i;

        return ((j + 3) >> 2) * tilesiz[FRAGBAR].y;
    }

    return 0;
}

void G_UpdateScreenArea(void)
{
    if (!in3dmode())
        return;

    if ((ud.screen_size = clamp(ud.screen_size, 0, 64)) == 0)
        renderFlushPerms();

    int const screenSize    = max(ud.screen_size - 8, 0);
    int const bottomStatusY = tilesiz[sbartile()].y;

    vec2_t v1 = { scale(screenSize, xdim, 160),
                  scale(screenSize, (200 * 100) - (bottomStatusY * ud.statusbarscale), 200 - bottomStatusY) };
    vec2_t v2 = { xdim - v1.x, 200 * 100 - v1.y };

    v1.y += fragbarheight() * 100;

    if (ud.screen_size >= 8 && ud.statusbarmode == 0)
        v2.y -= bottomStatusY * ud.statusbarscale;

    v1.y = scale(v1.y, ydim, 200 * 100);
    v2.y = scale(v2.y, ydim, 200 * 100);

    if (VM_HaveEvent(EVENT_UPDATESCREENAREA))
    {
        ud.returnvar[0] = v1.y;
        ud.returnvar[1] = v2.x;
        ud.returnvar[2] = v2.y;
        v1.x = VM_OnEventWithReturn(EVENT_UPDATESCREENAREA, g_player[screenpeek].ps->i, screenpeek, v1.x);
        v1.y = ud.returnvar[0];
        v2.x = ud.returnvar[1];
        v2.y = ud.returnvar[2];
    }

    videoSetViewableArea(v1.x, v1.y, v2.x-1, v2.y-1);

    G_GetCrosshairColor();
    G_SetCrosshairColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);

    pub = NUMPAGES;
    pus = NUMPAGES;
}

void P_MoveToRandomSpawnPoint(int playerNum)
{
    auto &p = *g_player[playerNum].ps;
    int i = playerNum;

    if ((g_netServer || ud.multimode > 1) && !(g_gametypeFlags[ud.coop] & GAMETYPE_FIXEDRESPAWN))
    {
        i = krand() % g_playerSpawnCnt;

        if (g_gametypeFlags[ud.coop] & GAMETYPE_TDMSPAWN)
        {
            uint32_t pdist = INT_MAX;

            for (int TRAVERSE_CONNECT(j))
            {
                if (j == playerNum)
                    continue;

                auto const &op = *g_player[j].ps;

                // pick a spawn near a living teammate
                if (op.team == p.team && sprite[op.i].extra > 0)
                {
                    for (int k = 0; k < g_playerSpawnCnt; k++)
                    {
                        uint32_t const dist = FindDistance2D(op.pos.x - g_playerSpawnPoints[k].x,
                                                             op.pos.y - g_playerSpawnPoints[k].y);
                        if (dist < pdist)
                            i = k, pdist = dist;
                    }
                    break;
                }
            }
        }
    }

    p.opos = p.pos = g_playerSpawnPoints[i].xyz;

    p.bobpos     = p.pos.xy;
    p.cursectnum = g_playerSpawnPoints[i].sect;
    p.q16ang     = fix16_from_int(g_playerSpawnPoints[i].ang);

    sprite[p.i].cstat = CSTAT_SPRITE_BLOCK + CSTAT_SPRITE_BLOCK_HITSCAN;
}

static inline void P_ResetTintFade(DukePlayer_t *const pPlayer)
{
    pPlayer->pals.f = 0;
}

void P_ResetOffsets(DukePlayer_t *const pPlayer)
{
    pPlayer->floorzrebound = 768;
    pPlayer->floorzcutoff  = 256;

    pPlayer->floorzoffset  = 40 << 8;
    pPlayer->waterzoffset  = 34 << 8;
    pPlayer->minwaterzdist = 16 << 8;
    pPlayer->shrunkzoffset = 12 << 8;
    pPlayer->spritezoffset = 38 << 8 /*PHEIGHT*/;

    pPlayer->gravity = g_spriteGravity + 80;

    pPlayer->crouchzincrement    = 2048 + 768;
    pPlayer->crouchspeedmodifier = 8192;

    pPlayer->maxswimzvel       = 1536;
    pPlayer->minswimzvel       = 256;
    pPlayer->swimspeedmodifier = 5120;
    pPlayer->swimzincrement    = 348;

    pPlayer->jetpackzincrement = 2048;
}

void P_ResetExtents(DukePlayer_t* const pPlayer)
{
    pPlayer->autostep = 20 << 8;
    pPlayer->autostep_sbw = 4 << 8;
    pPlayer->clipdist = 164;
    pPlayer->runspeed = g_playerFriction;

    P_ResetOffsets(pPlayer);
}

void P_ResetMultiPlayer(int playerNum)
{
    auto &p = *g_player[playerNum].ps;

    Bassert((unsigned)p.i < MAXSPRITES);

    auto &s = sprite[p.i];
    auto &a = actor[p.i];

    vec3_t tmpvect = p.pos;

    tmpvect.z += p.spritezoffset;

    P_MoveToRandomSpawnPoint(playerNum);

    a.bpos = p.opos = p.pos;
    p.bobpos = p.pos.xy;

    s.xyz = p.pos;

    setsprite(p.i, &tmpvect);

    s.clipdist = 64;
    s.cstat    = 257;
    s.owner    = p.i;
    s.pal      = p.palookup;
    s.shade    = -12; // ???
    s.xoffset  = 0;
    s.xrepeat  = 42;
    s.yrepeat  = 36;

    p.last_extra = s.extra = p.max_player_health;
    p.inv_amount[GET_SHIELD] = g_startArmorAmount;

    p.dead_flag       = 0;
    p.falling_counter = 0;
    p.footprintcount  = 0;
    p.frag_ps         = playerNum;
    p.fta             = 0;
    p.ftq             = 0;
    p.on_crane        = -1;
    p.opyoff          = 0;
    p.q16horiz        = F16(100);
    p.q16horizoff     = 0;
    p.rotscrnang      = 0;
    p.vel             = { 0, 0, 0 };
    p.wackedbyactor   = -1;
    p.wantweaponfire  = -1;
    p.weapreccnt      = 0;

    P_ResetTintFade(&p);

    a.cgg       = 0;
    a.dispicnum = 0;
    a.htextra   = -1;
    a.movflag   = 0;
    a.htowner   = p.i;
    a.stayput   = -1;
    a.t_data[4] = 0;
    a.tempang   = 0;

    P_ResetInventory(playerNum);
    P_ResetWeapons(playerNum);

    p.reloading     = 0;
    p.movement_lock = 0;

    VM_OnEvent(EVENT_RESETPLAYER, p.i, playerNum);
}

void P_ResetPlayer(int playerNum)
{
    auto &thisPlayer = g_player[playerNum];
    auto &p          = *thisPlayer.ps;

    ud.show_help  = 0;
    ud.showallmap = 0;

    p.access_spritenum   = -1;
    p.actorsqu           = -1;
    p.airleft            = 15 * GAMETICSPERSEC;
    p.bobcounter         = 0;
    p.buttonpalette      = 0;
    p.cheat_phase        = 0;
    p.crack_time         = 0;
    p.dead_flag          = 0;
    p.dummyplayersprite  = -1;
    p.extra_extra8       = 0;
    p.falling_counter    = 0;
    p.fist_incs          = 0;
    p.footprintcount     = 0;
    p.footprintpal       = 0;
    p.footprintshade     = 0;
    p.frag               = 0;
    p.frag_ps            = playerNum;
    p.fraggedself        = 0;
    p.fric               = { 0, 0 };
    p.fta                = 0;
    p.ftq                = 0;
    p.got_access         = ((g_netServer || ud.multimode > 1) && (g_gametypeFlags[ud.coop] & GAMETYPE_ACCESSATSTART)) ? 7 : 0;
    p.hard_landing       = 0;
    p.hbomb_hold_delay   = 0;
    p.heat_on            = 0;
    p.holoduke_on        = -1;
    p.holster_weapon     = 0;
    p.hurt_delay         = 0;
    p.invdisptime        = 0;
    p.jetpack_on         = 0;
    p.jumping_counter    = 0;
    p.jumping_toggle     = 0;
    p.knee_incs          = 0;
    p.knuckle_incs       = 1;
    p.last_full_weapon   = 0;
    p.last_pissed_time   = 0;
    p.loogcnt            = 0;
    p.look_ang           = 512 - ((ud.level_number&1)<<10);
    p.movement_lock      = 0;
    p.newowner           = -1;
    p.on_crane           = -1;
    p.on_ground          = 0;
    p.on_warping_sector  = 0;
    p.one_eighty_count   = 0;
    p.opyoff             = 0;
    p.oq16horiz          = F16(140);
    p.orotscrnang        = 1;  // JBF 20031220
    p.over_shoulder_on   = 0;
    p.palette            = BASEPAL;
    p.player_par         = 0;
    p.pycount            = 0;
    p.pyoff              = 0;
    p.q16angvel          = 0;
    p.q16horiz           = F16(140);
    p.q16horizoff        = 0;
    p.quick_kick         = 0;
    p.random_club_frame  = 0;
    p.rapid_fire_hold    = 0;
    p.reloading          = 0;
    p.return_to_center   = 9;
    p.rotscrnang         = 0;
    p.sbs                = 0;
    p.show_empty_weapon  = 0;
    p.somethingonplayer  = -1;
    p.spritebridge       = 0;
    p.subweapon          = 0;
    p.tipincs            = 0;
    p.toggle_key_flag    = 0;
    p.transporter_hold   = 0;
    p.vel.x              = 0;
    p.vel.y              = 0;
    p.vel.z              = 0;
    p.wackedbyactor      = -1;
    p.walking_snd_toggle = 0;
    p.wantweaponfire     = -1;
    p.weapon_ang         = 0;
    p.weapon_pos         = WEAPON_POS_START;
    p.weapon_sway        = 0;

    pus = 1;

    if (p.inv_amount[GET_STEROIDS] < 400)
    {
        p.inv_amount[GET_STEROIDS] = 0;
        p.inven_icon = ICON_NONE;
    }

    p.kickback_pic = ((PWEAPON(playerNum, p.curr_weapon, WorksLike) == PISTOL_WEAPON)
                      && (PWEAPON(playerNum, p.curr_weapon, Reload) > PWEAPON(playerNum, p.curr_weapon, TotalTime)))
                     ? PWEAPON(playerNum, p.curr_weapon, TotalTime)
                     : 0;

    thisPlayer.smoothcamera     = false;
    thisPlayer.horizRecenter    = false;
    thisPlayer.horizSkew        = 0;
    thisPlayer.horizAngleAdjust = 0;

    P_ResetExtents(&p);
    P_UpdateScreenPal(&p);
    VM_OnEvent(EVENT_RESETPLAYER, p.i, playerNum);
}

void P_ResetWeapons(int playerNum)
{
    auto &p = *g_player[playerNum].ps;

    for (short & ammo : p.ammo_amount)
        ammo = 0;

    p.curr_weapon       = PISTOL_WEAPON;
    p.gotweapon         = ((1 << PISTOL_WEAPON) | (1 << KNEE_WEAPON) | (1 << HANDREMOTE_WEAPON));
    p.holster_weapon    = 0;
    p.kickback_pic      = PWEAPON(playerNum, p.curr_weapon, TotalTime);
    p.last_pissed_time  = 0;
    p.last_used_weapon  = -1;
    p.last_weapon       = -1;
    p.show_empty_weapon = 0;
    p.weapon_pos        = WEAPON_POS_START;

    p.ammo_amount[PISTOL_WEAPON] = min<int16_t>(p.max_ammo_amount[PISTOL_WEAPON], 48);

    VM_OnEvent(EVENT_RESETWEAPONS, p.i, playerNum);
}

void P_ResetInventory(int playerNum)
{
    auto &p = *g_player[playerNum].ps;

    Bmemset(p.inv_amount, 0, sizeof(p.inv_amount));

    p.heat_on     = 0;
    p.holoduke_on = -1;
    p.inven_icon  = ICON_NONE;
    p.jetpack_on  = 0;
    p.scuba_on    = 0;

    p.inv_amount[GET_SHIELD] = g_startArmorAmount;

    VM_OnEvent(EVENT_RESETINVENTORY, p.i, playerNum);
}

static void P_PrepForNewLevel(int playerNum, int gameMode)
{
    auto &p = *g_player[playerNum].ps;

    g_spriteDeleteQueuePos = 0;

    for (short &i : SpriteDeletionQueue)
        i = -1;

    g_animWallCnt      = 0;
    g_animateCnt       = 0;
    g_cyclerCnt        = 0;
    g_earthquakeTime   = 0;
    g_interpolationCnt = 0;

    for (int vscrIndex = 0; vscrIndex < MAX_ACTIVE_VIEWSCREENS; vscrIndex++)
    {
        g_activeVscrSprite[vscrIndex] = -1;
        g_activeVscrTile[vscrIndex] = -1;
    }

    randomseed  = 1996;
    screenpeek  = myconnectindex;
    tempwallptr = 0;

    p.actors_killed     = 0;
    p.cheat_phase       = 0;
    p.customexitsound   = 0;
    p.hbomb_on          = 0;
    p.holster_weapon    = 0;
    p.interface_toggle  = 0;
    p.last_pissed_time  = 0;
    p.last_weapon       = -1;
    p.max_actors_killed = 0;
    p.max_secret_rooms  = 0;
    p.parallax_sectnum  = -1;
    p.secret_rooms      = 0;
    p.show_empty_weapon = 0;
    p.timebeforeexit    = 0;
    p.toggle_key_flag   = 0;
    p.visibility        = ud.const_visibility;
    p.weapon_pos        = WEAPON_POS_START;
    p.weapreccnt        = 0;

    p.kickback_pic = ((PWEAPON(playerNum, p.curr_weapon, WorksLike) == PISTOL_WEAPON)
                      && (PWEAPON(playerNum, p.curr_weapon, Reload) > PWEAPON(playerNum, p.curr_weapon, TotalTime)))
                     ? PWEAPON(playerNum, p.curr_weapon, TotalTime)
                     : 0;

    ud.camerasprite = -1;
    ud.eog          = 0;
    ud.pause_on     = 0;

    if (((gameMode & MODE_EOL) != MODE_EOL && numplayers < 2 && !g_netServer)
        || (!(g_gametypeFlags[ud.coop] & GAMETYPE_PRESERVEINVENTORYDEATH) && numplayers > 1))
    {
        P_ResetWeapons(playerNum);
        P_ResetInventory(playerNum);
    }
    else if (PWEAPON(playerNum, p.curr_weapon, WorksLike) == HANDREMOTE_WEAPON)
    {
        p.ammo_amount[HANDBOMB_WEAPON]++;
        p.curr_weapon = HANDBOMB_WEAPON;
    }

    P_ResetTintFade(&p);
}

// Tweak sprites contained in moving sectors with these SE lotags.
#define FIXSPR_SELOTAGP(k) ((k) == SE_0_ROTATING_SECTOR || (k) == SE_6_SUBWAY || (k) == SE_14_SUBWAY_CAR)

// Set up sprites in moving sectors that are to be fixed wrt a certain pivot
// position and should not diverge from it due to roundoff error in the future.
// Has to be after the spawning stuff.
static void G_SetupRotfixedSprites(void)
{
    int spriteNum, nextSpriteNum;

    for (SPRITES_OF_STAT_SAFE(STAT_EFFECTOR, spriteNum, nextSpriteNum))
    {
        auto const &s = sprite[spriteNum];

        if (FIXSPR_SELOTAGP(s.lotag))
        {
#ifdef YAX_ENABLE
            int firstrun = 1;
#endif
            int sectSprite = headspritesect[s.sectnum];

            do
            {
                auto const &ss = sprite[sectSprite];
                auto       &a  = actor[sectSprite];

                // TRIPBOMB uses t_data[7] for its own purposes. Wouldn't be
                // too useful with moving sectors anyway
                if ((ROTFIXSPR_STATNUMP(ss.statnum) && ss.picnum != TRIPBOMB)
                    || ((ss.statnum == STAT_ACTOR || ss.statnum == STAT_ZOMBIEACTOR) && A_CheckSpriteFlags(sectSprite, SFLAG_ROTFIXED)))
                {
                    int const pivotSprite = (s.lotag == 0) ? s.owner : spriteNum;

                    if (sectSprite != spriteNum && sectSprite != pivotSprite && pivotSprite >= 0 && pivotSprite < MAXSPRITES)
                    {
                        // let's hope we don't step on anyone's toes here
                        a.t_data[7] = ROTFIXSPR_MAGIC | pivotSprite;  // 'rs' magic + pivot SE sprite index
                        a.t_data[8] = ss.x - sprite[pivotSprite].x;
                        a.t_data[9] = ss.y - sprite[pivotSprite].y;
                    }
                }

                sectSprite = nextspritesect[sectSprite];
#ifdef YAX_ENABLE
                if ((sectSprite < 0 && firstrun) && (s.lotag == SE_6_SUBWAY || s.lotag == SE_14_SUBWAY_CAR))
                {
                    firstrun   = 0;
                    sectSprite = actor[spriteNum].t_data[9];

                    if (sectSprite >= 0)
                        sectSprite = headspritesect[sectSprite];
                }
#endif
            }
            while (sectSprite >= 0);
        }
    }
}

static void G_SetupLightSwitches()
{
    auto tagbitmap = (uint8_t *)Xcalloc(bitmap_size(65536), 1);

    for (int nextSprite, SPRITES_OF_STAT_SAFE(STAT_DEFAULT, spriteNum, nextSprite))
    {
        auto &s = sprite[spriteNum];

        if (s.picnum <= 0)  // oob safety for switch below
            continue;

        for (int i = 0; i < 2; i++)
        {
            switch (tileGetMapping(s.picnum-1+i))
            {
                case DIPSWITCH__:
                case DIPSWITCH2__:
                case FRANKENSTINESWITCH__:
                case HANDSWITCH__:
                case LIGHTSWITCH__:
                case LIGHTSWITCH2__:
                case LOCKSWITCH1__:
                case POWERSWITCH1__:
                case POWERSWITCH2__:
                case PULLSWITCH__:
                case SLOTDOOR__:
                case SPACEDOORSWITCH__:
                case SPACELIGHTSWITCH__:
                    // the lower code only for the 'on' state (*)
                    if (i == 0)
                    {
                        uint16_t const tag = s.lotag;
                        bitmap_set(tagbitmap, tag);
                    }

                    break;
            }
        }
    }

    // initially 'on' SE 12 light (*)
    for (int nextSprite, SPRITES_OF_STAT_SAFE(STAT_EFFECTOR, j, nextSprite))
    {
        uint16_t const tag = sprite[j].hitag;

        if (sprite[j].lotag == SE_12_LIGHT_SWITCH && bitmap_test(tagbitmap, tag))
            actor[j].t_data[0] = 1;
    }

    DO_FREE_AND_NULL(tagbitmap);
}

static void G_SetupSpecialWalls(void)
{
    g_mirrorCount = 0;

    for (int i = 0; i < numwalls; i++)
    {
        auto &w = wall[i];

        if (w.overpicnum == MIRROR && (w.cstat & 32) != 0)
        {
            int const nextSectnum = w.nextsector;

            if ((nextSectnum >= 0) && sector[nextSectnum].ceilingpicnum != MIRROR)
            {
                if (g_mirrorCount > 63)
                {
                    G_GameExit("Too many mirrors (64 max.)");
                }

                sector[nextSectnum].ceilingpicnum = MIRROR;
                sector[nextSectnum].floorpicnum   = MIRROR;
                g_mirrorWall[g_mirrorCount]       = i;
                g_mirrorSector[g_mirrorCount]     = nextSectnum;
                g_mirrorCount++;
                continue;
            }
        }

        if (g_animWallCnt >= MAXANIMWALLS)
        {
            Bsprintf(tempbuf, "Too many 'anim' walls (%d max).", MAXANIMWALLS);
            G_GameExit(tempbuf);
        }

        auto &aw = animwall[g_animWallCnt];

        aw.tag     = 0;
        aw.wallnum = 0;

        switch (tileGetMapping(G_GetForcefieldPicnum(i)))
        {
            case FANSHADOW__:
            case FANSPRITE__:
                // w.cstat |= 65;
                aw.wallnum = i;
                g_animWallCnt++;
                break;

            case W_FORCEFIELD__:
                if (w.overpicnum == W_FORCEFIELD__)
                {
                    for (int j = 0; j < 3; j++)
                        tloadtile(W_FORCEFIELD + j, 0);
                }

                if (w.shade > 31)
                    w.cstat = 0;
                else
                    w.cstat |= FORCEFIELD_CSTAT | CSTAT_WALL_BLOCK;

                if (w.lotag && w.nextwall >= 0)
                    wall[w.nextwall].lotag = w.lotag;

                fallthrough__;
            case BIGFORCE__:
                aw.wallnum = i;
                g_animWallCnt++;
                continue;
        }

        w.extra = -1;

        switch (tileGetMapping(w.picnum))
        {
#ifndef EDUKE32_STANDALONE
            case FEMPIC1__:
            case FEMPIC2__:
            case FEMPIC3__:
                w.extra = w.picnum;

                if (ud.lockout)
                    w.picnum = (w.picnum == FEMPIC1) ? BLANKSCREEN : SCREENBREAK6;

                aw.tag     = w.picnum;
                aw.wallnum = i;
                g_animWallCnt++;
                break;
#endif

            case W_TECHWALL1__:
            case W_TECHWALL2__:
            case W_TECHWALL3__:
            case W_TECHWALL4__:
                aw.wallnum = i;
                g_animWallCnt++;
                break;
            case SCREENBREAK6__:
            case SCREENBREAK7__:
            case SCREENBREAK8__:
                for (int j = SCREENBREAK6; j < SCREENBREAK9; j++)
                    tloadtile(j, 0);

                aw.tag     = -1;
                aw.wallnum = i;
                g_animWallCnt++;
                break;

            case SCREENBREAK1__:
            case SCREENBREAK2__:
            case SCREENBREAK3__:
            case SCREENBREAK4__:
            case SCREENBREAK5__:
            //
            case SCREENBREAK9__:
            case SCREENBREAK10__:
            case SCREENBREAK11__:
            case SCREENBREAK12__:
            case SCREENBREAK13__:
            case SCREENBREAK14__:
            case SCREENBREAK15__:
            case SCREENBREAK16__:
            case SCREENBREAK17__:
            case SCREENBREAK18__:
            case SCREENBREAK19__:
                aw.tag     = w.picnum;
                aw.wallnum = i;
                g_animWallCnt++;
                break;
        }
    }

    // Invalidate textures in sector behind mirror
    for (int i = 0; i < g_mirrorCount; i++)
    {
        int const startWall = sector[g_mirrorSector[i]].wallptr;
        int const endWall   = startWall + sector[g_mirrorSector[i]].wallnum;

        for (int j = startWall; j < endWall; j++)
        {
            wall[j].picnum = wall[j].overpicnum = MIRROR;

            if (wall[g_mirrorWall[i]].pal == 4)
                wall[j].pal = 4;
        }
    }
}

static void A_MaybeProcessEffector(int spriteNum)
{
    switch (tileGetMapping(PN(spriteNum)))
    {
        case ACTIVATOR__:
        case ACTIVATORLOCKED__:
        case LOCATORS__:
        case MASTERSWITCH__:
        case MUSICANDSFX__:
        case RESPAWN__:
        case SECTOREFFECTOR__:
        case TOUCHPLATE__:
            sprite[spriteNum].cstat &= ~(CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN | CSTAT_SPRITE_ALIGNMENT_MASK);
            break;

        case GPSPEED__:
            // DELETE_AFTER_LOADACTOR. Must not change statnum.
            sector[SECT(spriteNum)].extra = SLT(spriteNum);
            break;

        case CYCLER__:
        {
            // DELETE_AFTER_LOADACTOR. Must not change statnum.
            if (g_cyclerCnt >= MAXCYCLERS)
            {
                Bsprintf(tempbuf, "\nToo many cycling sectors (%d max).", MAXCYCLERS);
                G_GameExit(tempbuf);
            }

            auto &cycler = g_cyclers[g_cyclerCnt];

            cycler[0] = SECT(spriteNum);
            cycler[1] = SLT(spriteNum);
            cycler[2] = SS(spriteNum);
            cycler[3] = sector[SECT(spriteNum)].floorshade;
            cycler[4] = SHT(spriteNum);
            cycler[5] = (SA(spriteNum) == 1536);

            g_cyclerCnt++;
            break;
        }
    }
}

static void G_SpawnAllSprites()
{
    // I don't know why this is separated, but I have better things to do than combine them and see what happens
    for (int i = 0; i < MAXSPRITES; i++)
    {
        if (sprite[i].statnum < MAXSTATUS && (PN(i) != SECTOREFFECTOR || SLT(i) != SE_14_SUBWAY_CAR))
            A_Spawn(-1, i);
    }

    for (int i = 0; i < MAXSPRITES; i++)
    {
        if (sprite[i].statnum < MAXSTATUS && PN(i) == SECTOREFFECTOR && SLT(i) == SE_14_SUBWAY_CAR)
            A_Spawn(-1, i);
    }
}

static void G_DeleteTempEffectors()
{
    for (int nextSprite, SPRITES_OF_STAT_SAFE(STAT_DEFAULT, i, nextSprite))
    {
        switch (tileGetMapping(PN(i)))
        {
            case GPSPEED__:
            case CYCLER__: A_DeleteSprite(i); break;
        }
    }
}

static void prelevel(int g)
{
    //Bmemset(zhit, 0, sizeof(zhit));
    Bmemset(show2dsector, 0, sizeof(show2dsector));
#ifdef LEGACY_ROR
    Bmemset(ror_protectedsectors, 0, sizeof(ror_protectedsectors));
#endif
    g_cloudCnt = 0;

    P_PrepForNewLevel(0, g);
    G_SetupGlobalPsky();

    // Reset actor vars for all sprites, regardless of statnum.
    for (int i = 0; i < Numsprites; i++)
        A_ResetVars(i);

    VM_OnEvent(EVENT_PRELEVEL);

    int missedCloudSectors = 0;

    auto &p0 = *g_player[0].ps;

    for (int i = 0; i < numsectors; i++)
    {
        auto &s = sector[i];

        s.extra = 256;

        switch (s.lotag)
        {
        case ST_20_CEILING_DOOR:
        case ST_22_SPLITTING_DOOR:
            if (s.floorz > s.ceilingz)
                s.lotag |= 32768u;
            continue;
        }

        if (s.ceilingstat&1)
        {
            if (s.ceilingpicnum == CLOUDYSKIES)
            {
                if (g_cloudCnt < ARRAY_SSIZE(g_cloudSect))
                    g_cloudSect[g_cloudCnt++] = i;
                else
                    missedCloudSectors++;
            }

            if (p0.parallax_sectnum == -1)
                p0.parallax_sectnum = i;
        }

        if (s.lotag == 32767) //Found a secret room
        {
            p0.max_secret_rooms++;
            continue;
        }
    }

    if (missedCloudSectors > 0)
        LOG_F(WARNING, "Map has %d unhandled CLOUDYSKIES ceilings.", missedCloudSectors);

    // NOTE: must be safe loop because callbacks could delete sprites.
    for (int nextSprite, SPRITES_OF_STAT_SAFE(STAT_DEFAULT, i, nextSprite))
    {
        A_LoadActor(i);
        VM_OnEvent(EVENT_LOADACTOR, i);

        A_MaybeProcessEffector(i);
    }

    // Delete some effector / effector modifier sprites AFTER the loop running
    // the LOADACTOR events. DELETE_AFTER_LOADACTOR.
    G_DeleteTempEffectors();

    G_SpawnAllSprites();
    G_SetupRotfixedSprites();
    G_SetupLightSwitches();
    G_SetupSpecialWalls();
}


void G_PlayE4Cutscene(void)
{
    S_PlaySpecialMusicOrNothing(MUS_BRIEFING);

    renderFlushPerms();
    videoSetViewableArea(0, 0, xdim-1, ydim-1);
    videoClearViewableArea(0L);
    videoNextPage();

    if (Anim_Play("vol41a.anm"))
        goto end_vol4a;

    videoClearViewableArea(0L);
    videoNextPage();

    if (Anim_Play("vol42a.anm"))
        goto end_vol4a;

    videoClearViewableArea(0L);
    videoNextPage();

    Anim_Play("vol43a.anm");

end_vol4a:
    videoClearViewableArea(0L);
    videoNextPage();

    FX_StopAllSounds();
}

void G_NewGame(int volumeNum, int levelNum, int skillNum)
{
    auto &p0 = *g_player[0].ps;

    gameHandleEvents();

    if (ud.skill_voice > 0 && ud.config.SoundToggle)
    {
        // this is not an error
        while (ud.skill_voice > 0 && !FX_SoundActive(ud.skill_voice)) gameHandleEvents();
        while (ud.skill_voice > 0 && FX_SoundActive(ud.skill_voice)) gameHandleEvents();
    }

    S_PauseSounds(false);
    S_StopAllSounds();
    S_Cleanup();

    ready2send = 0;

    if (ud.m_recstat != 2 && ud.last_level != -1 && !VM_OnEventWithReturn(EVENT_EXITGAMESCREEN, g_player[myconnectindex].ps->i, myconnectindex, 0)
        && (g_netServer || ud.multimode > 1) && (ud.coop & GAMETYPE_SCORESHEET))
        G_BonusScreen(1);

    g_showShareware = GAMETICSPERSEC*34;

    ud.from_bonus    = 0;
    ud.last_level    = -1;
    ud.level_number  = levelNum;
    ud.player_skill  = skillNum;
    ud.secretlevel   = 0;
    ud.skill_voice   = -1;
    ud.volume_number = volumeNum;

    g_lastAutoSaveArbitraryID = -1;
    g_lastautosave.reset();
    g_lastusersave.reset();
    g_quickload = nullptr;

    // we don't want the intro to play after the multiplayer setup screen
    if ((!g_netServer && ud.multimode < 2) && !Menu_HaveUserMap()
        && !VM_OnEventWithReturn(EVENT_NEWGAMESCREEN, g_player[myconnectindex].ps->i, myconnectindex, 0)
        && !levelNum && volumeNum == 3 && !ud.lockout && !(G_GetLogoFlags() & LOGO_NOE4CUTSCENE))
        G_PlayE4Cutscene();

#ifdef EDUKE32_TOUCH_DEVICES
    p0.zoom = 360;
#else
    p0.zoom = 768;
#endif
    p0.gm = 0;

    Menu_Close(0);

    Gv_ResetVars();
    Gv_InitWeaponPointers();
    Gv_RefreshPointers();
    Gv_ResetSystemDefaults();

    for (int i=0; i < (MAXVOLUMES*MAXLEVELS); i++)
        G_FreeMapState(i);

    if (ud.m_coop != 1)
    {
        for (int weaponNum = 0; weaponNum < MAX_WEAPONS; weaponNum++)
        {
            if (PWEAPON(0, weaponNum, WorksLike) == PISTOL_WEAPON)
            {
                p0.curr_weapon = weaponNum;
                p0.gotweapon |= (1 << weaponNum);
                p0.ammo_amount[weaponNum] = min<int16_t>(p0.max_ammo_amount[weaponNum], 48);
            }
            else if (PWEAPON(0, weaponNum, WorksLike) == KNEE_WEAPON || PWEAPON(0, weaponNum, WorksLike) == HANDREMOTE_WEAPON)
                p0.gotweapon |= (1 << weaponNum);
        }

        p0.last_weapon = -1;
    }

    display_mirror = 0;

    VM_OnEvent(EVENT_NEWGAME, g_player[screenpeek].ps->i, screenpeek);
}

static void G_CollectSpawnPoints(int gameMode)
{
    g_playerSpawnCnt = 0;
    //    circ = 2048/ud.multimode;

    for (int pindex = 0, pal = 9, nexti, SPRITES_OF_STAT_SAFE(STAT_PLAYER, i, nexti))
    {
        if (g_playerSpawnCnt == MAXPLAYERS)
            G_GameExit("\nToo many player sprites (max 16.)");

        auto &s     = sprite[i];
        auto &spawn = g_playerSpawnPoints[g_playerSpawnCnt];

        spawn.xyz  = s.xyz;
        spawn.ang  = s.ang;
        spawn.sect = s.sectnum;

        g_playerSpawnCnt++;

        if (pindex >= MAXPLAYERS)
        {
            A_DeleteSprite(i);
            i = nexti;
            continue;
        }

        s.clipdist = 64;
        s.owner    = i;
        s.shade    = 0;
        s.xoffset  = 0;
        s.xrepeat  = 42;
        s.yrepeat  = 36;

        s.cstat
        = (pindex < (!g_fakeMultiMode ? numplayers : ud.multimode)) ? CSTAT_SPRITE_BLOCK + CSTAT_SPRITE_BLOCK_HITSCAN : CSTAT_SPRITE_INVISIBLE;

        auto &plr = g_player[pindex];
        auto &p   = *plr.ps;

        if ((gameMode & MODE_EOL) != MODE_EOL || p.last_extra == 0)
        {
            p.last_extra = p.max_player_health;
            s.extra      = p.max_player_health;
        }
        else
            s.extra = p.last_extra;

        s.yvel = pindex;

        if (!plr.pcolor && (g_netServer || ud.multimode > 1) && !(g_gametypeFlags[ud.coop] & GAMETYPE_TDM))
        {
            if (s.pal == 0)
            {
                for (int TRAVERSE_CONNECT(k))
                {
                    if (pal == g_player[k].ps->palookup)
                    {
                        if (++pal > 16)
                            pal = 9;
                        k = 0;
                    }
                }

                plr.pcolor = s.pal = p.palookup = pal++;

                if (pal > 16)
                    pal = 9;
            }
            else
                plr.pcolor = p.palookup = s.pal;
        }
        else
        {
            int k = plr.pcolor;

            if (g_gametypeFlags[ud.coop] & GAMETYPE_TDM)
            {
                k      = G_GetTeamPalette(plr.pteam);
                p.team = plr.pteam;
            }

            s.pal = p.palookup = k;
        }

        p.frag_ps = pindex;

        actor[i].htowner = p.i = i;
        actor[i].bpos = p.opos = p.pos = s.xyz;
        p.bobpos = s.xy;

        p.oq16ang = p.q16ang = fix16_from_int(s.ang);

        p.cursectnum = s.sectnum;

        pindex++;

        i = nexti;
    }
}

static void G_ResetAllPlayers(void)
{
    uint8_t aimmode[MAXPLAYERS], autoaim[MAXPLAYERS], wswitch[MAXPLAYERS];
    DukeStatus_t tsbar[MAXPLAYERS];

    if (g_player[0].ps->cursectnum >= 0)  // < 0 may happen if we start a map in void space (e.g. testing it)
    {
        A_InsertSprite(g_player[0].ps->cursectnum,g_player[0].ps->pos.x,g_player[0].ps->pos.y,g_player[0].ps->pos.z,
                       APLAYER,0,0,0,fix16_to_int(g_player[0].ps->q16ang),0,0,0,10);
    }

    if (ud.recstat != 2)
    {
        for (int TRAVERSE_CONNECT(i))
        {
            auto &p = *g_player[i].ps;

            aimmode[i] = p.aim_mode;
            autoaim[i] = p.auto_aim;
            wswitch[i] = p.weaponswitch;

            if ((g_netServer || ud.multimode > 1) && (g_gametypeFlags[ud.coop] & GAMETYPE_PRESERVEINVENTORYDEATH) && ud.last_level >= 0)
            {
                for (int j = 0; j < MAX_WEAPONS; j++)
                    tsbar[i].ammo_amount[j] = p.ammo_amount[j];

                tsbar[i].gotweapon   = p.gotweapon;
                tsbar[i].curr_weapon = p.curr_weapon;
                tsbar[i].inven_icon  = p.inven_icon;
                Bmemcpy(tsbar[i].inv_amount, p.inv_amount, sizeof(tsbar[i].inv_amount));
            }
        }
    }

    P_ResetPlayer(0);

    for (int TRAVERSE_CONNECT(i))
    {
        auto &plr = g_player[i];

        Bmemset(plr.frags, 0, sizeof(plr.frags));

        if (i != 0)
            Bmemcpy(plr.ps, g_player[0].ps, sizeof(DukePlayer_t));
    }

    if (ud.recstat != 2)
    {
        for (int TRAVERSE_CONNECT(i))
        {
            auto &p = *g_player[i].ps;

            p.aim_mode     = aimmode[i];
            p.auto_aim     = autoaim[i];
            p.weaponswitch = wswitch[i];

            if ((g_netServer || ud.multimode > 1) && (g_gametypeFlags[ud.coop] & GAMETYPE_PRESERVEINVENTORYDEATH) && ud.last_level >= 0)
            {
                for (int j = 0; j < MAX_WEAPONS; j++)
                    p.ammo_amount[j] = tsbar[i].ammo_amount[j];

                p.gotweapon   = tsbar[i].gotweapon;
                p.curr_weapon = tsbar[i].curr_weapon;
                p.inven_icon  = tsbar[i].inven_icon;
                Bmemcpy(p.inv_amount, tsbar[i].inv_amount, sizeof(tsbar[i].inv_amount));
            }
        }
    }

    // take away the pistol if the player spawns on any of these textures
    for (int TRAVERSE_CONNECT(i))
    {
        auto &p = *g_player[i].ps;

        if (p.cursectnum >= 0)
        {
            switch (tileGetMapping(sector[p.cursectnum].floorpicnum))
            {
                case HURTRAIL__:
                case FLOORSLIME__:
                case FLOORPLASMA__:
                    P_ResetWeapons(i);
                    P_ResetInventory(i);

                    p.ammo_amount[PISTOL_WEAPON] = 0;
                    p.gotweapon &= ~(1 << PISTOL_WEAPON);
                    p.curr_weapon  = KNEE_WEAPON;
                    p.kickback_pic = 0;

                    break;
            }
        }
    }
}

void G_ResetTimers(bool saveMoveCnt)
{
    g_lastInputTicks = 0;
    totalclock = g_cloudClock = ototalclock = lockclock = 0;
    ready2send = 1;
    g_levelTextTime = 85;

    if (!saveMoveCnt)
        g_moveThingsCount = 0;
}

void G_ResetViewScreenData(void)
{
    for (int vscrIndex = 0; vscrIndex < MAX_ACTIVE_VIEWSCREENS; vscrIndex++)
    {
        if (g_activeVscrSprite[vscrIndex] >= 0)
        {
            actor[g_activeVscrSprite[vscrIndex]].t_data[0] = 0;
            actor[g_activeVscrSprite[vscrIndex]].t_data[1] = -1;
        }
    }
}

void G_ClearFIFO(void)
{
    // [JM] Black-hole any leftover inputs.
    // Prevents being spun around at map start if you move the mouse while loading.
    ControlInfo blackHole;
    CONTROL_GetInput(&blackHole);

    g_lastInputTicks = 0;
    localInput = {};
    Bmemset(&inputfifo, 0, sizeof(input_t) * MOVEFIFOSIZ * MAXPLAYERS);

    for (int p = 0; p < MAXPLAYERS; ++p)
    {
        auto &player = g_player[p];

        player.input   = {};
        player.vote    = 0;
        player.gotvote = 0;

        player.horizSkew        = 0;
        player.horizAngleAdjust = 0;
        player.lastViewUpdate   = 0;
    }
}

int G_FindLevelByFile(const char *fileName)
{
    int i = 0;

    for (auto &levelNum : g_mapInfo)
    {
        i++;

        if (levelNum.filename == NULL)
            continue;
        else if (!Bstrcasecmp(fileName, levelNum.filename))
            return i-1;
    }

    return -1;
}

#if 0
static void G_FadeLoad(int32_t r, int32_t g, int32_t b, int32_t start, int32_t end, int32_t step, int32_t ticwait, int32_t tc)
{
    int32_t m = (step < 0) ? -1 : 1;

    int32_t nexttic = totalclock;

    for (; m*start <= m*end; start += step)
    {
        while (totalclock < nexttic)
            sampletimer();
        nexttic += ticwait;

        if (KB_KeyPressed(sc_Space))
        {
            KB_ClearKeyDown(sc_Space);
            return;
        }

        setpalettefade(r,g,b,start);
        flushperms();
        G_DoLoadScreen(" ", tc);
    }
}
#endif

static int G_TryMapHack(const char *mhkfile)
{
    int const failure = engineLoadMHK(mhkfile);

    if (!failure)
        LOG_F(INFO, "Loaded %s", mhkfile);

    return failure;
}

static void G_LoadMapHack(char *outbuf, const char *filename)
{
    if (filename != NULL)
        Bstrcpy(outbuf, filename);

    append_ext_UNSAFE(outbuf, ".mhk");

    if (G_TryMapHack(outbuf) && usermaphacks != NULL)
    {
        auto pMapInfo = find_usermaphack();

        if (pMapInfo)
            G_TryMapHack(pMapInfo->mhkfile);
    }
}

// levnamebuf should have at least size BMAX_PATH
void G_SetupFilenameBasedMusic(char *nameBuf, const char *fileName)
{
    char *p;
    char const *exts[] = {
#ifdef HAVE_FLAC
        "flac",
#endif
#ifdef HAVE_VORBIS
        "ogg",
#endif
#ifdef HAVE_XMP
        "xm",
        "mod",
        "it",
        "s3m",
        "mtm",
#endif
        "mid"
    };

    Bstrncpy(nameBuf, fileName, BMAX_PATH);

    Bcorrectfilename(nameBuf, 0);

    if (NULL == (p = Bstrrchr(nameBuf, '.')))
    {
        p = nameBuf + Bstrlen(nameBuf);
        p[0] = '.';
    }

    for (auto & ext : exts)
    {
        buildvfs_kfd kFile;

        Bmemcpy(p+1, ext, Bstrlen(ext) + 1);

        if ((kFile = kopen4loadfrommod(nameBuf, 0)) != buildvfs_kfd_invalid)
        {
            kclose(kFile);
            realloc_copy(&g_mapInfo[USERMAPMUSICFAKESLOT].musicfn, nameBuf);
            return;
        }
    }

    char const * usermapMusic = g_mapInfo[MUS_USERMAP].musicfn;
    if (usermapMusic != nullptr)
    {
        realloc_copy(&g_mapInfo[USERMAPMUSICFAKESLOT].musicfn, usermapMusic);
        return;
    }

#ifndef EDUKE32_STANDALONE
    if (!FURY)
    {
        char const * e1l8 = g_mapInfo[7].musicfn;
        if (e1l8 != nullptr)
        {
            realloc_copy(&g_mapInfo[USERMAPMUSICFAKESLOT].musicfn, e1l8);
            return;
        }
    }
#endif
}

static void G_CheckIfStateless()
{
    for (bssize_t i = 0; i < (MAXVOLUMES * MAXLEVELS); i++)
    {
        map_t *const pMapInfo = &g_mapInfo[i];
        if (pMapInfo->savedstate != nullptr)
        {
            // buildprint("G_CheckIfStateless: no ", ud.volume_number, " ", ud.level_number, "\n");
            return;
        }
    }

    // buildprint("G_CheckIfStateless: yes ", ud.volume_number, " ", ud.level_number, "\n");
    ud.last_stateless_volume = ud.volume_number;
    ud.last_stateless_level = ud.level_number;
}

int G_EnterLevel(int gameMode)
{
    vote_map = vote_episode = voting = -1;

    ud.respawn_monsters  = ud.m_respawn_monsters;
    ud.respawn_items     = ud.m_respawn_items;
    ud.respawn_inventory = ud.m_respawn_inventory;
    ud.monsters_off      = ud.m_monsters_off;
    ud.coop              = ud.m_coop;
    ud.marker            = ud.m_marker;
    ud.ffire             = ud.m_ffire;
    ud.noexits           = ud.m_noexits;

    if ((gameMode & MODE_DEMO) != MODE_DEMO)
        ud.recstat = ud.m_recstat;
    if ((gameMode & MODE_DEMO) == 0 && ud.recstat == 2)
        ud.recstat = 0;

    if (g_networkMode != NET_DEDICATED_SERVER)
    {
        S_PauseSounds(false);
        S_StopAllSounds();
        S_ClearSoundLocks();
        FX_SetReverb(0);
        videoSetGameMode(ud.setup.fullscreen, ud.setup.xdim, ud.setup.ydim, ud.setup.bpp, upscalefactor);
    }

    if (Menu_HaveUserMap() || G_HaveUserMap())
    {
        Bcorrectfilename(boardfilename, 0);

        int levelNum = G_FindLevelByFile(boardfilename);

        if (levelNum != -1)
        {
            int volumeNum = levelNum;

            levelNum &= MAXLEVELS-1;
            volumeNum = (volumeNum - levelNum) / MAXLEVELS;

            ud.level_number = ud.m_level_number = levelNum;
            ud.volume_number = ud.m_volume_number = volumeNum;

            boardfilename[0] = 0;
        }
    }
    else
        boardfilename[0] = '\0';

    int const mapidx = (ud.volume_number * MAXLEVELS) + ud.level_number;

    Bassert((unsigned)mapidx < ARRAY_SIZE(g_mapInfo));

    auto &m = g_mapInfo[mapidx];

    if (VOLUMEONE || !Menu_HaveUserMap())
    {
        if (m.name == NULL || m.filename == NULL)
        {
            LOG_F(ERROR, "Map E%dL%d not defined!", ud.volume_number+1, ud.level_number+1);
            return 1;
        }
    }

    int const ssize = ud.screen_size;
    ud.screen_size = 0;

    G_DoLoadScreen("Loading map . . .", -1);
    G_UpdateScreenArea();
#ifdef POLYMER
    G_DeleteAllLights();
#endif

    ud.screen_size = ssize;

    G_UpdateAppTitle(Menu_HaveUserMap() ? boardfilename : m.name);

    auto   &p0 = *g_player[0].ps;
    int16_t playerAngle;

    char levelName[BMAX_PATH];
    NET_75_CHECK++; // a major problem with how STAT_NETALLOC works, is that loadboard loads sprites directly into the arrays and does not take from
                    // STAT_NETALLOC, even though the loaded sprites are very, very likely to be relevant to the netcode.

    if (!VOLUMEONE && G_HaveUserMap())
    {
        if (engineLoadBoard(boardfilename, 0, &p0.pos, &playerAngle, &p0.cursectnum) < 0)
        {
            LOG_F(ERROR, "Unable to load %s: file not found or has invalid version!", boardfilename);
            return 1;
        }

        G_LoadMapHack(levelName, boardfilename);
        G_SetupFilenameBasedMusic(levelName, boardfilename);
    }
    else if (engineLoadBoard(m.filename, VOLUMEONE, &p0.pos, &playerAngle, &p0.cursectnum) < 0)
    {
        LOG_F(ERROR, "Unable to load %s: file not found or has invalid version!", m.filename);
        return 1;
    }
    else
    {
        G_LoadMapHack(levelName, m.filename);
    }

    if ((unsigned)p0.cursectnum >= (unsigned)numsectors)
    {
        LOG_F(ERROR, "Unable to load %s: bad player start point!", G_HaveUserMap() ? boardfilename : m.filename);
        return 1;
    }

    p0.q16ang = fix16_from_int(playerAngle);

    NET_75_CHECK++; // resetpspritevars attempts to insert player 0's sprite, which isn't going to work because we don't have
                    // the STAT_NETALLOC sprites allocated yet.

    Net_NotifyNewGame();


    prelevel(gameMode);

    G_AlignWarpElevators();
    G_ResetAllPlayers();
    G_CollectSpawnPoints(gameMode);

    ud.playerbest = CONFIG_GetMapBestTime(Menu_HaveUserMap() ? boardfilename : m.filename, g_loadedMapHack.md4);

    // G_FadeLoad(0,0,0, 252,0, -28, 4, -1);
    G_CacheMapData();
    // G_FadeLoad(0,0,0, 0,252, 28, 4, -2);

    if (ud.recstat != 2)
    {
        if (Menu_HaveUserMap())
        {
            S_PlayLevelMusicOrNothing(USERMAPMUSICFAKESLOT);
        }
        else if (g_mapInfo[g_musicIndex].musicfn == NULL || m.musicfn == NULL ||
            strcmp(g_mapInfo[g_musicIndex].musicfn, m.musicfn) || g_musicSize == 0 || ud.last_level == -1)
        {
            S_PlayLevelMusicOrNothing(mapidx);
        }
        else
        {
            S_ContinueLevelMusic();
        }
    }

    if (gameMode & (MODE_GAME|MODE_EOL))
    {
        for (int TRAVERSE_CONNECT(i))
        {
            g_player[i].ps->gm = MODE_GAME;
            Menu_Close(i);
        }
    }
    else if (gameMode & MODE_RESTART)
        g_player[myconnectindex].ps->gm = (ud.recstat == 2) ? MODE_DEMO : MODE_GAME;

    if ((ud.recstat == 1) && (gameMode&MODE_RESTART) != MODE_RESTART)
        G_OpenDemoWrite();

#ifndef EDUKE32_TOUCH_DEVICES
    if (VOLUMEONE && ud.level_number == 0 && ud.recstat != 2)
        P_DoQuote(QUOTE_F1HELP,g_player[myconnectindex].ps);
#endif

    //Net_NotifyNewGame();
    Net_ResetPrediction();

    //g_player[myconnectindex].ps->palette = palette;
    //G_FadePalette(0,0,0,0);
    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308
    P_UpdateScreenPal(g_player[myconnectindex].ps);
    renderFlushPerms();

    everyothertime = 0;
    g_globalRandom = 0;

    ud.last_level = ud.level_number+1;

    G_ClearFIFO();

    for (int i=g_interpolationCnt-1; i>=0; i--) bakipos[i] = *curipos[i];

    G_ResetTimers(0);  // Here we go
    G_ResetViewScreenData();

    Bmemcpy(currentboardfilename, boardfilename, BMAX_PATH);

    G_CheckIfStateless();

    for (int TRAVERSE_CONNECT(i))
    {
        if (!VM_OnEventWithReturn(EVENT_ENTERLEVEL, g_player[i].ps->i, i, 0))
            break;
    }

    if (G_HaveUserMap())
        OSD_Printf(OSDTEXT_YELLOW "User Map: %s\n", boardfilename);
    else if (FURY)
        OSD_Printf(OSDTEXT_YELLOW "Entering: %s\n", m.name);
    else
        OSD_Printf(OSDTEXT_YELLOW "E%dL%d: %s\n", ud.volume_number + 1, ud.level_number + 1, m.name);

    g_restorePalette = -1;

    G_UpdateScreenArea();
    videoClearViewableArea(0L);
    G_DrawBackground();
    G_DrawRooms(myconnectindex,65536);
    if (g_netClient || g_netServer) // [75] : Initialize map states after map load
    {
        Net_InitMapStateHistory();
        Net_AddWorldToInitialSnapshot();
    }

    Net_WaitForServer();
    return 0;
}

void G_FreeMapState(int levelNum)
{
    auto &board = g_mapInfo[levelNum];

    if (board.savedstate == NULL)
        return;

    for (int j=0; j<g_gameVarCount; j++)
    {
        if (aGameVars[j].flags & SAVEGAMEMAPSTATEVARSKIPMASK)
            continue;

        if (aGameVars[j].flags & (GAMEVAR_PERPLAYER|GAMEVAR_PERACTOR))
            ALIGNED_FREE_AND_NULL(board.savedstate->vars[j]);
    }

    for (int j=0; j<g_gameArrayCount; j++)
    {
        if (aGameArrays[j].flags & SAVEGAMEARRAYSKIPMASK)
            continue;

        if (aGameArrays[j].flags & GAMEARRAY_RESTORE)
            ALIGNED_FREE_AND_NULL(board.savedstate->arrays[j]);
    }

    ALIGNED_FREE_AND_NULL(board.savedstate);
}
