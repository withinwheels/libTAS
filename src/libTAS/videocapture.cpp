/*
    Copyright 2015-2016 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "videocapture.h"

#ifdef LIBTAS_ENABLE_AVDUMPING

#include "hook.h"
#include "logging.h"
#include "../external/gl.h" // glReadPixels enum arguments
#include "../external/SDL.h" // SDL_Surface
#include <vector>
#include <string.h> // memcpy


bool useGL;

/* Temporary pixel arrays */
std::vector<uint8_t> glpixels;
std::vector<uint8_t> winpixels;

/* Original function pointers */
void (*SDL_GL_GetDrawableSize_real)(void* window, int* w, int* h);
SDL_Surface* (*SDL_GetWindowSurface_real)(void* window);
SDL1::SDL_Surface* (*SDL_GetVideoSurface_real)(void);
int (*SDL_LockSurface_real)(SDL1::SDL_Surface* surface);
void (*SDL_UnlockSurface_real)(SDL1::SDL_Surface* surface);
int (*SDL_RenderReadPixels_real)(void*, const SDL_Rect*, Uint32, void*, int);
Uint32 (*SDL_GetWindowPixelFormat_real)(void* window);
void* (*SDL_GetRenderer_real)(void* window);
int (*SDL_GetRendererOutputSize_real)(void* renderer, int* w, int* h);

void (*glReadPixels_real)(int x, int y, int width, int height, unsigned int format, unsigned int type, void* data);

/* Video dimensions */
int width, height;
int size;

void* renderer;
int bpp = 0;

int initVideoCapture(void* window, bool video_opengl, int *pwidth, int *pheight)
{
    /* If the game uses openGL, the method to capture the screen will be different */
    useGL = video_opengl;

    /* Link the required functions, and get the window dimensions */
    if (SDLver == 1) {
        LINK_SUFFIX_SDL1(SDL_GetVideoSurface);
        if (useGL)
            LINK_SUFFIX(glReadPixels, "libGL");
        else {
            LINK_SUFFIX_SDL1(SDL_LockSurface);
            LINK_SUFFIX_SDL1(SDL_UnlockSurface);
        }

        /* Get dimensions from the window surface */
        if (!SDL_GetVideoSurface_real) {
            debuglog(LCF_DUMP | LCF_SDL | LCF_ERROR, "Need function SDL_GetVideoSurface.");
            return -1;
        }

        SDL1::SDL_Surface *surf = SDL_GetVideoSurface_real();
        
        /* Currently only supporting ARGB (32 bpp) pixel format */
        bpp = surf->format->BitsPerPixel;
        
        *pwidth = surf->w;
        *pheight = surf->h;
    }
    else if (SDLver == 2) {

        LINK_SUFFIX_SDL2(SDL_GetWindowPixelFormat);
        Uint32 pixelfmt = SDL_GetWindowPixelFormat_real(window);
        bpp = (pixelfmt >> 8) & 0xFF;
        debuglog(LCF_DUMP | LCF_SDL, "Dumped video has bpp of ", bpp);

        if (useGL) {
            LINK_SUFFIX_SDL2(SDL_GL_GetDrawableSize);
            LINK_SUFFIX(glReadPixels, "libGL");

            /* Get information about the current screen */
            if (!SDL_GL_GetDrawableSize_real) {
                debuglog(LCF_DUMP | LCF_SDL | LCF_ERROR, "Need function SDL_GL_GetDrawableSize.");
                return -1;
            }

            SDL_GL_GetDrawableSize_real(window, pwidth, pheight);
        }
        else {
            LINK_SUFFIX_SDL2(SDL_RenderReadPixels);
            LINK_SUFFIX_SDL2(SDL_GetRenderer);
            LINK_SUFFIX_SDL2(SDL_GetRendererOutputSize);

            /* Get surface from window */
            if (!SDL_GetRendererOutputSize_real) {
                debuglog(LCF_DUMP | LCF_SDL | LCF_ERROR, "Need function SDL_GetWindowSize.");
                return -1;
            }

            renderer = SDL_GetRenderer_real(window);
            SDL_GetRendererOutputSize_real(renderer, pwidth, pheight);
        }
    }
    else {
        debuglog(LCF_DUMP | LCF_SDL | LCF_ERROR, "Unknown SDL version.");
        return -1;
    }

    /* Check bpp */
    if ((bpp != 32) && (bpp != 24)) {
        debuglog(LCF_DUMP | LCF_ERROR, "Unsupported pixel format of bpp ", bpp);
        return -1;
    }

    /* Save dimensions for later */
    width = *pwidth;
    height = *pheight;

    /* Allocate an array of pixels */
    size = width * height * (bpp/8);
    winpixels.resize(size);

    /* Dimensions must be a multiple of 2 */
    if ((width % 1) || (height % 1)) {
        debuglog(LCF_DUMP | LCF_ERROR, "Screen dimensions must be a multiple of 2");
        return -1;
    }

    if (useGL) {
        /* Do we already have access to the glReadPixels function? */
        if (!glReadPixels_real) {
            debuglog(LCF_DUMP | LCF_OGL | LCF_ERROR, "Could not load function glReadPixels.");
            return -1;
        }

        /* Allocate another pixels array,
         * because the image will need to be flipped.
         */
        glpixels.resize(size);
    }

    return bpp;
}

