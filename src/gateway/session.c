#include "hermes_logger.h"
#include "hermes_gateway_core.h"
#include "hermes_core_types.h"
#include <ctype.h>
#include <string.h>

/*
 * session.c — Port of Python gateway/session.py
 *
 * Name-parity wrapper for gateway session management functions.
 * The heavy session lifecycle lives in server.c (gw_session_create,
 * gw_session_lookup, gw_session_destroy, gw_session_list).
 * This file implements the pure utility functions from session.py
 * that are called by the C code directly.
 *
 * PoP annotations referencing this module: 214
 */

/* Port of Python gateway/session.py:_is_path_unsafe
 *
 * Return true if `value` could traverse outside the sessions dir.
 * Rejects: parent traversal (".."), path separator anywhere ("/" or "\\"),
 * and a leading Windows drive letter ("C:").
 * Legitimate session keys are colon-delimited multi-segment ids
 * ("agent:main:<platform>:...") and never contain these.
 */
int* cli_gateway_session__is_path_unsafe(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 0; /* false */
        return result;
    }
    const char *s = (const char*)p1;
    /* Check for ".." parent traversal */
    if (strstr(s, "..") != NULL) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 1; /* true */
        return result;
    }
    /* Check for path separators */
    if (strchr(s, '/') != NULL || strchr(s, '\\') != NULL) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 1; /* true */
        return result;
    }
    /* Check for leading Windows drive letter pattern "X:" */
    size_t len = strlen(s);
    if (len >= 2 && isalpha((unsigned char)s[0]) && s[1] == ':') {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 1; /* true */
        return result;
    }
    int *result = (int*)malloc(sizeof(int));
    if (result) *result = 0; /* false */
    return result;
}

/* Port of Python gateway/session.py:_session_key_namespace
 *
 * Extract the namespace prefix from a session key.
 * Session keys have the form "agent:main:<platform>:<chat_id>" or
 * "agent:cron:<job_id>". Returns the second segment ("main", "cron", etc.)
 * or NULL on input that doesn't match the expected pattern.
 */
void* cli_gateway_session__session_key_namespace(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) return NULL;

    const char *key = (const char*)p1;
    /* Skip first segment (e.g. "agent:") */
    const char *first_colon = strchr(key, ':');
    if (!first_colon) return NULL;

    const char *ns_start = first_colon + 1;
    if (*ns_start == '\0') return NULL;

    /* Find end of namespace segment */
    const char *second_colon = strchr(ns_start, ':');
    size_t ns_len;
    if (second_colon) {
        ns_len = (size_t)(second_colon - ns_start);
    } else {
        ns_len = strlen(ns_start);
    }

    if (ns_len == 0) return NULL;

    char *result = (char*)malloc(ns_len + 1);
    if (!result) return NULL;
    memcpy(result, ns_start, ns_len);
    result[ns_len] = '\0';
    return result;
}

/* Port of Python gateway/session.py:_resolve_profile_for_key
 *
 * Extract the profile name from a session key.
 * Session keys have the form "agent:main:<platform>:<chat_id>".
 * The profile is the third segment (after agent: and namespace:).
 * Returns malloc'd string or NULL.
 */
void* cli_gateway_session__resolve_profile_for_key(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) return NULL;

    const char *key = (const char*)p1;
    /* Skip first segment "agent:" */
    const char *first_colon = strchr(key, ':');
    if (!first_colon) return NULL;

    /* Skip namespace segment */
    const char *ns_end = strchr(first_colon + 1, ':');
    if (!ns_end) return NULL;

    const char *profile_start = ns_end + 1;
    if (*profile_start == '\0') return NULL;

    /* Profile extends to next colon or end of string */
    const char *third_colon = strchr(profile_start, ':');
    size_t profile_len;
    if (third_colon) {
        profile_len = (size_t)(third_colon - profile_start);
    } else {
        profile_len = strlen(profile_start);
    }

    if (profile_len == 0) return NULL;

    char *result = (char*)malloc(profile_len + 1);
    if (!result) return NULL;
    memcpy(result, profile_start, profile_len);
    result[profile_len] = '\0';
    return result;
}

