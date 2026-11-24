/*
 * noc_w25_idlewatch_8d8b25.h - C surface for upstream
 * tools/bot_desktop/idle_watch.py (gate w25_idlewatch_8d8b25).
 *
 * The pure decision core of the launcher-driven idle check: how long the
 * launcher sleeps before the next check, and which launcher identity the check
 * may stop.  The seams -- runtime.stop_if_idle(), runtime.idle_stop_seconds(),
 * runtime.idle_seconds(), runtime._create_time(pid), os.setsid(), the config
 * read and the logging handler -- stay in the caller; this file owns only what
 * the values decide.
 */
#ifndef NOC_W25_IDLEWATCH_8D8B25_H
#define NOC_W25_IDLEWATCH_8D8B25_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the gateway watcher's cadence; a window shorter than that is checked at its own length */
#define W25_IDLEWATCH_8D8B25_MIN_DELAY_S 30.0
/* runtime.DEFAULT_IDLE_STOP_MINUTES */
#define W25_IDLEWATCH_8D8B25_DEFAULT_IDLE_STOP_MINUTES 30

/* {"limit": <number>, "idle": <number|null>, "default_minutes": <number|null>} ->
 * whole seconds (integer) until the screen could next be due. */
json_t *w25_idlewatch_8d8b25_next_check_in(const json_t *req);

/* {"argv": [...], "born": <number|null>} -> [pid, create_time] or null.
 * "argv" is sys.argv[1:]; "born" is runtime._create_time(int(argv[0])). */
json_t *w25_idlewatch_8d8b25_own_launcher(const json_t *req);

/* The cadence constants, as an object. */
json_t *w25_idlewatch_8d8b25_constants(void);

#ifdef __cplusplus
}
#endif
#endif
