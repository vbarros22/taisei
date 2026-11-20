/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2019, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2019, Andrei Alexeyev <akari@taisei-project.org>.
 * Wii U: the data and what the game writes live on the SD card.
 */

#include "setup.h"

#include "util/stringops.h"

static void vfs_setup_onsync(CallChainResult ccr) {
	const char *program_dir = wiiu_get_program_dir();
	// The game data is copied to data/ next to the log (see wiiu/README.md).
	char *res_path = strfmt("%s/data", program_dir);
	char *storage_path = strfmt("%s/storage", program_dir);
	char *cache_path = strfmt("%s/cache", program_dir);

	VfsSetupFixedPaths paths = {
		.res_path = res_path,
		.storage_path = storage_path,
		.cache_path = cache_path,
	};

	vfs_setup_fixedpaths(&paths);

	free(res_path);
	free(storage_path);
	free(cache_path);

	vfs_setup_onsync_done(ccr);
}

VFS_SETUP_SYNCING(vfs_setup_onsync)
