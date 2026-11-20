/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Wii U support: ProcUI (HOME menu), the log on the SD card, and the
 * OpenGL context from gx2gl (https://github.com/ExpansionPak/gx2gl), which
 * draws with GX2 and mirrors the TV image to the GamePad.
 *
 * SDL runs its "dummy" video driver: windows exist only on paper, and the
 * SDL_GL_* functions the renderer calls are redirected here (see
 * arch_wiiu_gl.h).
 */

#include "arch_wiiu.h"
#include "arch_wiiu_gl.h"
#include "util/env.h"

/* wut uses alignas() where C only allows __attribute__((aligned)), when
 * <stdalign.h> (included by Taisei) defines it. */
#pragma push_macro("alignas")
#undef alignas
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <coreinit/time.h>
#include <gx2/display.h>
#include <gx2gl/present.h>
#include <gx2gl/proc.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#pragma pop_macro("alignas")

#undef exit
#undef atexit
#undef abort

static FILE *wiiu_log;
static OSTime wiiu_log_start;
static OSTime wiiu_log_lastflush;
static bool wiiu_log_linestart = true;
static bool wiiu_leaving_foreground;

static WiiUAtExitFn atexit_fns[8];
static int num_atexit_fns;

static int screen_width = 1920;
static int screen_height = 1080;

/* ================================================================ */

void wiiu_log_raw(const char *string) {
	WHBLogWrite(string);

	if(!wiiu_log) {
		return;
	}

	while(*string) {
		const char *nl = strchr(string, '\n');
		size_t len = nl ? (size_t)(nl - string) + 1 : strlen(string);

		if(wiiu_log_linestart) {
			fprintf(wiiu_log, "[%6u] ",
				(unsigned)OSTicksToMilliseconds(OSGetTime() - wiiu_log_start));
		}

		fwrite(string, 1, len, wiiu_log);
		wiiu_log_linestart = (nl != NULL);
		string += len;
	}
}

void wiiu_sync_log(void) {
	fflush(stdout);
	fflush(stderr);

	if(wiiu_log) {
		fflush(wiiu_log);
		fsync(fileno(wiiu_log));
	}
}

static void open_log(void) {
	/* Taisei's log goes to stdout, which goes to taisei.log (wiiu_init()
	 * sends every level there and none to stderr). Flushing every line to
	 * the SD card is slow, so it's flushed once a second (and when
	 * crashing or hanging). */
	if(freopen(WIIU_APP_DIR "/taisei.log", "w", stdout)) {
		setvbuf(stdout, NULL, _IOFBF, 64 * 1024);
		wiiu_log = stdout;
	}

	wiiu_log_start = wiiu_log_lastflush = OSGetTime();
}

void wiiu_init(void) {
	/* The HOME menu needs ProcUI running before anything else. */
	WHBProcInit();
	WHBLogCafeInit();
	WHBLogUdpInit();

	mkdir(WIIU_APP_DIR, 0755);
	chdir(WIIU_APP_DIR);
	open_log();
	wiiu_init_debug();

	switch(GX2GetSystemTVScanMode()) {
		case GX2_TV_SCAN_MODE_480I:
		case GX2_TV_SCAN_MODE_480P:
			screen_width = 854;
			screen_height = 480;
			break;
		case GX2_TV_SCAN_MODE_720P:
			screen_width = 1280;
			screen_height = 720;
			break;
		default:
			screen_width = 1920;
			screen_height = 1080;
			break;
	}

	/* No real windows: SDL's dummy video driver keeps the window code
	 * happy, the picture comes from gx2gl. */
	env_set_string("SDL_VIDEODRIVER", "dummy", true);
	env_set_string("TAISEI_RENDERER", "gl33", true);

	/* All of the log into taisei.log, with timestamps. */
	env_set_string("TAISEI_LOGLVLS_CONSOLE", "+a", true);
	env_set_string("TAISEI_LOGLVLS_STDOUT", "+a", true);
	env_set_string("TAISEI_LOGLVLS_STDERR", "-a", true);
}

void wiiu_frame(void) {
	wiiu_heartbeat();

	if(wiiu_log && OSTicksToMilliseconds(OSGetTime() - wiiu_log_lastflush) >= 1000) {
		fflush(stdout);
		fflush(stderr);
		wiiu_log_lastflush = OSGetTime();
	}

	/* WHBProcIsRunning() blocks while the HOME menu is open. */
	wiiu_pause_watchdog(1);
	bool running = WHBProcIsRunning();
	wiiu_pause_watchdog(0);

	if(!running) {
		/* HOME menu "close software", or a power down. */
		wiiu_leaving_foreground = true;
		wiiu_exit(0);
	}
}

int wiiu_atexit(WiiUAtExitFn fn) {
	if(num_atexit_fns >= (int)(sizeof(atexit_fns) / sizeof(*atexit_fns))) {
		return -1;
	}

	atexit_fns[num_atexit_fns++] = fn;
	return 0;
}

void noreturn wiiu_exit(int rc) {
	wiiu_pause_watchdog(1);

	while(num_atexit_fns > 0) {
		atexit_fns[--num_atexit_fns]();
	}

	wiiu_log_raw("Exiting\n");
	wiiu_sync_log();

	if(!wiiu_leaving_foreground) {
		/* Quitting from the game's menu or an error: ask for the Wii U
		 * menu and run ProcUI until it has released the foreground. The
		 * renderer is gone, so drop WHBGfx's callbacks, they would wait
		 * on the GPU that has been shut down. */
		ProcUIClearCallbacks();
		SYSLaunchMenu();

		while(WHBProcIsRunning()) {
		}
	}

	WHBProcShutdown();
	exit(rc);
}

void noreturn wiiu_abort(void) {
	wiiu_log_raw("abort()\n");
	wiiu_sync_log();
	/* A trap, so the crash handler reports where it came from. */
	__builtin_trap();
}

const char *wiiu_get_program_dir(void) {
	return WIIU_APP_DIR;
}

int wiiu_get_screen_width(void) {
	return screen_width;
}

int wiiu_get_screen_height(void) {
	return screen_height;
}

/* ================================================================
 * SDL_GL_* replacements (see arch_wiiu_gl.h)
 * ================================================================ */

static SDL_Window *gl_window;
static GX2GL_Context gl_context;

int wiiu_gl_load_library(const char *path) {
	(void)path;
	return 0;
}

void wiiu_gl_unload_library(void) {
}

void *wiiu_gl_get_proc_address(const char *name) {
	return GX2GL_GetProcAddress(name);
}

int wiiu_gl_set_attribute(SDL_GLattr attr, int value) {
	(void)attr;
	(void)value;
	return 0;
}

int wiiu_gl_get_attribute(SDL_GLattr attr, int *value) {
	switch(attr) {
		case SDL_GL_CONTEXT_PROFILE_MASK: *value = SDL_GL_CONTEXT_PROFILE_CORE; break;
		case SDL_GL_CONTEXT_MAJOR_VERSION: *value = 3; break;
		case SDL_GL_CONTEXT_MINOR_VERSION: *value = 3; break;
		case SDL_GL_DOUBLEBUFFER: *value = 1; break;
		case SDL_GL_RED_SIZE:
		case SDL_GL_GREEN_SIZE:
		case SDL_GL_BLUE_SIZE:
		case SDL_GL_ALPHA_SIZE: *value = 8; break;
		default: *value = 0; break;
	}

	return 0;
}

void wiiu_gl_reset_attributes(void) {
}

SDL_GLContext wiiu_gl_create_context(SDL_Window *window) {
	if(!gl_context) {
		gl_context = GX2GL_CreateContext();

		if(!gl_context) {
			SDL_SetError("GX2GL_CreateContext() failed");
			return NULL;
		}
	}

	gl_window = window;
	GX2GL_MakeCurrent(gl_context);
	return gl_context;
}

int wiiu_gl_make_current(SDL_Window *window, SDL_GLContext context) {
	gl_window = window;
	GX2GL_MakeCurrent(context);
	return 0;
}

SDL_Window *wiiu_gl_get_current_window(void) {
	return gl_window;
}

void wiiu_gl_delete_context(SDL_GLContext context) {
	if(context && context == gl_context) {
		GX2GL_DeleteContext(gl_context);
		gl_context = NULL;
	}
}

void wiiu_gl_get_drawable_size(SDL_Window *window, int *w, int *h) {
	(void)window;
	GX2ColorBuffer *tv = WHBGfxGetTVColourBuffer();
	int width = screen_width, height = screen_height;

	if(tv && tv->surface.width > 0) {
		width = (int)tv->surface.width;
		height = (int)tv->surface.height;
	}

	if(w) *w = width;
	if(h) *h = height;
}

int wiiu_gl_set_swap_interval(int interval) {
	/* No adaptive vsync on GX2. */
	GX2GL_SetSwapInterval(interval ? 1 : 0);
	return 0;
}

int wiiu_gl_get_swap_interval(void) {
	return GX2GL_GetSwapInterval();
}

void wiiu_gl_swap_window(SDL_Window *window) {
	(void)window;
	GX2GL_Present();
}