int captureVideoFrame(void *window, const uint8_t* orig_plane[], int orig_stride[])
{
    int pitch = (bpp/8) * width;

    if (useGL) {
        /* TODO: Check that the openGL dimensions did not change in between */

        /* We access to the image pixels directly using glReadPixels */
        int pixfmt = (bpp==32)?GL_RGBA:GL_RGB;
        glReadPixels_real(0, 0, width, height, pixfmt, GL_UNSIGNED_BYTE, &glpixels[0]);
        /* TODO: I saw this in some examples before calling glReadPixels: glPixelStorei(GL_PACK_ALIGNMENT, 1); */

        /*
         * Flip image horizontally
         * This is because OpenGL has a different reference point
         * Code taken from http://stackoverflow.com/questions/5862097/sdl-opengl-screenshot-is-black
         * TODO: Could this be done without allocating another array ?
         */

        for (int line = 0; line < height; line++) {
            int pos = line * pitch;
            for (int p=0; p<pitch; p++) {
                winpixels[pos + p] = glpixels[(size-pos)-pitch + p];
            }
        }

    }

    else {
        /* Not tested !! */
        debuglog(LCF_DUMP | LCF_UNTESTED | LCF_FRAME, "Access SDL_Surface pixels for video dump");

        if (SDLver == 1) {
            /* Get surface from window */
            SDL1::SDL_Surface* surf1 = SDL_GetVideoSurface_real();

            /* Checking for a size modification */
            int cw = surf1->w;
            int ch = surf1->h;
            if ((cw != width) || (ch != height)) {
                debuglog(LCF_DUMP | LCF_ERROR, "Window coords have changed (",width,",",height,") -> (",cw,",",ch,")");
                return 1;
            }

            /* We must lock the surface before accessing the raw pixels */
            int ret = SDL_LockSurface_real(surf1);
            if (ret != 0) {
                debuglog(LCF_DUMP | LCF_ERROR, "Could not lock SDL surface");
                return 1;
            }

            /* I know memcpy is not recommended for vectors... */
            memcpy(&winpixels[0], surf1->pixels, size);

            /* Unlock surface */
            SDL_UnlockSurface_real(surf1);
        }

        if (SDLver == 2) {
            /* Checking for a size modification */
            int cw, ch;
            SDL_GetRendererOutputSize_real(renderer, &cw, &ch);
            if ((cw != width) || (ch != height)) {
                debuglog(LCF_DUMP | LCF_ERROR, "Window coords have changed (",width,",",height,") -> (",cw,",",ch,")");
                return 1;
            }

            SDL_RenderReadPixels_real(renderer, NULL, 0, &winpixels[0], pitch);
        }
    }

    orig_plane[0] = (const uint8_t*)(&winpixels[0]);
    orig_stride[0] = pitch;

    return 0;
}

#endif