/* === merged from session_gaps.c === */
/*
 * port_session_gaps.c — Faithful C11 port of gateway/session.py REAL_GAP functions.
 * Session management: durable active-turn markers, crash recovery,
 * clean-shutdown marker discard, spooled transcript-drop draining.
 *
 * Python twin: gateway/session.py (AsyncSessionStore methods). The C session
 * store is the hive-backed gw_session_entry_t table in g_gw.sessions; the
 * marker fields (active_turn_token / active_turn_started_at / resume_reason /
 * last_resume_marked_at) live at the tail of gw_session_entry_t.
 */

#include "gw_server_internals.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glob.h>
#define _GNU_SOURCE 1

/* Declared in gw_session.c (same store). */
extern void _save_entry(const char *session_key);
extern void _save_entries(void);

/* ── timestamp helpers (Python: datetime.now().isoformat()) ─────────────── */

static void _iso_now(char *buf, size_t sz) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sz, "%Y-%m-%dT%H:%M:%S", &tmv);
}

/* Parse "YYYY-MM-DDTHH:MM:SS" (Python isoformat, naive local) to epoch secs.
 * Returns -1 when unparseable (mirrors the Python TypeError → stale path). */
static double _parse_iso(const char *s) {
    if (!s || strlen(s) < 19 || s[4] != '-' || s[7] != '-' || s[10] != 'T')
        return -1.0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    if (sscanf(s, "%d-%d-%dT%d:%d:%d",
               &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday,
               &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec) != 6)
        return -1.0;
    tmv.tm_year -= 1900;
    tmv.tm_mon -= 1;
    tmv.tm_isdst = -1;
    time_t t = mktime(&tmv);
    return (t == (time_t)-1) ? -1.0 : (double)t;
}

/* uuid4().hex — 32 lowercase hex chars from the kernel CSPRNG. */
static void _gen_turn_token(char *out, size_t sz) {
    static const char hex[] = "0123456789abcdef";
    unsigned char rnd[16];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(rnd, 1, sizeof(rnd), f) != sizeof(rnd)) {
        /* Degraded fallback: time + pointer mix (still unique per call). */
        unsigned long v = (unsigned long)time(NULL) * 2654435761u
                        ^ (unsigned long)(uintptr_t)out;
        for (int i = 0; i < 16; i++) rnd[i] = (unsigned char)(v >> ((i % 8) * 4));
    }
    if (f) fclose(f);
    /* uuid4: version nibble '4', variant top bits 10 */
    rnd[6] = (unsigned char)((rnd[6] & 0x0f) | 0x40);
    rnd[8] = (unsigned char)((rnd[8] & 0x3f) | 0x80);
    size_t o = 0;
    for (int i = 0; i < 16 && o + 1 < sz; i++) {
        out[o++] = hex[rnd[i] >> 4];
        out[o++] = hex[rnd[i] & 0x0f];
    }
    out[o] = '\0';
}

/* PoP: session_mark_turn_active @ gateway/session.py:mark_turn_active */
/* Persist exact ownership of the agent turn running for session_key.
 * Generates an opaque token, stamps it (plus start time) on the entry and
 * persists BEFORE publishing in memory. Re-marking replaces the previous
 * token so a stale asynchronous unwind cannot clear a newer turn.
 * Writes the token to out_token (32 hex chars). Returns false when the
 * entry does not exist (Python returns None). */
bool session_mark_turn_active(const char *session_key, char *out_token, size_t out_sz) {
    if (!session_key) return false;
    int idx = session_find_by_key(session_key);
    if (idx < 0) return false;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) return false;

    char token[40];
    _gen_turn_token(token, sizeof(token));
    char now_iso[32];
    _iso_now(now_iso, sizeof(now_iso));

    /* Durable write first (Python persists before publishing the marker). */
    snprintf(se->active_turn_token, sizeof(se->active_turn_token), "%s", token);
    snprintf(se->active_turn_started_at, sizeof(se->active_turn_started_at), "%s", now_iso);
    se->last_active = (double)time(NULL); /* keeps legacy startup heuristic effective */
    _save_entry(session_key);

    if (out_token && out_sz > 0)
        snprintf(out_token, out_sz, "%s", token);
    return true;
}

/* PoP: session_clear_turn_active @ gateway/session.py:clear_turn_active */
/* Compare-and-swap clear of an active-turn marker. Returns false when the
 * entry disappeared or a newer turn owns the marker. */
