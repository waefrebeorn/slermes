/*
 * noc_w25_asyncdeleg_9f3438.h - C surface for upstream
 * tools/async_delegation_recovery_hints.py (gate w25_asyncdeleg_9f3438).
 *
 * When the owner process dies before a background child records a terminal
 * result, the recovery event carries the last lines of each live transcript and
 * a one-line git snapshot of the owner's cwd, so the parent can decide whether
 * to continue or re-dispatch without opening files.  Reading the transcript,
 * running git and reading the clock are seams; this header owns the bounded
 * formatting those values decide.
 */
#ifndef NOC_W25_ASYNCDELEG_9F3438_H
#define NOC_W25_ASYNCDELEG_9F3438_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_ASYNCDELEG_9F3438_TAIL_LINES 20
#define W25_ASYNCDELEG_9F3438_TAIL_CHARS 2000
#define W25_ASYNCDELEG_9F3438_GIT_TIMEOUT_S 5

/* transcript_tail over an already-read window: the last `lines` lines (at most
 * `chars` characters, "…"-prefixed when cut) of `raw`, or null when the tail is
 * empty.  A null/non-string `raw` is the unreadable-file case -> null. */
json_t *w25_asyncdeleg_9f3438_tail_text(const json_t *raw, long long lines, long long chars);

/* transcript_tails over fixed contents: {index: raw-window-or-null} ->
 * {index: tail} keeping only the indexes whose tail is non-empty. */
json_t *w25_asyncdeleg_9f3438_tails(const json_t *contents);

/* git_state_hint over fixed git replies:
 *   head   -- stdout of ``git log -1 --format=%h %s`` (null when git failed)
 *   branch -- stdout of ``git rev-parse --abbrev-ref HEAD`` (null when it failed)
 *   status -- stdout of ``git status --porcelain`` (null when it failed)
 * -> "<branch> @ <head> (<dirty> uncommitted file(s)) in <cwd>", or null when
 * there is no git checkout (``head`` null, or ``cwd`` not a directory). */
json_t *w25_asyncdeleg_9f3438_git_hint(const json_t *head, const json_t *branch,
                                       const json_t *status, const json_t *cwd,
                                       const json_t *cwd_is_dir);

/* The best-effort bounds, as an object. */
json_t *w25_asyncdeleg_9f3438_constants(void);

#ifdef __cplusplus
}
#endif
#endif
