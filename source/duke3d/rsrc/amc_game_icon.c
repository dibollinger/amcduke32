#include "sdl_inc.h"
#include "sdlappicon.h"

static Uint8 sdlappicon_pixels[] = {
#if defined _WIN32 && SDL_MAJOR_VERSION==1
# include "amc_game_icon_32px.c"
#else
# include "amc_game_icon_48px.c"
#endif
};

struct sdlappicon sdlappicon = {
#if defined _WIN32 && SDL_MAJOR_VERSION==1
	32,32,
#else
	48,48,
#endif
	sdlappicon_pixels
};