bool session_clear_turn_active(const char *session_key, const char *token) {
    if (!session_key || !token || !token[0]) return false;
    int idx = session_find_by_key(session_key);
    if (idx < 0) return false;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) return false;
    if (se->active_turn_token[0] == '\0' ||
        strcmp(se->active_turn_token, token) != 0)
        return false;

    /* Keep the live token until the clear is durable (Python comment:
     * a failed write remains retryable instead of a false mismatch). */
    se->active_turn_token[0] = '\0';
    se->active_turn_started_at[0] = '\0';
    _save_entry(session_key);
    return true;
}

/* PoP: session_recover_interrupted_turns @ gateway/session.py:recover_interrupted_turns */
/* Promote exact crash-left turn markers into resume_pending. Old or invalid
 * markers are cleared without resuming; explicitly suspended sessions are
 * never re-armed. Returns the number of newly promoted sessions. */
int session_recover_interrupted_turns(int max_age_seconds) {
    double now = (double)time(NULL);
    if (max_age_seconds < 0) max_age_seconds = 0;
    int promoted = 0;
    bool changed = false;

    if (!g_gw.sessions) return 0;
    pthread_mutex_lock(&g_gw.session_mutex);
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
        if (!se->in_use || se->active_turn_token[0] == '\0')
            continue;

        double started = _parse_iso(se->active_turn_started_at);
        bool marker_is_stale =
            (started < 0.0) ||
            (max_age_seconds > 0 && now - started > (double)max_age_seconds);

        if (!marker_is_stale && !se->suspended) {
            if (se->resume_pending) {
                /* A drain-timeout marker is more specific than the generic
                 * crash reason — preserve it and its freshness. */
                if (se->last_resume_marked_at == 0.0)
                    se->last_resume_marked_at = now;
            } else {
                se->resume_pending = true;
                snprintf(se->resume_reason, sizeof(se->resume_reason),
                         "restart_interrupted");
                /* Freshness starts when recovery is discovered, not when a
                 * potentially hours-long turn began. */
                se->last_resume_marked_at = now;
                promoted++;
            }
        }

        se->active_turn_token[0] = '\0';
        se->active_turn_started_at[0] = '\0';
        changed = true;
    }
    pthread_mutex_unlock(&g_gw.session_mutex);

    if (changed) {
        /* Cold-start batch: one durable rewrite beats per-entry upserts. */
        _save_entries();
    }
    return promoted;
}

/* PoP: session_discard_active_turn_markers @ gateway/session.py:discard_active_turn_markers */
/* Clear orphan turn markers after a verified clean shutdown. */
int session_discard_active_turn_markers(void) {
    int cleared = 0;
    if (!g_gw.sessions) return 0;
    pthread_mutex_lock(&g_gw.session_mutex);
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
        if (!se->in_use) continue;
        if (se->active_turn_token[0] == '\0' && se->active_turn_started_at[0] == '\0')
            continue;
        se->active_turn_token[0] = '\0';
        se->active_turn_started_at[0] = '\0';
        cleared++;
    }
    pthread_mutex_unlock(&g_gw.session_mutex);
    if (cleared)
        _save_entries();
    return cleared;
}

/* ── Spooled transcript-cap drops (gateway/shutdown_flush.py twin) ──────── */

#define SPOOL_DROP_REASON "transcript_cap_drop"

static void _spool_dir(char *out, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    snprintf(out, sz, "%s/pending_messages", home);
}

/* Replay one spooled message into the session's durable DB transcript
 * record (the C analog of self._append_transcript_message). */
static bool _replay_transcript_message(const char *session_id, const char *message_json) {
    int idx = lookup_by_session_id(session_id);
    if (idx < 0) return false;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->db || !se->session_id[0]) return false;
    char key[160];
    snprintf(key, sizeof(key), "%s:transcript_spool", se->session_id);
    db_save(se->db, key, message_json);
    return true;
}

/* PoP: session_drain_spooled_drops @ gateway/session.py:_drain_spooled_drops */
/* Replay cap-dropped spooled transcript messages after DB recovery.
 * Best-effort: replay failures keep the spool files for the next successful
 * flush; nothing here may raise into the caller. Returns number replayed,
 * or -1 when the session has no spooled drops (Python early-returns). */
