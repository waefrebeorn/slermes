/*
 * noc_w23_webservers_36f0c2.h - C surface for upstream
 * hermes_cli/web_server_skew_exit.py (gate w23_webservers_36f0c2).
 *
 * Code-skew retirement for a Desktop-owned backend reached over SSH: poll the
 * boot-vs-disk revision pair, require two consecutive reasons, then exit only
 * through the retirement fence (proven idle, admission closed first).
 *
 * Seams (supplied by the harness, never decided here): the skew reader, the
 * update-lock probe (including its raise), and the fence's prepare/commit
 * verdicts.  Anything uncertain keeps the process up.
 */
#ifndef NOC_W23_WEBSERVERS_36F0C2_H
#define NOC_W23_WEBSERVERS_36F0C2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DEFAULT_SKEW_POLL_S and _CONFIRMATIONS (two consecutive observations). */
double w23_webservers_36f0c2_poll_s(void);
int w23_webservers_36f0c2_confirmations(void);

/* should_retire_for_skew: bool(skew) and not update_in_progress.
 * skew is JSON null (no drift) or the ["boot_rev","disk_rev"] pair. */
int w23_webservers_36f0c2_should_retire(const json_t *skew, int update_in_progress);

/* _observe's answer for one poll: a malloc'd log line when retirement is
 * warranted, NULL otherwise.  Short-circuits on a falsy skew, so the probe is
 * not consulted then.  Caller frees. */
char *w23_webservers_36f0c2_observe(const json_t *reading);

/* The watchdog loop, driven by a scripted world:
 *   should_exit  - server.should_exit before the thread starts
 *   readings[]   - {skew|null, update, update_raises}; one per poll, and the
 *                  probe raising stands for "cannot prove the swap is over"
 *   fence        - {prepare:[bool...], commit:[bool...], tokens:[str...]},
 *                  indexed by call number and clamped to the last entry
 *   max_polls    - poll budget; null means "the script bounds it"
 * Returns {should_exit, polls, prepare_calls, commit_calls, tokens[], log[]}. */
json_t *w23_webservers_36f0c2_run(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif
