/* port_hermes_cli_heartbeat.c — faithful C11 port of hermes_cli/heartbeat.py.
 *
 * Per-session recurring re-entry prompts. Persistence is backed by the
 * shared SessionDB state.db `state_meta` table (key heartbeat:<session_id>),
 * the same seam goals.py/goals.py use — see port_gateway_run_deps.c's
 * state_meta reader for the schema (SQLite state.db under slermes_home()).
 *
 * Name parity with the Python module is preserved: every exported symbol is
 * the direct analogue of its Python counterpart. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include "port_hermes_cli_heartbeat.h"
#include "hermes_json.h"
#include "slermes_home.h"
#include "hermes_regex.h"
#include "sqlite3.h"

/* ── constants (PoP: MIN_INTERVAL_SECONDS / POLL_SECONDS) ─────────────── */
/* PoP: hb_unit_seconds @ hermes_cli/heartbeat.py:MIN_INTERVAL_SECONDS */
const int hb_MIN_INTERVAL_SECONDS = 60;
/* PoP: hb_unit_seconds @ hermes_cli/heartbeat.py:POLL_SECONDS */
const double hb_POLL_SECONDS = 5.0;

/* PoP: hb_unit_seconds @ hermes_cli/heartbeat.py:HEARTBEAT_PROMPT_TEMPLATE */
const char *hb_PROMPT_TEMPLATE =
    "[Heartbeat — recurring instruction, fires every %s]\n"
    "%s\n\n"
    "If there is nothing meaningful to do or report for this instruction "
    "right now, reply briefly that nothing has changed and stop — "
    "do not invent work.";

/* ── interval parsing ──────────────────────────────────────────────── */

/* PoP: hb_unit_seconds @ hermes_cli/heartbeat.py:_INTERVAL_RE */
/* Faithful port of the compiled regex: captures a float + word unit. */
typedef struct { const char *word; int seconds; } hb_unit_t;
static const hb_unit_t HB_UNITS[] = {
    {"s",1},{"sec",1},{"secs",1},{"second",1},{"seconds",1},
    {"m",60},{"min",60},{"mins",60},{"minute",60},{"minutes",60},
    {"h",3600},{"hr",3600},{"hrs",3600},{"hour",3600},{"hours",3600},
    {"d",86400},{"day",86400},{"days",86400},
};
/* PoP: hb_unit_seconds @ hermes_cli/heartbeat.py:_UNIT_SECONDS */
static int hb_unit_seconds(const char *word) {
    size_t bestlen = 0; const char *best = NULL;
    for (size_t i = 0; i < sizeof(HB_UNITS)/sizeof(HB_UNITS[0]); i++) {
        if (strcasecmp(HB_UNITS[i].word, word) == 0) {
            size_t l = strlen(HB_UNITS[i].word);
            if (l > bestlen) { bestlen = l; best = word; } /* longest match wins */
        }
    }
    if (!best) return 0;
    for (size_t i = 0; i < sizeof(HB_UNITS)/sizeof(HB_UNITS[0]); i++) {
        if (strcasecmp(HB_UNITS[i].word, best) == 0) return HB_UNITS[i].seconds;
    }
    return 0;
}

/* PoP: None @ hermes_cli/heartbeat.py:parse_interval */
/* Faithful port: returns None (0, *out=NULL) for non-interval,
 * -1 (0, *out=INT_MIN) for below the floor, else the seconds.
 * Uses libregex (POSIX ERE) with the exact same anchored semantics as
 * Python's _INTERVAL_RE (re.IGNORECASE), so mixed-case "Every 10m" and
 * inputs with stray trailing chars / signs / exponents correctly fail
 * to match (Python returns None). */
int hb_parse_interval(const char *text, int *out) {
    if (out) *out = 0;
    if (!text || !*text) return 0;  /* None */
    /* Python pattern (re.IGNORECASE):
     *   ^\s*(?:every\s+)?(\d+(?:\.\d+)?)\s*(s|sec|secs|second|seconds|m|min|mins|minute|minutes|h|hr|hrs|hour|hours|d|day|days)\s*$
     * POSIX ERE equivalent: \s -> [[:space:]], \d -> [0-9], \.\d+ -> (\.[0-9]+)?,
     * (?:every\s+) -> (every[[:space:]]+) (non-capturing becomes capturing; harmless).
     * The unit alternation is ordered longest-first so POSIX leftmost-longest
     * matching agrees with Python's "seconds?" etc. */
    hregex_t *re = regex_compile(
        "^[[:space:]]*(every[[:space:]]+)?([0-9]+(\\.[0-9]+)?)[[:space:]]*"
        "(seconds?|second|secs?|sec|s|minutes?|minute|mins?|min|m|"
        "hours?|hour|hrs?|hr|h|days?|day|d)"
        "[[:space:]]*$", 1);  /* flags & 1 == REG_ICASE */
    if (!re) return 0;
    regex_match_t *m = regex_match(re, text);
    bool ok = false;
    if (m && m->matched && m->groups[2]) {
        double value = strtod(m->groups[2], NULL);
        int secs = hb_unit_seconds(m->groups[4]);
        if (secs != 0) {
            int seconds = (int)(value * secs);
            if (seconds < hb_MIN_INTERVAL_SECONDS) {
                if (out) *out = INT_MIN;  /* -1: too small */
                ok = true;
            } else {
                if (out) *out = seconds;
                ok = true;
            }
        }
    }
    regex_match_free(m);
    regex_free(re);
    return ok ? 1 : 0;
}

