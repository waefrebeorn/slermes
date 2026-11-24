/*
 * _delivery_ledger.c - real C11 port of upstream Hermes
 *   persistence/sqlite/gateway/_delivery_ledger.py.
 *
 * The obligation verbs are SQL transactions; this port keeps their decision table and
 * their writes. A "row" here is one delivery_obligations row as JSON; the caller hands
 * in the row set (the verb's SELECT) and gets back the claimed tuples plus the rows as
 * the transaction left them. The transaction/connection plumbing stays on the Python
 * side (it is the store seam, not the behaviour).
 *
 * PoP: w19_deliveryle_311300_expected_profile @ persistence/sqlite/gateway/_delivery_ledger.py:_expected_profile
 * PoP: w19_deliveryle_311300_sweep_recoverable @ persistence/sqlite/gateway/_delivery_ledger.py:sweep_recoverable_obligations
 * PoP: w19_deliveryle_311300_sweep_runtime @ persistence/sqlite/gateway/_delivery_ledger.py:sweep_failed_obligations_for_runtime
 * PoP: w19_deliveryle_311300_runtime_candidates @ persistence/sqlite/gateway/_delivery_ledger.py:list_runtime_retry_candidates
 * PoP: w19_deliveryle_311300_prune @ persistence/sqlite/gateway/_delivery_ledger.py:_prune_unlocked
 */
#include "hermes_json.h"
#include "noc_w19_deliveryle_311300.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBL_MAX_ATTEMPTS        3
#define OBL_STALE_AFTER_S       86400.0
#define OBL_RETENTION_S         604800.0
#define OBL_MAX_ROWS            500

static const char *const FLOOD_MARKER =
    "\xe2\x99\xbb\xef\xb8\x8f Recovered reply \xe2\x80\x94 the messaging platform's "
    "rate limit refused the original, so part of it may already have arrived above:\n\n";
static const char *const RECONNECTED_MARKER =
    "\xe2\x99\xbb\xef\xb8\x8f Recovered reply \xe2\x80\x94 the messaging platform reconnected "
    "after the original delivery failed, so this may be a duplicate:\n\n";

/* ---------- small value helpers ---------- */

static json_t *jcn(json_t *v) { return v ? json_copy(v) : json_null(); }

static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }

static long long jint(json_t *v, long long def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}

