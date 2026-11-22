/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Wii U support: ProcUI (HOME menu), the log on the SD card, a crash
 * handler, and the OpenGL context from gx2gl.
 */

#pragma once
#include "taisei.h"

/* Everything lives on the SD card: the game data in data/, what the game
 * writes in storage/ and cache/, and the log. */
#define WIIU_APP_DIR "fs:/vol/external01/wiiu/apps/taisei"

typedef void (*WiiUAtExitFn)(void);

/* First thing in main(). */
void wiiu_init(void);

/* Every frame: keeps ProcUI (the HOME menu) going. */
void wiiu_frame(void);

int wiiu_atexit(WiiUAtExitFn fn);
void noreturn wiiu_exit(int rc);
void noreturn wiiu_abort(void);

const char *wiiu_get_program_dir(void);
int wiiu_get_screen_width(void);
int wiiu_get_screen_height(void);

/* Called first thing in every thread Taisei creates: moves it off the main
 * thread's core, below the main thread's priority. */
void wiiu_thread_started(void);

/* Where the main loop's time goes, reported with the frame statistics. */
enum { WIIU_PHASE_LOGIC, WIIU_PHASE_RENDER, WIIU_PHASE_WAIT, WIIU_PHASE_COUNT };
uint64_t wiiu_now_us(void);
void wiiu_phase_add(int phase, uint64_t start_us);
void wiiu_render_started(void);

/* The log on the SD card (taisei.log), with milliseconds since startup. */
void wiiu_log_raw(const char *string);
void wiiu_sync_log(void);

/* Crash handler and hang watchdog (arch_wiiu_debug.c). */
void wiiu_init_debug(void);
void wiiu_heartbeat(void);
void wiiu_pause_watchdog(int paused);