/* PoP: hb_format_interval @ hermes_cli/heartbeat.py:format_interval */
/* Faithful port: 600 -> "10m" etc. */
char *hb_format_interval(int seconds) {
    seconds = (int)seconds;
    char *out = NULL;
    if (seconds % 86400 == 0) asprintf(&out, "%dd", seconds / 86400);
    else if (seconds % 3600 == 0) asprintf(&out, "%dh", seconds / 3600);
    else if (seconds % 60 == 0) asprintf(&out, "%dm", seconds / 60);
    else asprintf(&out, "%ds", seconds);
    return out;
}

/* ── SessionDB state_meta persistence ──────────────────────────────── */

static char *hb_state_meta_get(const char *key) {
    if (!key) return NULL;
    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/state.db", slermes_home());
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    char *value = NULL;
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT value FROM state_meta WHERE key = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(st, 0);
            if (v) value = strdup(v);
        }
    }
    if (st) sqlite3_finalize(st);
    sqlite3_close(db);
    return value;
}

static bool hb_state_meta_set(const char *key, const char *value) {
    if (!key || !value) return false;
    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/state.db", slermes_home());
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    /* Mirror Python SessionDB.set_meta: upsert. */
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS state_meta (key TEXT PRIMARY KEY, value TEXT);",
        NULL, NULL, NULL);
    const char *sql = "INSERT INTO state_meta (key, value) VALUES (?, ?) "
                      "ON CONFLICT(key) DO UPDATE SET value=excluded.value";
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, value, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE);
    }
    if (st) sqlite3_finalize(st);
    sqlite3_close(db);
    return ok;
}

/* ── HeartbeatState ────────────────────────────────────────────────── */

hb_state_t *hb_state_new(const char *prompt, int interval_seconds,
                         const char *status, double created_at,
                         double last_fired_at, int fire_count) {
    hb_state_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->prompt = strdup(prompt ? prompt : "");
    s->interval_seconds = interval_seconds;
    s->status = strdup(status ? status : "active");
    s->created_at = created_at;
    s->last_fired_at = last_fired_at;
    s->fire_count = fire_count;
    if (!s->prompt || !s->status) { hb_state_free(s); return NULL; }
    return s;
}

void hb_state_free(hb_state_t *s) {
    if (!s) return;
    free(s->prompt);
    free(s->status);
    free(s);
}

/* PoP: hb_state_to_json @ hermes_cli/heartbeat.py:to_json */
char *hb_state_to_json(const hb_state_t *s) {
    if (!s) return NULL;
    char *out = NULL;
    asprintf(&out,
        "{\"prompt\":%s,\"interval_seconds\":%d,\"status\":%s,"
        "\"created_at\":%.6f,\"last_fired_at\":%.6f,\"fire_count\":%d}",
        json_escape_string(s->prompt), s->interval_seconds, json_escape_string(s->status),
        s->created_at, s->last_fired_at, s->fire_count);
    return out;
}

/* PoP: hb_state_from_json @ hermes_cli/heartbeat.py:from_json */
hb_state_t *hb_state_from_json(const char *raw) {
    if (!raw || !*raw) return NULL;
    json_t *j = json_parse(raw, NULL);
    if (!j) return NULL;
    hb_state_t *s = hb_state_new(
        json_get_str(j, "prompt", ""),
        (int)json_get_num(j, "interval_seconds", 0.0),
        json_get_str(j, "status", "active"),
        json_get_num(j, "created_at", 0.0),
        json_get_num(j, "last_fired_at", 0.0),
        (int)json_get_num(j, "fire_count", 0.0));
    json_free(j);
    return s;
}