static double jnum(json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* Python `a == b` / `a != b` over the stored owner stamps (NULL included). */
static int jsame(json_t *a, json_t *b) {
    int an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
    return 0;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str(error or "").strip().lower() into out. */
static const char *lower_strip(const char *s, char *out, size_t n) {
    size_t i = 0;
    if (!s) s = "";
    while (*s && is_ws(*s)) s++;
    size_t len = strlen(s);
    while (len && is_ws(s[len - 1])) len--;
    for (; i < len && i + 1 < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = '\0';
    return out;
}

/* `flood control exceeded.*?retry in\s+(\d+(?:\.\d+)?)` on an already-lowered text. */
static int raw_flood_wait(const char *lower, double *wait) {
    const char *p = strstr(lower, "flood control exceeded");
    if (!p) return 0;
    p += strlen("flood control exceeded");
    for (;;) {
        const char *r = strstr(p, "retry in");
        if (!r) return 0;
        const char *q = r + strlen("retry in");
        if (*q && is_ws(*q)) {
            while (*q && is_ws(*q)) q++;
            if (isdigit((unsigned char)*q)) {
                *wait = strtod(q, NULL);
                return 1;
            }
        }
        p = r + 1;
    }
}

static double flood_wait_seconds(const char *error) {
    char buf[4096];
    const char *t = lower_strip(error, buf, sizeof buf);
    double wait = 60.0;
    const char *prefix = "flood_control:";
    if (!strncmp(t, prefix, strlen(prefix))) {
        const char *rest = t + strlen(prefix);
        while (*rest && is_ws(*rest)) rest++;
        char *end = NULL;
        double parsed = strtod(rest, &end);
        int ok = end && end != rest;
        if (ok) {
            const char *tail = end;
            while (*tail && is_ws(*tail)) tail++;
            if (*tail) ok = 0;
        }
        wait = ok ? parsed : 60.0;
    } else {
        double raw = 0.0;
        if (raw_flood_wait(t, &raw)) wait = raw;
    }
    return wait > 0 ? wait : 60.0;
}

static int is_flood_error(const char *error) {
    char buf[4096];
    const char *t = lower_strip(error, buf, sizeof buf);
    double raw = 0.0;
    if (!strncmp(t, "flood_control:", strlen("flood_control:"))) return 1;
    return raw_flood_wait(t, &raw);
}

static double flood_not_before(double updated_at, const char *last_error) {
    return updated_at + flood_wait_seconds(last_error);
}

/* ---------- row helpers ---------- */

static json_t *row_state(json_t *row) { return json_obj_get(row, "state"); }
static const char *row_state_s(json_t *row) { return jstr(row_state(row)); }

static int state_in3(const char *s) {
    return s && (!strcmp(s, "pending") || !strcmp(s, "attempting") || !strcmp(s, "failed"));
}

static void row_set(json_t *row, const char *k, json_t *v) { json_set(row, k, v); }

/* ClaimedObligation -> JSON. */
static json_t *claimed_json(const char *oid, const char *session_key, const char *platform,
                            const char *chat_id, const char *thread_id, const char *content,
                            const char *profile, long long attempts, int needs_marker,
                            const char *marker, int runtime, const char *last_error,
                            int adopted, int has_not_before, double not_before) {
    json_t *e = json_object();
    json_set(e, "obligation_id", oid ? json_string(oid) : json_null());
    json_set(e, "session_key", session_key ? json_string(session_key) : json_null());
    json_set(e, "platform", platform ? json_string(platform) : json_null());
    json_set(e, "chat_id", chat_id ? json_string(chat_id) : json_null());
    json_set(e, "thread_id", thread_id ? json_string(thread_id) : json_null());
    json_set(e, "content", content ? json_string(content) : json_null());
    json_set(e, "profile", profile ? json_string(profile) : json_null());
    json_set(e, "attempts", json_number((double)attempts));
    json_set(e, "needs_marker", json_bool(needs_marker ? true : false));
    json_set(e, "marker", marker ? json_string(marker) : json_null());
    json_set(e, "runtime_recovery", json_bool(runtime ? true : false));
    json_set(e, "last_error", last_error ? json_string(last_error) : json_null());
    json_set(e, "adopted", json_bool(adopted ? true : false));
    json_set(e, "not_before", has_not_before ? json_number(not_before) : json_null());
    return e;
}

/* _claimed_row(...): the marker rules live here. `last_error` is the row's PRE-claim error,
 * copied by the caller because the claim itself may clear the column. */
static json_t *claimed_row(json_t *row, long long attempts, const char *profile, int needs_marker,
                           int runtime, int flood, const char *last_error) {
    const char *marker = flood ? FLOOD_MARKER : (runtime ? RECONNECTED_MARKER : NULL);
    return claimed_json(jstr(json_obj_get(row, "obligation_id")),
                        jstr(json_obj_get(row, "session_key")),
                        jstr(json_obj_get(row, "platform")),
                        jstr(json_obj_get(row, "chat_id")),
                        jstr(json_obj_get(row, "thread_id")),
                        jstr(json_obj_get(row, "content")),
                        profile, attempts + 1, needs_marker,
                        (needs_marker && marker) ? marker : NULL,
                        runtime, last_error, 0, 0, 0.0);
}

json_t *w19_deliveryle_311300_expected_profile(json_t *profile) {
    const char *p = jstr(profile);
    if (!p || !p[0] || !strcmp(p, "default")) return json_string("default");
    return json_string(p);
}

/* try: return kb... -> the claim sweep over `rows` (the SELECT of pending/attempting/failed). */
json_t *w19_deliveryle_311300_sweep_recoverable(json_t *rows, double now, json_t *platforms, json_t *targets,
                                                json_t *owner_pid, json_t *owner_started, json_t *dead_owners) {
    json_t *out = json_object();
    json_t *claimed = json_array();
    json_t *after = json_array();
    size_t n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    long long pid = jint(owner_pid, 0);
    int pid_null = !owner_pid || owner_pid->type == JSON_NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_copy(json_get(rows, i));
        if (!row || row->type != JSON_OBJECT) { json_free(row); continue; }
        json_append(after, row);
        const char *state = row_state_s(row);
        if (!state_in3(state)) continue;
        int dead = 0;
        if (dead_owners && dead_owners->type == JSON_ARRAY) {
            for (size_t d = 0; d < json_len(dead_owners); d++) {
                json_t *o = json_get(dead_owners, d);
                if (!o || o->type != JSON_OBJECT) continue;
                if (jsame(json_obj_get(o, "pid"), json_obj_get(row, "owner_pid")) &&
                    jsame(json_obj_get(o, "started_at"), json_obj_get(row, "owner_started_at"))) {
                    dead = 1;
                    break;
                }
            }
        }
        if (!dead) continue;                     /* a live gateway still owns this row */
        long long attempts = jint(json_obj_get(row, "attempts"), 0);
        double created_at = jnum(json_obj_get(row, "created_at"), 0.0);
        if (attempts >= OBL_MAX_ATTEMPTS || (now - created_at) > OBL_STALE_AFTER_S) {
            row_set(row, "state", json_string("abandoned"));
            row_set(row, "updated_at", json_number(now));
            continue;
        }
        const char *platform = jstr(json_obj_get(row, "platform"));
        const char *adapter_profile = jstr(json_obj_get(row, "adapter_profile"));
        if (platforms && platforms->type == JSON_ARRAY) {
            int hit = 0;
            for (size_t k = 0; k < json_len(platforms); k++) {
                const char *p = jstr(json_get(platforms, k));
                if (p && platform && !strcmp(p, platform)) { hit = 1; break; }
            }
            if (!hit) continue;                  /* no adapter this boot */
        }
        if (targets && targets->type == JSON_ARRAY) {
            int hit = 0;
            for (size_t k = 0; k < json_len(targets); k++) {
                json_t *pair = json_get(targets, k);
                if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) continue;
                const char *p = jstr(json_get(pair, 0));
                json_t *prof = json_get(pair, 1);
                int same = (p && platform && !strcmp(p, platform)) &&
                           ((!adapter_profile && (!prof || prof->type == JSON_NULL)) ||
                            (adapter_profile && jstr(prof) && !strcmp(adapter_profile, jstr(prof))));
                if (same) { hit = 1; break; }
            }
            if (!hit) continue;
        }
        double updated_at = jnum(json_obj_get(row, "updated_at"), 0.0);
        int flood = state && !strcmp(state, "failed") && is_flood_error(jstr(json_obj_get(row, "last_error")));
        if (flood && now < flood_not_before(updated_at, jstr(json_obj_get(row, "last_error")))) {
            /* Inside the platform's wait: adopt without spending an attempt. */
            row_set(row, "owner_pid", pid_null ? json_null() : json_number((double)pid));
            row_set(row, "owner_started_at", jcn(owner_started));
            if (!adapter_profile) row_set(row, "adapter_profile", json_string("default"));
            json_append(claimed, claimed_json(jstr(json_obj_get(row, "obligation_id")),
                                              jstr(json_obj_get(row, "session_key")),
                                              jstr(json_obj_get(row, "platform")),
                                              jstr(json_obj_get(row, "chat_id")),
                                              jstr(json_obj_get(row, "thread_id")),
                                              jstr(json_obj_get(row, "content")),
                                              adapter_profile ? adapter_profile : "default",
                                              attempts, 0, NULL, 0, NULL, 1, 1,
                                              flood_not_before(updated_at, jstr(json_obj_get(row, "last_error")))));
            continue;
        }
        /* Claim: re-stamp the owner, move to attempting, spend an attempt; a claimed flood row drops its error. */
        row_set(row, "owner_pid", pid_null ? json_null() : json_number((double)pid));
        row_set(row, "owner_started_at", jcn(owner_started));
        row_set(row, "attempts", json_number((double)(attempts + 1)));
        row_set(row, "updated_at", json_number(now));
        if (!adapter_profile) row_set(row, "adapter_profile", json_string("default"));
        row_set(row, "state", json_string("attempting"));
        if (flood) row_set(row, "last_error", json_null());
        /* the recoverable claim passes no last_error: the row's pre-claim error is not carried */
        json_append(claimed, claimed_row(row, attempts, adapter_profile ? adapter_profile : "default",
                                         (strcmp(state, "pending") != 0) || attempts > 0, 0, flood,
                                         NULL));
    }
    json_set(out, "claimed", claimed);
    json_set(out, "rows", after);
    return out;
}

json_t *w19_deliveryle_311300_sweep_runtime(json_t *rows, const char *platform, double now, json_t *profile,
                                            json_t *owner_pid, json_t *owner_started, json_t *due) {
    json_t *out = json_object();
    json_t *claimed = json_array();
    json_t *after = json_array();
    size_t n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    int started_null = !owner_started || owner_started->type == JSON_NULL;
    char expected_buf[256];
    json_t *exp_j = w19_deliveryle_311300_expected_profile(profile);
    snprintf(expected_buf, sizeof expected_buf, "%s", jstr(exp_j) ? jstr(exp_j) : "default");
    json_free(exp_j);
    if (!started_null) {
        for (size_t i = 0; i < n; i++) {
            json_t *row = json_copy(json_get(rows, i));
            if (!row || row->type != JSON_OBJECT) { json_free(row); continue; }
            json_append(after, row);
            const char *st = row_state_s(row);
            if (!st || strcmp(st, "failed")) continue;
            const char *p = jstr(json_obj_get(row, "platform"));
            if (!platform || !p || strcmp(p, platform)) continue;
            /* find the due candidate for this row id */
            json_t *item = NULL;
            if (due && due->type == JSON_ARRAY) {
                for (size_t d = 0; d < json_len(due); d++) {
                    json_t *c = json_get(due, d);
                    if (!c || c->type != JSON_OBJECT) continue;
                    const char *cid = jstr(json_obj_get(c, "obligation_id"));
                    const char *rid = jstr(json_obj_get(row, "obligation_id"));
                    if (cid && rid && !strcmp(cid, rid)) { item = c; break; }
                }
            }
            if (!item) continue;
            const char *adapter_profile = jstr(json_obj_get(row, "adapter_profile"));
            if (!adapter_profile || strcmp(adapter_profile, expected_buf)) continue;
            if (!jsame(json_obj_get(row, "owner_pid"), owner_pid)) continue;
            if (!jsame(json_obj_get(row, "owner_started_at"), owner_started)) continue;
            const char *last_error = jstr(json_obj_get(row, "last_error"));
            json_t *c_le = json_obj_get(item, "last_error");
            int le_same = jsame(c_le, json_obj_get(row, "last_error"));
            if (!le_same) continue;
            if (jnum(json_obj_get(row, "updated_at"), 0.0) != jnum(json_obj_get(item, "updated_at"), 0.0)) continue;
            if (jint(json_obj_get(row, "attempts"), 0) != jint(json_obj_get(item, "attempts"), 0)) continue;
            long long attempts = jint(json_obj_get(row, "attempts"), 0);
            double created_at = jnum(json_obj_get(row, "created_at"), 0.0);
            if (attempts >= OBL_MAX_ATTEMPTS || (now - created_at) > OBL_STALE_AFTER_S) {
                row_set(row, "state", json_string("abandoned"));
                row_set(row, "updated_at", json_number(now));
                continue;
            }
            if (now < jnum(json_obj_get(item, "not_before"), 0.0)) continue;
            char le_buf[2048];
            snprintf(le_buf, sizeof le_buf, "%s", last_error ? last_error : "");
            row_set(row, "state", json_string("attempting"));
            row_set(row, "attempts", json_number((double)(attempts + 1)));
            row_set(row, "updated_at", json_number(now));
            row_set(row, "last_error", json_null());
            json_append(claimed, claimed_row(row, attempts, adapter_profile, 1, 1,
                                             is_flood_error(last_error),
                                             last_error ? le_buf : NULL));
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            json_t *row = json_copy(json_get(rows, i));
            if (row && row->type == JSON_OBJECT) json_append(after, row);
            else json_free(row);
        }
    }
    json_set(out, "claimed", claimed);
    json_set(out, "rows", after);
    return out;
}

json_t *w19_deliveryle_311300_runtime_candidates(json_t *rows, const char *platform, json_t *profile,
                                                 json_t *owner_pid, json_t *owner_started) {
    json_t *out = json_array();
    size_t n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    int started_null = !owner_started || owner_started->type == JSON_NULL;
    if (started_null) return out;
    json_t *exp_j = w19_deliveryle_311300_expected_profile(profile);
    char expected[256];
    snprintf(expected, sizeof expected, "%s", jstr(exp_j) ? jstr(exp_j) : "default");
    json_free(exp_j);
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_OBJECT) continue;
        const char *st = row_state_s(row);
        if (!st || strcmp(st, "failed")) continue;
        const char *p = jstr(json_obj_get(row, "platform"));
        if (!platform || !p || strcmp(p, platform)) continue;
        const char *ap = jstr(json_obj_get(row, "adapter_profile"));
        if (!ap || strcmp(ap, expected)) continue;
        if (!jsame(json_obj_get(row, "owner_pid"), owner_pid)) continue;
        if (!jsame(json_obj_get(row, "owner_started_at"), owner_started)) continue;
        json_t *e = json_object();
        json_set(e, "obligation_id", jcn(json_obj_get(row, "obligation_id")));
        json_set(e, "updated_at", jcn(json_obj_get(row, "updated_at")));
        json_set(e, "last_error", jcn(json_obj_get(row, "last_error")));
        json_set(e, "attempts", jcn(json_obj_get(row, "attempts")));
        json_append(out, e);
    }
    return out;
}