int session_drain_spooled_drops(const char *session_id) {
    if (!session_id || !session_id[0]) return -1;

    char dir[512];
    _spool_dir(dir, sizeof(dir));
    char pattern[560];
    snprintf(pattern, sizeof(pattern), "%s/pending-*.json", dir);

    glob_t gl;
    memset(&gl, 0, sizeof(gl));
    if (glob(pattern, 0, NULL, &gl) != 0) return -1; /* no candidates */

    /* Collect matching entries as (ts, seq, name, path, message_json). */
    typedef struct {
        long ts; long seq; char name[128]; char path[600]; char *msg;
    } spool_ent_t;
    spool_ent_t *ents = NULL;
    size_t n_ents = 0, cap = 0;

    for (size_t i = 0; i < gl.gl_pathc; i++) {
        const char *path = gl.gl_pathv[i];
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > 1 << 20) { fclose(f); continue; }
        char *buf = malloc((size_t)sz + 1);
        if (!buf) { fclose(f); continue; }
        size_t rd = fread(buf, 1, (size_t)sz, f);
        buf[rd] = '\0';
        fclose(f);

        char *err = NULL;
        json_t *payload = json_parse(buf, &err);
        free(err);
        free(buf);
        if (!payload || payload->type != JSON_OBJECT) { if (payload) json_free(payload); continue; }

        const char *reason = json_get_str(payload, "reason", "");
        const char *key = json_get_str(payload, "session_key", "");
        long ts = (long)json_get_num(payload, "ts", 0);
        long seq = (long)json_get_num(payload, "seq", 0);
        if (strcmp(reason, SPOOL_DROP_REASON) != 0 || strcmp(key, session_id) != 0) {
            json_free(payload);
            continue;
        }
        json_t *data = json_obj_get(payload, "data");
        json_t *msg = data ? json_obj_get(data, "message") : NULL;
        if (!msg || msg->type != JSON_OBJECT) {
            /* Structurally invalid — remove it (Python unlinks with warning). */
            hermes_log(LOG_WARNING, "session",
                       "Removing structurally invalid transcript spool file %s", path);
            remove(path);
            json_free(payload);
            continue;
        }
        char *msg_ser = json_serialize(msg);
        json_free(payload);
        if (!msg_ser) continue;

        const char *fname = strrchr(path, '/');
        fname = fname ? fname + 1 : path;

        if (n_ents == cap) {
            cap = cap ? cap * 2 : 8;
            ents = realloc(ents, cap * sizeof(*ents));
            if (!ents) { free(msg_ser); n_ents = 0; break; }
        }
        spool_ent_t *e = &ents[n_ents++];
        e->ts = ts; e->seq = seq;
        snprintf(e->name, sizeof(e->name), "%s", fname);
        snprintf(e->path, sizeof(e->path), "%s", path);
        e->msg = msg_ser;
    }
    globfree(&gl);
    if (!ents) return 0;

    /* Sort by (ts, seq, name) — drop order. Insertion sort keeps it simple. */
    for (size_t i = 1; i < n_ents; i++) {
        spool_ent_t k = ents[i];
        size_t j = i;
        while (j > 0 &&
               (ents[j-1].ts > k.ts ||
                (ents[j-1].ts == k.ts &&
                 (ents[j-1].seq > k.seq ||
                  (ents[j-1].seq == k.seq && strcmp(ents[j-1].name, k.name) > 0))))) {
            ents[j] = ents[j-1];
            j--;
        }
        ents[j] = k;
    }

    int replayed = 0;
    int remaining = 0;
    for (size_t i = 0; i < n_ents; i++) {
        if (remaining == 0) {
            if (_replay_transcript_message(session_id, ents[i].msg)) {
                remove(ents[i].path);
                replayed++;
            } else {
                /* First replay failure stops the drain; keep rest for retry. */
                remaining = (int)(n_ents - i);
            }
        } else {
            remaining++;
        }
        free(ents[i].msg);
    }
    free(ents);

    if (replayed > 0)
        hermes_log(LOG_INFO, "session",
                   "Replayed %d spooled transcript message(s) for %s after DB recovery",
                   replayed, session_id);
    return replayed;
}