/* PoP: hb_state_is_due @ hermes_cli/heartbeat.py:is_due */
bool hb_state_is_due(const hb_state_t *s, double now) {
    if (!s) return false;
    if (strcmp(s->status, "active") != 0) return false;
    if (!s->prompt || !*s->prompt) return false;
    if (s->interval_seconds <= 0) return false;
    double anchor = s->last_fired_at > 0 ? s->last_fired_at : s->created_at;
    return (now - anchor) >= s->interval_seconds;
}

/* PoP: hb_state_render_prompt @ hermes_cli/heartbeat.py:render_prompt */
char *hb_state_render_prompt(const hb_state_t *s) {
    if (!s || !s->prompt) return NULL;
    char *iv = hb_format_interval(s->interval_seconds);
    char *out = NULL;
    asprintf(&out, hb_PROMPT_TEMPLATE, iv ? iv : "?", s->prompt);
    free(iv);
    return out;
}

/* ── meta key + persistence ────────────────────────────────────────── */

/* PoP: hb_meta_key @ hermes_cli/heartbeat.py:_meta_key */
char *hb_meta_key(const char *session_id) {
    if (!session_id || !*session_id) return NULL;
    char *out = NULL;
    asprintf(&out, "heartbeat:%s", session_id);
    return out;
}

/* PoP: hb_get_session_db_path @ hermes_cli/heartbeat.py:_get_session_db */
/* In C the SessionDB is the on-disk state.db; return its path. */
char *hb_get_session_db_path(void) {
    const char *home = slermes_home();
    if (!home || !*home) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/state.db", home);
    return out;
}

/* PoP: hb_load_heartbeat @ hermes_cli/heartbeat.py:load_heartbeat */
hb_state_t *hb_load_heartbeat(const char *session_id) {
    if (!session_id || !*session_id) return NULL;
    char *key = hb_meta_key(session_id);
    if (!key) return NULL;
    char *raw = hb_state_meta_get(key);
    free(key);
    if (!raw) return NULL;
    hb_state_t *state = hb_state_from_json(raw);
    free(raw);
    if (!state) return NULL;
    if (strcmp(state->status, "cleared") == 0) {
        hb_state_free(state);
        return NULL;
    }
    return state;
}

/* PoP: hb_save_heartbeat @ hermes_cli/heartbeat.py:save_heartbeat */
bool hb_save_heartbeat(const char *session_id, const hb_state_t *state) {
    if (!session_id || !*session_id || !state) return false;
    char *key = hb_meta_key(session_id);
    if (!key) return false;
    char *json = hb_state_to_json(state);
    bool ok = false;
    if (json) {
        ok = hb_state_meta_set(key, json);
        free(json);
    }
    free(key);
    return ok;
}

/* ── HeartbeatManager ──────────────────────────────────────────────── */

/* Forward declarations (used before definition below). */
void hb_manager_free(hb_manager_t *m);

/* PoP: hb_manager_new @ hermes_cli/heartbeat.py:__init__ */
hb_manager_t *hb_manager_new(const char *session_id) {
    if (!session_id || !*session_id) return NULL;
    hb_manager_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->session_id = strdup(session_id);
    m->state = hb_load_heartbeat(session_id);
    if (!m->session_id) { hb_manager_free(m); return NULL; }
    return m;
}

void hb_manager_free(hb_manager_t *m) {
    if (!m) return;
    free(m->session_id);
    hb_state_free(m->state);
    free(m);
}

/* PoP: hb_manager_state @ hermes_cli/heartbeat.py:state */
hb_state_t *hb_manager_state(hb_manager_t *m) {
    return m ? m->state : NULL;
}

/* PoP: hb_manager_has_heartbeat @ hermes_cli/heartbeat.py:has_heartbeat */
bool hb_manager_has_heartbeat(hb_manager_t *m) {
    return m && m->state &&
           (strcmp(m->state->status, "active") == 0 ||
            strcmp(m->state->status, "paused") == 0);
}

/* PoP: hb_manager_is_active @ hermes_cli/heartbeat.py:is_active */
bool hb_manager_is_active(hb_manager_t *m) {
    return m && m->state && strcmp(m->state->status, "active") == 0;
}

