// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

// CB: SDL imports
#include <SDL2/SDL.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#define POINTER_WARP_COUNTDOWN 1

// CB: SDL-relevent variables
SDL_Window *SDL_window = NULL;
SDL_Renderer *SDL_renderer = NULL;
SDL_Texture *SDL_doomTexture = NULL;
int8_t colors[256 * 3]; // CB: new palettes

// Fake mouse handling.
// This cannot work properly w/o DGA.
// Needs an invisible mouse cursor at least.
boolean grabMouse;
int doPointerWarp = POINTER_WARP_COUNTDOWN;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int multiply = 3;

//
//  Translates the key currently in X_event
//

int xlatekey(void)
{
    return 0;
}

int SDLKeyToDoom(int SDLkeycode)
{
    int rc = SDLkeycode;

    switch (rc) {
    case SDLK_LEFT:
        rc = KEY_LEFTARROW;
        break;
    case SDLK_RIGHT:
        rc = KEY_RIGHTARROW;
        break;
    case SDLK_DOWN:
        rc = KEY_DOWNARROW;
        break;
    case SDLK_UP:
        rc = KEY_UPARROW;
        break;
    case SDLK_ESCAPE:
        rc = KEY_ESCAPE;
        break;
    case SDLK_RETURN:
        rc = KEY_ENTER;
        break;
    case SDLK_TAB:
        rc = KEY_TAB;
        break;
    case SDLK_F1:
        rc = KEY_F1;
        break;
    case SDLK_F2:
        rc = KEY_F2;
        break;
    case SDLK_F3:
        rc = KEY_F3;
        break;
    case SDLK_F4:
        rc = KEY_F4;
        break;
    case SDLK_F5:
        rc = KEY_F5;
        break;
    case SDLK_F6:
        rc = KEY_F6;
        break;
    case SDLK_F7:
        rc = KEY_F7;
        break;
    case SDLK_F8:
        rc = KEY_F8;
        break;
    case SDLK_F9:
        rc = KEY_F9;
        break;
    case SDLK_F10:
        rc = KEY_F10;
        break;
    case SDLK_F11:
        rc = KEY_F11;
        break;
    case SDLK_F12:
        rc = KEY_F12;
        break;

    case SDLK_BACKSPACE:
    case SDLK_DELETE:
        rc = KEY_BACKSPACE;
        break;

    case SDLK_PAUSE:
        rc = KEY_PAUSE;
        break;

    case SDLK_KP_EQUALS:
    case SDLK_EQUALS:
        rc = KEY_EQUALS;
        break;

    case SDLK_KP_MINUS:
    case SDLK_MINUS:
        rc = KEY_MINUS;
        break;

    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        rc = KEY_RSHIFT;
        break;

    case KMOD_LCTRL:
    case KMOD_RCTRL:
        rc = KEY_RCTRL;
        break;

    case KMOD_ALT:
        rc = KEY_RALT;
        break;

    default:
        if (rc >= SDLK_SPACE && rc <= 0x7E) // tilde
            rc = rc - SDLK_SPACE + ' ';
        if (rc >= 'A' && rc <= 'Z')
            rc = rc - 'A' + 'a';
        break;
    }

    return rc;
}

void I_ShutdownGraphics(void)
{
    SDL_DestroyWindow(SDL_window);
    SDL_Quit();
}

//
// I_StartFrame
//
void I_StartFrame(void)
{
    // er?
}

static int lastmousex = 0;
static int lastmousey = 0;
boolean mousemoved = false;
boolean shmFinished;

boolean I_GetEvent(void)
{
    SDL_Event e;
    if (!SDL_PollEvent(&e)) {
        return false;
    }

    event_t event;

    // CB: grab the latest SDL event, and convert it into a doom one.
    switch (e.type) {
    case SDL_KEYDOWN:
        // The user has pressed a key
        event.type = ev_keydown;
        event.data1 = SDLKeyToDoom(e.key.keysym.sym);
        D_PostEvent(&event);
        break;

    case SDL_KEYUP:
        // The user has released a key
        event.type = ev_keyup;
        event.data1 = SDLKeyToDoom(e.key.keysym.sym);
        D_PostEvent(&event);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        event.type = ev_mouse;
        event.data1 = (uint8_t)SDL_GetMouseState(NULL, NULL);
        event.data2 = event.data3 = 0;
        D_PostEvent(&event);
        break;

    case SDL_MOUSEMOTION:
        event.type = ev_mouse;

        int relX = e.motion.xrel / multiply; // Scale mouse coordinates back down to 320x200
        int relY = e.motion.yrel / multiply; // ditto.

        event.data1 = (uint8_t)SDL_GetMouseState(NULL, NULL);
        event.data2 = relX << 4;
        event.data3 = -relY << 4;

        if (event.data2 || event.data3) {
            D_PostEvent(&event);
        }
        break;

    default:
        break;
    }

    return true;
}

