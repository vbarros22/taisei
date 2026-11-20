/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * On the Wii U the OpenGL context comes from gx2gl, not SDL: the SDL_GL_*
 * functions the renderer uses are redirected to arch_wiiu.c.
 */

#pragma once
#include "taisei.h"

#include <SDL.h>

int wiiu_gl_load_library(const char *path);
void wiiu_gl_unload_library(void);
void *wiiu_gl_get_proc_address(const char *name);
int wiiu_gl_set_attribute(SDL_GLattr attr, int value);
int wiiu_gl_get_attribute(SDL_GLattr attr, int *value);
void wiiu_gl_reset_attributes(void);
SDL_GLContext wiiu_gl_create_context(SDL_Window *window);
int wiiu_gl_make_current(SDL_Window *window, SDL_GLContext context);
SDL_Window *wiiu_gl_get_current_window(void);
void wiiu_gl_delete_context(SDL_GLContext context);
void wiiu_gl_get_drawable_size(SDL_Window *window, int *w, int *h);
int wiiu_gl_set_swap_interval(int interval);
int wiiu_gl_get_swap_interval(void);
void wiiu_gl_swap_window(SDL_Window *window);

#ifndef WIIU_GL_NO_REDIRECT
#define SDL_GL_LoadLibrary         wiiu_gl_load_library
#define SDL_GL_UnloadLibrary       wiiu_gl_unload_library
#define SDL_GL_GetProcAddress      wiiu_gl_get_proc_address
#define SDL_GL_SetAttribute        wiiu_gl_set_attribute
#define SDL_GL_GetAttribute        wiiu_gl_get_attribute
#define SDL_GL_ResetAttributes     wiiu_gl_reset_attributes
#define SDL_GL_CreateContext       wiiu_gl_create_context
#define SDL_GL_MakeCurrent         wiiu_gl_make_current
#define SDL_GL_GetCurrentWindow    wiiu_gl_get_current_window
#define SDL_GL_DeleteContext       wiiu_gl_delete_context
#define SDL_GL_GetDrawableSize     wiiu_gl_get_drawable_size
#define SDL_GL_SetSwapInterval     wiiu_gl_set_swap_interval
#define SDL_GL_GetSwapInterval     wiiu_gl_get_swap_interval
#define SDL_GL_SwapWindow          wiiu_gl_swap_window
#endif