/* PoP: hb_manager_status_line @ hermes_cli/heartbeat.py:status_line */
char *hb_manager_status_line(hb_manager_t *m) {
    hb_state_t *s = m ? m->state : NULL;
    if (!s) return strdup("No heartbeat. Set one with /heartbeat every <interval> <prompt>.");
    char *every = hb_format_interval(s->interval_seconds);
    char *out = NULL;
    if (strcmp(s->status, "active") == 0) {
        double anchor = s->last_fired_at > 0 ? s->last_fired_at : s->created_at;
        int next_in = (int)(anchor + s->interval_seconds - time(NULL));
        if (next_in < 0) next_in = 0;
        char fired[32] = "";
        if (s->fire_count) snprintf(fired, sizeof(fired), ", fired %d×", s->fire_count);
        asprintf(&out, "♥ Heartbeat (every %s, next in ~%ds%s): %s",
                 every ? every : "?", next_in, fired, s->prompt);
    } else if (strcmp(s->status, "paused") == 0) {
        char fired[32] = "";
        if (s->fire_count) snprintf(fired, sizeof(fired), ", fired %d×", s->fire_count);
        asprintf(&out, "⏸ Heartbeat (paused, every %s%s): %s",
                 every ? every : "?", fired, s->prompt);
    } else {
        asprintf(&out, "Heartbeat (%s, every %s): %s",
                 s->status, every ? every : "?", s->prompt);
    }
    free(every);
    return out;
}

/* PoP: hb_manager_set @ hermes_cli/heartbeat.py:set */
hb_state_t *hb_manager_set(hb_manager_t *m, const char *prompt,
                           int interval_seconds) {
    if (!m) return NULL;
    if (!prompt) return NULL;
    /* Python: prompt = (prompt or "").strip() -> raise ValueError on empty */
    char *p = strdup(prompt);
    if (!p) return NULL;
    char *start = p;
    while (*start && isspace((unsigned char)*start)) start++;
    size_t len = strlen(start);
    while (len && isspace((unsigned char)start[len-1])) start[--len] = '\0';
    memmove(p, start, len + 1);
    if (!*p) { free(p); return NULL; }  /* empty -> caller raises ValueError */
    if (interval_seconds < hb_MIN_INTERVAL_SECONDS) {
        free(p); return NULL;  /* < floor -> caller raises ValueError */
    }
    /* Replace old state (Python mutates self._state then saves). */
    hb_state_t *old = m->state;
    double now = (double)time(NULL);
    hb_state_t *state = hb_state_new(p, interval_seconds, "active", now, 0.0, 0);
    free(p);
    if (!state) return NULL;
    m->state = state;
    hb_save_heartbeat(m->session_id, state);
    hb_state_free(old);
    return state;
}

/* PoP: hb_manager_pause @ hermes_cli/heartbeat.py:pause */
hb_state_t *hb_manager_pause(hb_manager_t *m) {
    if (!m || !m->state) return NULL;
    free(m->state->status);
    m->state->status = strdup("paused");
    hb_save_heartbeat(m->session_id, m->state);
    return m->state;
}

/* PoP: hb_manager_resume @ hermes_cli/heartbeat.py:resume */
hb_state_t *hb_manager_resume(hb_manager_t *m) {
    if (!m || !m->state) return NULL;
    free(m->state->status);
    m->state->status = strdup("active");
    /* Re-anchor so resuming doesn't instantly fire a stale tick. */
    m->state->last_fired_at = (double)time(NULL);
    hb_save_heartbeat(m->session_id, m->state);
    return m->state;
}

/* PoP: hb_manager_clear @ hermes_cli/heartbeat.py:clear */
bool hb_manager_clear(hb_manager_t *m) {
    if (!m || !m->state) return false;
    free(m->state->status);
    m->state->status = strdup("cleared");
    hb_save_heartbeat(m->session_id, m->state);
    hb_state_free(m->state);
    m->state = NULL;
    return true;
}

/* PoP: hb_manager_due_prompt @ hermes_cli/heartbeat.py:due_prompt */
char *hb_manager_due_prompt(hb_manager_t *m, double now) {
    if (!m || !m->state) return NULL;
    if (!hb_state_is_due(m->state, now)) return NULL;
    /* Record the fire immediately (anchor resets to NOW, missed ticks coalesce). */
    m->state->last_fired_at = now;
    m->state->fire_count += 1;
    hb_save_heartbeat(m->session_id, m->state);
    return hb_state_render_prompt(m->state);
}

/* PoP: hb_migrate_heartbeat_to_session @ hermes_cli/heartbeat.py:migrate_heartbeat_to_session */
bool hb_migrate_heartbeat_to_session(const char *old_session_id,
                                     const char *new_session_id) {
    if (!old_session_id || !new_session_id ||
        strcmp(old_session_id, new_session_id) == 0) return false;
    hb_state_t *state = hb_load_heartbeat(old_session_id);
    if (!state) return false;
    if (hb_load_heartbeat(new_session_id) != NULL) {
        hb_state_free(state);
        return false;
    }
    hb_save_heartbeat(new_session_id, state);
    free(state->status);
    state->status = strdup("cleared");
    hb_save_heartbeat(old_session_id, state);
    hb_state_free(state);
    return true;
}