//
// I_StartTic
//
void I_StartTic(void)
{
    if (SDL_window == NULL)
        return;

    while (I_GetEvent())
        ;
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate(void)
{
    // CB: write the image to the screen
    //  Clear the screen
    SDL_SetRenderDrawColor(SDL_renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(SDL_renderer);

    byte *pixels;
    int pitch = SCREENWIDTH * 3; // bytes per row

    SDL_LockTexture(SDL_doomTexture, NULL, &pixels, &pitch);

    // Set each pixel of the output texture to the palette, using screens[0] as an index
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++) {
        pixels[i * 3] = colors[screens[0][i] * 3];
        pixels[i * 3 + 1] = colors[screens[0][i] * 3 + 1];
        pixels[i * 3 + 2] = colors[screens[0][i] * 3 + 2];
    }

    SDL_UnlockTexture(SDL_doomTexture);

    // And render the pixel array. This is where we scale to the appropriate size.
    SDL_Rect dest = { 0, 0, SCREENWIDTH * multiply, SCREENHEIGHT * multiply };
    SDL_RenderCopy(SDL_renderer, SDL_doomTexture, NULL, &dest);
    SDL_RenderPresent(SDL_renderer);
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
    memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette(byte *palette)
{
    int c;
    for (int i = 0; i < 256 * 3; i++) {
        c = gammatable[usegamma][*palette++];
        colors[i] = (c << 8) + c;
    }
}

//
// This function is probably redundant,
//  if XShmDetach works properly.
// ddt never detached the XShm memory,
//  thus there might have been stale
//  handles accumulating.
//
void grabsharedmemory(int size)
{
    return;
}

void I_InitGraphics(void)
{
    // CB: a whole new SDL window-opening routine!
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        I_Error("SDL failed to initalize. Error: %s\n", SDL_GetError());
        return;
    }

    SDL_window = SDL_CreateWindow("DOOM-SDL-port", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREENWIDTH * multiply, SCREENHEIGHT * multiply, SDL_WINDOW_SHOWN);
    if (SDL_window == NULL) {
        I_Error("Window could not be created. Error: %s\n", SDL_GetError());
        return;
    }

    SDL_renderer = SDL_CreateRenderer(SDL_window, -1, SDL_RENDERER_ACCELERATED);
    if (SDL_renderer == NULL) {
        I_Error("Renderer could not be created. Error: %s\n", SDL_GetError());
        return;
    }

    SDL_doomTexture = SDL_CreateTexture(SDL_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SCREENWIDTH, SCREENHEIGHT);
    if (SDL_doomTexture == NULL) {
        I_Error("Texture could not be created. Error: %s\n", SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0); // set scaling to linear interpolation
    SDL_SetRelativeMouseMode(SDL_TRUE);            // lock cursor.
}

unsigned exptable[256];

void InitExpand(void)
{
    int i;

    for (i = 0; i < 256; i++)
        exptable[i] = i | (i << 8) | (i << 16) | (i << 24);
}

double exptable2[256 * 256];

void InitExpand2(void)
{
    int i;
    int j;
    // UNUSED unsigned	iexp, jexp;
    double *exp;
    union {
        double d;
        unsigned u[2];
    } pixel;

    printf("building exptable2...\n");
    exp = exptable2;
    for (i = 0; i < 256; i++) {
        pixel.u[0] = i | (i << 8) | (i << 16) | (i << 24);
        for (j = 0; j < 256; j++) {
            pixel.u[1] = j | (j << 8) | (j << 16) | (j << 24);
            *exp++ = pixel.d;
        }
    }
    printf("done.\n");
}

int inited;

void Expand4(unsigned *lineptr, double *xline)
{
    double dpixel;
    unsigned x;
    unsigned y;
    unsigned fourpixels;
    unsigned step;
    double *exp;

    exp = exptable2;
    if (!inited) {
        inited = 1;
        InitExpand2();
    }

    step = 3 * SCREENWIDTH / 2;

    y = SCREENHEIGHT - 1;
    do {
        x = SCREENWIDTH;

        do {
            fourpixels = lineptr[0];

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff0000) >> 13));
            xline[0] = dpixel;
            xline[160] = dpixel;
            xline[320] = dpixel;
            xline[480] = dpixel;

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff) << 3));
            xline[1] = dpixel;
            xline[161] = dpixel;
            xline[321] = dpixel;
            xline[481] = dpixel;

            fourpixels = lineptr[1];

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff0000) >> 13));
            xline[2] = dpixel;
            xline[162] = dpixel;
            xline[322] = dpixel;
            xline[482] = dpixel;

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff) << 3));
            xline[3] = dpixel;
            xline[163] = dpixel;
            xline[323] = dpixel;
            xline[483] = dpixel;

            fourpixels = lineptr[2];

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff0000) >> 13));
            xline[4] = dpixel;
            xline[164] = dpixel;
            xline[324] = dpixel;
            xline[484] = dpixel;

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff) << 3));
            xline[5] = dpixel;
            xline[165] = dpixel;
            xline[325] = dpixel;
            xline[485] = dpixel;

            fourpixels = lineptr[3];

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff0000) >> 13));
            xline[6] = dpixel;
            xline[166] = dpixel;
            xline[326] = dpixel;
            xline[486] = dpixel;

            dpixel = *(double *)((int)exp + ((fourpixels & 0xffff) << 3));
            xline[7] = dpixel;
            xline[167] = dpixel;
            xline[327] = dpixel;
            xline[487] = dpixel;

            lineptr += 4;
            xline += 8;
        } while (x -= 16);
        xline += step;
    } while (y--);
}
