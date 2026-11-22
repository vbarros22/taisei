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

/* wut's structs use alignas() where C only allows __attribute__((aligned)),
 * when <stdalign.h> (included by Taisei) has defined alignas. */
#include <wut_structsize.h>
#undef WUT_ALIGNAS
#define WUT_ALIGNAS(x) __attribute__((aligned(x)))
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <coreinit/core.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <malloc.h>
#include <gx2/display.h>
#include <gx2gl/present.h>
#include <gx2gl/proc.h>
#include <gx2gl/stats.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

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

static OSThread *wiiu_main_thread;

/* gx2gl's guard zone reports (wiiu/gx2gl-mem-guard.patch). */
extern void (*gx2gl_debug_report)(const char *text);

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

	wiiu_main_thread = OSGetCurrentThread();
	gx2gl_debug_report = wiiu_log_raw;

	mkdir(WIIU_APP_DIR, 0755);
	chdir(WIIU_APP_DIR);
	open_log();
	wiiu_init_debug();

	{
		char text[96];
		snprintf(text, sizeof(text), "Main thread: core %u, priority %d\n",
			(unsigned)OSGetCoreId(), (int)OSGetThreadPriority(wiiu_main_thread));
		wiiu_log_raw(text);
	}

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

	/* SDL's Wii U threads get 32 KB of stack by default: Taisei's loader
	 * threads (shader #includes, image decoders) overflowed that into the
	 * heap, which crashed in unrelated places later. */
	env_set_string("SDL_THREAD_STACK_SIZE", "1048576", true);
	env_set_string("TAISEI_RENDERER", "gl33", true);

	/* All of the log into taisei.log, with timestamps. */
	env_set_string("TAISEI_LOGLVLS_CONSOLE", "+a", true);
	env_set_string("TAISEI_LOGLVLS_STDOUT", "+a", true);
	env_set_string("TAISEI_LOGLVLS_STDERR", "-a", true);
	/* Write the log right away: queued lines would be lost in a crash. */
	env_set_string("TAISEI_LOG_ASYNC", "0", true);
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

void wiiu_thread_started(void) {
	/* Cafe OS doesn't time-slice: a thread at the main thread's priority
	 * or above, on its core, can keep it from running for seconds (the
	 * texture loaders did, while transcoding). Lower numbers are higher
	 * priorities. Cores 0 and 2 are otherwise idle. */
	OSThread *thread = OSGetCurrentThread();
	int before = OSGetThreadPriority(thread);
	int prio = OSGetThreadPriority(wiiu_main_thread) + 1;

	if(prio > 31) {
		prio = 31;
	}

	if(before < prio) {
		OSSetThreadPriority(thread, prio);
	}

	OSSetThreadAffinity(thread, OS_THREAD_ATTRIB_AFFINITY_CPU0 | OS_THREAD_ATTRIB_AFFINITY_CPU2);
	OSYieldThread();

	char text[128];
	snprintf(text, sizeof(text), "Thread '%s': priority %d -> %d, now on core %u, %u KB of stack\n",
		OSGetThreadName(thread) ? OSGetThreadName(thread) : "?",
		before, (int)OSGetThreadPriority(thread), (unsigned)OSGetCoreId(),
		(unsigned)(((uint32_t)thread->stackStart - (uint32_t)thread->stackEnd) / 1024));
	wiiu_log_raw(text);
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

	/* No KHR_debug in gx2gl: the first GL errors go to the log this way. */
	static unsigned (*get_error)(void);
	static int frame, reported_errors, reported_slow;
	unsigned err;
	char line[512];

	if(!get_error) {
		get_error = (unsigned (*)(void))GX2GL_GetProcAddress("glGetError");
	}

	while(get_error && (err = get_error()) != 0 && reported_errors < 60) {
		snprintf(line, sizeof(line), "GL error 0x%04x in frame %d\n", err, frame);
		wiiu_log_raw(line);
		reported_errors++;
	}

	frame++;
	GX2GL_Present();

	/* What gx2gl did in slow frames, and in one frame every 10 s. */
	static OSTime last, last_sample;
	OSTime now = OSGetTime();
	int ms = last ? (int)OSTicksToMilliseconds(now - last) : 0;
	bool sample = OSTicksToMilliseconds(now - last_sample) >= 10000;
	last = now;

	if((ms >= 40 || sample) && reported_slow < 3000) {
		GX2GL_FrameStats st;
		struct mallinfo mi = mallinfo();

		GX2GL_GetFrameStats(&st);
		snprintf(line, sizeof(line),
			"%s frame %d: %d ms | draw %u/%ums buf %u(%uKB)/%ums unif %u/%ums "
			"bind %u/%ums tex %u/%ums(alloc %u copy %u) mip %u/%ums shd %u+%u/%ums prog %u/%ums "
			"clr %u/%ums other %u/%ums | gpu %ums vsync %ums | heap %uMB\n",
			sample ? "sample" : "SLOW", frame, ms,
			st.draws, st.draw_us / 1000, st.buffer_uploads, st.buffer_bytes / 1024,
			st.buffer_us / 1000, st.uniform_calls, st.uniform_us / 1000,
			st.texture_binds, st.texture_bind_us / 1000,
			st.texture_uploads, st.texture_upload_us / 1000,
			st.texture_storage_us / 1000, st.texture_copy_us / 1000,
			st.mipmap_generations, st.mipmap_us / 1000,
			st.shader_compiles, st.program_links, st.shader_us / 1000,
			st.program_switches, st.program_switch_us / 1000,
			st.clears, st.clear_us / 1000, st.other_calls, st.other_us / 1000,
			st.finish_us / 1000, st.vsync_us / 1000,
			(unsigned)(mi.uordblks / (1024 * 1024)));
		wiiu_log_raw(line);
		reported_slow++;

		if(sample) {
			last_sample = now;
		}
	}
}
