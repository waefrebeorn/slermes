/*
 * port_cli_status_pure.c — Pure formatting helpers from cli.py.
 *
 * Faithful ports of the deterministic, I/O-free static helpers:
 *   - _status_bar_goal_segment(snapshot) -> str
 *   - _fmt_stash_age(stashed_at, now)    -> str
 *
 * Both are @staticmethod-style pure functions on cli.py used by the TUI
 * status bar and the prompt-stash panel. They take plain data (a dict
 * snapshot / a monotonic timestamp) and return plain strings — no globals,
 * no I/O — so they oracle cleanly against live Python.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_cli_status_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"

/* ── _status_bar_goal_segment ────────────────────────────────── */

/* PoP: _status_bar_goal_segment @ cli.py:_status_bar_goal_segment */
/* Returns the "⊙ goal 3/20" segment, or "" when no goal is active.
 * snapshot is a JSON object with optional keys goal_active (bool),
 * goal_turns_used (int), goal_max_turns (int). Caller frees. */
char *cli_status_goal_segment(const char *snapshot_json)
{
    if (!snapshot_json) return strdup("");

    char *err = NULL;
    json_t *snap = json_parse(snapshot_json, &err);
    if (err) { free(err); }
    if (!snap || snap->type != JSON_OBJECT) {
        if (snap) json_free(snap);
        return strdup("");
    }

    /* if not snapshot.get("goal_active"): return "" */
    json_t *ga = json_obj_get(snap, "goal_active");
    bool active = ga && ga->type == JSON_BOOL && ga->bool_val;
    if (!active) {
        json_free(snap);
        return strdup("");
    }

    /* used = snapshot.get("goal_turns_used") or 0
     * max  = snapshot.get("goal_max_turns")  or 0 */
    long used = 0, max_turns = 0;
    json_t *u = json_obj_get(snap, "goal_turns_used");
    if (u && u->type == JSON_NUMBER) used = (long)u->num_val;
    json_t *m = json_obj_get(snap, "goal_max_turns");
    if (m && m->type == JSON_NUMBER) max_turns = (long)m->num_val;

    char buf[64];
    if (max_turns) {
        /* f"⊙ goal {used}/{max_turns}" */
        snprintf(buf, sizeof(buf), "⊙ goal %ld/%ld", used, max_turns);
    } else {
        snprintf(buf, sizeof(buf), "⊙ goal");
    }
    json_free(snap);
    return strdup(buf);
}

/* ── _fmt_stash_age ──────────────────────────────────────────── */

/* PoP: _fmt_stash_age @ cli.py:_fmt_stash_age */
/* Human-readable age for a stash entry, given seconds-since-stash.
 * stashed_at is a time.monotonic() value and now_mono is the caller's
 * current monotonic clock; the delta is what matters. Caller frees. */
char *cli_status_fmt_stash_age(double stashed_at, double now_mono)
{
    /* secs = int(now_monotonic() - stashed_at) */
    double delta = now_mono - stashed_at;
    if (delta < 0) delta = 0;
    long secs = (long)delta;

    char buf[64];
    if (secs < 10) {
        snprintf(buf, sizeof(buf), "just now");
    } else if (secs < 90) {
        snprintf(buf, sizeof(buf), "%lds ago", secs);
    } else {
        long mins = secs / 60;
        if (mins < 60) {
            snprintf(buf, sizeof(buf), "%ld min ago", mins);
        } else {
            snprintf(buf, sizeof(buf), "%ldh ago", mins / 60);
        }
    }
    return strdup(buf);
}
