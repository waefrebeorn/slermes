/*
 * port_web_update.h — Slermes C11 port of the Hermes update-check loop.
 *
 * Implements the online update loop that powers the dashboard's "Releases"
 * section and `hermes update --check`:
 *   - hermes_cli/banner.py:check_for_updates            (6h-cached online loop)
 *   - hermes_cli/banner.py:_check_via_local_git         (git fetch + count)
 *   - hermes_cli/banner.py:_check_via_rev               (ls-remote compare)
 *   - hermes_cli/web_server.py:_recent_upstream_commits (changelog list)
 *   - hermes_cli/web_server.py:check_hermes_update      (endpoint payload)
 *   - hermes_cli/update_lock.py:UpdateLock              (update marker)
 *
 * Memory: string-returning functions return malloc'd strings (caller frees)
 * or NULL. json-returning functions return a json_t* owned by the caller.
 */

#ifndef PORT_WEB_UPDATE_H
#define PORT_WEB_UPDATE_H

#include <stdbool.h>
#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes shared by the behind-count helpers. */
#define WEB_UPDATE_BEHIND_UP_TO_DATE     0
#define WEB_UPDATE_BEHIND_NO_COUNT      -1   /* Python UPDATE_AVAILABLE_NO_COUNT */
#define WEB_UPDATE_BEHIND_FAILED        -2   /* could not determine (offline, no git, ...) */

/* ── Online loop (banner.check_for_updates) ─────────────────────────── */

/* Return how many commits this checkout is behind origin/main:
 *   >=0  exact count (0 = up to date)
 *   -1   behind but count unknown (shallow clone / rev compare)
 *   -2   check could not run (offline, no git checkout, docker, ...)
 * Caches the result in $HERMES_HOME/.update_check for 6h; force=1 busts the
 * cache so the dashboard's "Check now" button reflects reality immediately. */
int web_update_behind(int force);

/* Resolve the slermes repo root (the git checkout this binary was built
 * from): /proc/self/exe dirname walked up to a .git dir; falls back to
 * $HERMES_HOME/hermes-agent, then the CWD walk. Returns malloc'd path or
 * NULL when no checkout is found. */
char *web_update_repo_root(void);

/* ── Changelog (web_server._recent_upstream_commits) ─────────────────── */

/* Commits the local checkout is behind origin/main by, newest first.
 * Returns a JSON array of {sha, summary, author, at} objects (malloc'd
 * serialized string, caller frees). Best-effort: [] on any failure. */
char *web_update_recent_commits_json(int n);

/* ── Endpoint payload (web_server.check_hermes_update) ───────────────── */

/* Build the full /api/hermes/update/check JSON payload:
 * {install_method, current_version, behind, update_available, can_apply,
 *  update_command, message, commits}. force=1 busts the cache. Returns a
 * malloc'd string (caller frees). */
char *web_update_check_json(int force);

/* ── Update lock (update_lock.UpdateLock) ────────────────────────────── */

/* Acquire the shared update marker ($HERMES_HOME/.hermes-update-in-progress).
 * Returns true when the caller holds the lock (or the marker is ours /
 * unwritable). Returns false when another live update holds it. */
bool web_update_lock_acquire(void);

/* Drop the marker if this process still owns it. Never raises. */
void web_update_lock_release(void);

/* True when a live update marker exists (another updater is running). */
bool web_update_lock_held(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_WEB_UPDATE_H */