/* state rank for the row-cap prune: delivered 0, abandoned 1, everything else 2. */
static int state_rank(const char *s) {
    if (s && !strcmp(s, "delivered")) return 0;
    if (s && !strcmp(s, "abandoned")) return 1;
    return 2;
}

json_t *w19_deliveryle_311300_prune(json_t *rows, double now) {
    json_t *out = json_array();
    size_t n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    json_t **keep = (json_t **)calloc(n ? n : 1, sizeof(json_t *));
    size_t nk = 0;
    if (!keep) return out;
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_copy(json_get(rows, i));
        if (!row || row->type != JSON_OBJECT) { json_free(row); continue; }
        const char *st = row_state_s(row);
        double updated = jnum(json_obj_get(row, "updated_at"), 0.0);
        int settled = st && (!strcmp(st, "delivered") || !strcmp(st, "abandoned"));
        if (settled && updated < now - OBL_RETENTION_S) { json_free(row); continue; }
        keep[nk++] = row;
    }
    if (nk > (size_t)OBL_MAX_ROWS) {
        size_t over = nk - (size_t)OBL_MAX_ROWS;
        /* stable selection of the `over` rows ordered by (rank, updated_at) ascending */
        char *gone = (char *)calloc(nk, 1);
        if (gone) {
            for (size_t pick = 0; pick < over; pick++) {
                size_t best = nk;
                for (size_t i = 0; i < nk; i++) {
                    if (gone[i]) continue;
                    if (best == nk) { best = i; continue; }
                    int ra = state_rank(row_state_s(keep[i])), rb = state_rank(row_state_s(keep[best]));
                    double ua = jnum(json_obj_get(keep[i], "updated_at"), 0.0);
                    double ub = jnum(json_obj_get(keep[best], "updated_at"), 0.0);
                    if (ra < rb || (ra == rb && ua < ub)) best = i;
                }
                if (best < nk) gone[best] = 1;
            }
            size_t w = 0;
            for (size_t i = 0; i < nk; i++) {
                if (gone[i]) json_free(keep[i]);
                else keep[w++] = keep[i];
            }
            nk = w;
            free(gone);
        }
    }
    for (size_t i = 0; i < nk; i++) json_append(out, keep[i]);
    free(keep);
    return out;
}
