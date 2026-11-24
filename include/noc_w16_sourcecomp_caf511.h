#ifndef NOC_W16_SOURCECOMP_CAF511_H
#define NOC_W16_SOURCECOMP_CAF511_H

#include "hermes_json.h"

/* C port of selected pieces of hermes_cli/source_completion.py.
 * All exports are w16_sourcecomp_caf511_-prefixed. */

/* module constant _PREPARED (caller frees) */
char *w16_sourcecomp_caf511_prepared_flag(void);
/* module constant INSTALLER_LOCK_WAIT_SECONDS (30 * 60) */
int w16_sourcecomp_caf511_lock_wait_seconds(void);

/* _acquire_waiting(lock, wait) replayed with an injected deterministic clock:
 *   clocks  - successive time.monotonic() readings (last value repeats)
 *   acquire - successive lock.acquire() results (default True when exhausted)
 * Returns {"ok": bool, "notices": [str], "sleeps": [number]}; caller owns it. */
json_t *w16_sourcecomp_caf511_acquire_waiting(double wait, json_t *clocks,
                                              json_t *acquire, int holder_pid,
                                              const char *holder_reason);

#endif
