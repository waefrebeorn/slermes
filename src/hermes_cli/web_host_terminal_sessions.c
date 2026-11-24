/*
 * web_host_terminal_sessions.c - C11 port of
 * hermes_cli/web_host_terminal_sessions.py
 * (gates w34_webhostter_a3f534_p1, w34_webhostter_a3f534_p2).
 *
 * PoP: w34_webhostter_a3f534_p1_constants @ hermes_cli/web_host_terminal_sessions.py:<module + pty_session budgets>
 * PoP: w34_webhostter_a3f534_p1_request_identity @ hermes_cli/web_host_terminal_sessions.py:_request_identity
 * PoP: w34_webhostter_a3f534_p1_home_live @ hermes_cli/web_host_terminal_sessions.py:_home_live (HostOwner.current)
 * PoP: w34_webhostter_a3f534_p1_request_home @ hermes_cli/web_host_terminal_sessions.py:_request_home
 * PoP: w34_webhostter_a3f534_p1_resolve_verdict @ hermes_cli/web_host_terminal_sessions.py:HostTerminalRegistry.resolve (+ _gone, HostOwner.matches/admit)
 * PoP: w34_webhostter_a3f534_p1_get_registry_state @ hermes_cli/web_host_terminal_sessions.py:get_host_terminals
 * PoP: w34_webhostter_a3f534_p1_lifespan_exit @ hermes_cli/web_host_terminal_sessions.py:host_terminal_lifespan
 * PoP: w34_webhostter_a3f534_p1_reap_plan @ hermes_cli/web_host_terminal_sessions.py:HostTerminalRegistry.reap_idle (+ _remember_exit, _exited)
 * PoP: w34_webhostter_a3f534_p2_metadata_payload @ hermes_cli/web_host_terminal_sessions.py:_metadata
 * PoP: w34_webhostter_a3f534_p2_closed_payload @ hermes_cli/web_host_terminal_sessions.py:_send_closed
 * PoP: w34_webhostter_a3f534_p2_frame_payload @ hermes_cli/web_host_terminal_sessions.py:_frame_payload
 * PoP: w34_webhostter_a3f534_p2_frame_cost @ hermes_cli/web_host_terminal_sessions.py:_frame_cost
 * PoP: w34_webhostter_a3f534_p2_queue_full @ hermes_cli/web_host_terminal_sessions.py:_InputQueue.full
 * PoP: w34_webhostter_a3f534_p2_input_runs @ hermes_cli/web_host_terminal_sessions.py:_input_runs
 * PoP: w34_webhostter_a3f534_p2_resize_undo @ hermes_cli/web_host_terminal_sessions.py:_resize
 * PoP: w34_webhostter_a3f534_p2_admit_verdict @ hermes_cli/web_host_terminal_sessions.py:_admit
 * PoP: w34_webhostter_a3f534_p2_token_ok @ hermes_cli/web_host_terminal_sessions.py:TOKEN_RE
 *
 * What is ported is exactly what the values decide: who a request is, whether
 * a profile home is still live, which home a ``profile`` parameter selects,
 * resolve/attach verdicts and the order they are applied in, the lifespan
 * registry stack, one reaper tick's retirements and its bounded exit memory,
 * the frames a terminal writes (META_PREFIX starts with NUL, so payloads are
 * compared as byte arrays), the input queue's backpressure bound, keystroke
 * coalescing around resize frames, resize undo bookkeeping, and ``_admit``'s
 * verdict.  The one-line constant refusal `_reap_one_idle_or_raise` ->
 * RegistryFull is carried as max_shells in the constants object: eviction is
 * never offered, capacity is refused.
 *
 * Seams, deliberately left in the caller: the asyncio loop and its threads,
 * the PTY spawn/drain/write/resize path, WebSocket send/close/receive, the
 * reaper task, ThreadPoolExecutor shutdown, path resolution (Path.resolve),
 * every filesystem read behind _home_live, and the session output counter.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w34_webhostter_a3f534_p1.h"
#include "noc_w34_webhostter_a3f534_p2.h"

/* The frame prefix every host-terminal payload starts with ("\0HERMES_..."). */
static const char WS_META_PREFIX[] = "\0HERMES_TERMINAL_META:";
#define WS_META_PREFIX_N (sizeof(WS_META_PREFIX) - 1)

static const char WS_NOT_RUNNING[] =
    "Host terminal service is not running; start the application lifespan first.";

/* ------------------------------------------------------------------ */
/* shared value helpers                                                */
/* ------------------------------------------------------------------ */

/* Python truthiness of a JSON value. */
static int ws_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* A string member, or NULL when the member is missing/null/not a string. */
static const char *ws_str(const json_t *obj, const char *key) {
    const json_t *v = obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val ? v->str_val : "";
}

/* A numeric member: int, float or bool (Python's int()/float() view). */
static int ws_num(const json_t *obj, const char *key, double *out) {
    const json_t *v = obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static char *ws_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* str.strip(): leading/trailing Python whitespace (ASCII set here). */
static char *ws_strip(const char *s) {
    const char *a = s ? s : "";
    const char *b;
    size_t n;
    char *p;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, a, n);
    p[n] = '\0';
    return p;
}

/* requested.lower() != "current" */
static int ws_is_current(const char *s) {
    static const char k[] = "current";
    size_t i;
    for (i = 0; k[i]; i++)
        if (!s[i] || tolower((unsigned char)s[i]) != k[i]) return 0;
    return s[i] == '\0';
}

/* TOKEN_RE.fullmatch: exactly 43 of [A-Za-z0-9_-]. */
static int ws_token_ok(const char *token) {
    size_t i;
    if (!token) return 0;
    if (strlen(token) != W34_WEBHOSTTER_A3F534_TOKEN_LEN) return 0;
    for (i = 0; token[i]; i++) {
        unsigned char c = (unsigned char)token[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* Python tuple equality over the [provider, user] identity pairs. */
static int ws_ident_eq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return 0;
    n = json_len(a);
    if (n != json_len(b)) return 0;
    for (i = 0; i < n; i++) {
        const json_t *x = json_get(a, i);
        const json_t *y = json_get(b, i);
        if (!x || !y || x->type != JSON_STRING || y->type != JSON_STRING) return 0;
        if (strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "") != 0) return 0;
    }
    return 1;
}

/* _request_home's selection: the current home for ""/"current", else the
 * profile table's directory.  1 = resolved, 0 = the HTTPException the route
 * maps to the 4410 close (unknown/invalid profile).  *out is malloc'd. */
static int ws_home_for(const char *profile, const char *current_home,
                       const json_t *profiles, char **out) {
    char *stripped = ws_strip(profile ? profile : "");
    char *result = NULL;
    if (!stripped) return 0;
    if (stripped[0] != '\0' && !ws_is_current(stripped)) {
        const char *dir = ws_str(profiles, stripped);
        if (dir) result = ws_dup(dir);
    } else if (current_home) {
        result = ws_dup(current_home);
    }
    free(stripped);
    if (!result) return 0;
    *out = result;
    return 1;
}

/* ------------------------------------------------------------------ */
/* payload buffer + json.dumps-compatible escaping                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n, cap;
} wsbuf;

static void wsb_init(wsbuf *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void wsb_put(wsbuf *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 128;
        char *np;
        while (nc < b->n + len + 1) nc *= 2;
        np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void wsb_puts(wsbuf *b, const char *s) { wsb_put(b, s, strlen(s)); }
static void wsb_putc(wsbuf *b, char c) { wsb_put(b, &c, 1); }

static void wsb_uescape(wsbuf *b, unsigned cp) {
    char tmp[16];
    if (cp > 0xFFFF) {
        unsigned v = cp - 0x10000u;
        unsigned hi = 0xD800u + (v >> 10);
        unsigned lo = 0xDC00u + (v & 0x3FFu);
        snprintf(tmp, sizeof tmp, "\\u%04x\\u%04x", hi, lo);
        wsb_puts(b, tmp);
    } else {
        snprintf(tmp, sizeof tmp, "\\u%04x", cp);
        wsb_puts(b, tmp);
    }
}

/* json.dumps(s) with ensure_ascii=True: quotes, the short escapes, \u00xx for
 * the other control characters, and \uxxxx for everything outside 0x20..0x7e. */
static void wsb_jstr(wsbuf *b, const char *s) {
    const unsigned char *u = (const unsigned char *)(s ? s : "");
    wsb_putc(b, '"');
    while (*u) {
        unsigned cp;
        unsigned char c = *u;
        int len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && u[1] && (u[1] & 0xC0) == 0x80) {
            cp = ((unsigned)(c & 0x1F) << 6) | (unsigned)(u[1] & 0x3F);
            len = 2;
        } else if ((c & 0xF0) == 0xE0 && u[1] && u[2] &&
                   (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
            cp = ((unsigned)(c & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) |
                 (unsigned)(u[2] & 0x3F);
            len = 3;
        } else if ((c & 0xF8) == 0xF0 && u[1] && u[2] && u[3] &&
                   (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 &&
                   (u[3] & 0xC0) == 0x80) {
            cp = ((unsigned)(c & 0x07) << 18) | ((unsigned)(u[1] & 0x3F) << 12) |
                 ((unsigned)(u[2] & 0x3F) << 6) | (unsigned)(u[3] & 0x3F);
            len = 4;
        } else {
            cp = 0xFFFD;
        }
        u += len;
        if (cp == '"') wsb_puts(b, "\\\"");
        else if (cp == '\\') wsb_puts(b, "\\\\");
        else if (cp == 0x08) wsb_puts(b, "\\b");
        else if (cp == 0x0C) wsb_puts(b, "\\f");
        else if (cp == 0x0A) wsb_puts(b, "\\n");
        else if (cp == 0x0D) wsb_puts(b, "\\r");
        else if (cp == 0x09) wsb_puts(b, "\\t");
        else if (cp < 0x20 || cp > 0x7E) wsb_uescape(b, cp);
        else wsb_putc(b, (char)cp);
    }
    wsb_putc(b, '"');
}

/* json.dumps of a number: integral values as integers. */
static void wsb_jnum(wsbuf *b, double v) {
    char tmp[40];
    if (v == floor(v) && fabs(v) < 1e15) snprintf(tmp, sizeof tmp, "%lld", (long long)v);
    else snprintf(tmp, sizeof tmp, "%.17g", v);
    wsb_puts(b, tmp);
}

static json_t *ws_bytes_json(const char *p, size_t n) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(arr, json_number((double)(unsigned char)p[i]));
    return arr;
}

/* ------------------------------------------------------------------ */
/* p1: budgets                                                         */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_constants(void) {
    json_t *o = json_object();
    json_set(o, "retention_seconds", json_number(W34_WEBHOSTTER_A3F534_RETENTION_SECONDS));
    json_set(o, "max_shells", json_number(W34_WEBHOSTTER_A3F534_MAX_SHELLS));
    json_set(o, "exits_kept", json_number(W34_WEBHOSTTER_A3F534_EXITS_KEPT));
    json_set(o, "input_queue_frames", json_number(W34_WEBHOSTTER_A3F534_INPUT_QUEUE_FRAMES));
    json_set(o, "input_queue_bytes", json_number(W34_WEBHOSTTER_A3F534_INPUT_QUEUE_BYTES));
    json_set(o, "frame_overhead_bytes", json_number(W34_WEBHOSTTER_A3F534_FRAME_OVERHEAD_BYTES));
    json_set(o, "liveness_probe_seconds", json_number(1.0));
    json_set(o, "resize_answer_seconds", json_number(1.0));
    json_set(o, "viewer_send_timeout_seconds", json_number(5.0));
    json_set(o, "ws_close_process_exited", json_number(4410));
    json_set(o, "ws_close_process_exited_reason", json_string("Process exited"));
    json_set(o, "ws_close_viewer_stalled", json_number(1001));
    json_set(o, "ws_close_viewer_stalled_reason",
             json_string("Terminal viewer stopped reading; reconnect"));
    json_set(o, "token_regex", json_string("^[A-Za-z0-9_-]{43}$"));
    json_set(o, "meta_prefix_bytes", ws_bytes_json(WS_META_PREFIX, WS_META_PREFIX_N));
    /* the pump's resize pattern, as source text (web_server_chat._RESIZE_RE) */
    json_set(o, "resize_regex", json_string("\\x1b\\[RESIZE:(\\d+);(\\d+)\\]"));
    return o;
}

/* ------------------------------------------------------------------ */
/* p1: _request_identity                                               */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_request_identity(const json_t *state) {
    const json_t *identity = state && state->type == JSON_OBJECT
                                 ? json_obj_get(state, "identity")
                                 : NULL;
    const char *user = ws_str(identity, "user_id");
    const char *provider = ws_str(identity, "provider");
    json_t *out = json_object();
    /* `identity = getattr(ws, "_hermes_auth_identity", None) or {}`, then a
     * non-empty str check on each member; anything else raises TerminalDenied. */
    if (!user || !user[0] || !provider || !provider[0]) {
        json_set(out, "denied", json_bool(true));
        return out;
    }
    json_set(out, "provider", json_string(provider));
    json_set(out, "user", json_string(user));
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: _home_live                                                      */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_home_live(const json_t *state) {
    int named = ws_truthy(state ? json_obj_get(state, "named") : NULL);
    int exists = ws_truthy(state ? json_obj_get(state, "exists") : NULL);
    int tombstoned = ws_truthy(state ? json_obj_get(state, "tombstoned") : NULL);
    int is_dir = ws_truthy(state ? json_obj_get(state, "is_dir") : NULL);
    /* assert_named_profile_home_live raises FileNotFoundError for a missing or
     * tombstoned *named* home; after that only home.is_dir() remains. */
    if (named && (tombstoned || !exists)) return json_bool(false);
    return json_bool(is_dir ? true : false);
}

/* ------------------------------------------------------------------ */
/* p1: _request_home                                                   */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_request_home(const json_t *state) {
    const char *profile = ws_str(state, "profile");
    const char *current = ws_str(state, "current_home");
    const json_t *profiles = state && state->type == JSON_OBJECT
                                 ? json_obj_get(state, "profiles")
                                 : NULL;
    char *home = NULL;
    json_t *out = json_object();
    if (!ws_home_for(profile, current, profiles, &home)) {
        json_set(out, "expired", json_bool(true));
        return out;
    }
    json_set(out, "home", json_string(home));
    free(home);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: HostTerminalRegistry.resolve (with _gone and HostOwner.matches) */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_resolve_verdict(const json_t *state) {
    const json_t *identity = state ? json_obj_get(state, "identity") : NULL;
    const json_t *owner_identity = state ? json_obj_get(state, "owner_identity") : NULL;
    const json_t *exits = state ? json_obj_get(state, "exits") : NULL;
    const char *token = ws_str(state, "token");
    const char *owner_home = ws_str(state, "owner_home");
    const char *current = ws_str(state, "current_home");
    const json_t *profiles = state ? json_obj_get(state, "profiles") : NULL;
    const char *request_profile = ws_str(state, "request_profile");
    int closing = ws_truthy(state ? json_obj_get(state, "closing") : NULL);
    const json_t *gone;
    char *home = NULL;
    int verdict = 0; /* 0 ok, 1 denied, 2 expired, 3 exited */
    const char *name;

    /* _gone: a token with no registered shell learns whether its own shell
     * exited by itself; anything else is expired. */
    gone = exits && exits->type == JSON_OBJECT && token ? json_obj_get(exits, token) : NULL;
    if (!ws_truthy(state ? json_obj_get(state, "session_present") : NULL) ||
        !ws_truthy(state ? json_obj_get(state, "owner_present") : NULL)) {
        verdict = (gone && ws_ident_eq(gone, identity)) ? 3 : 2;
    } else if (!ws_ident_eq(owner_identity, identity)) {
        verdict = 1; /* TerminalDenied: another identity's shell */
    } else if (ws_truthy(state ? json_obj_get(state, "ssh_terminal") : NULL)) {
        verdict = 1; /* HostOwner.matches: an SSH-owned shell never matches */
    } else if (!ws_home_for(request_profile, current, profiles, &home)) {
        verdict = 2; /* the HTTPException the route closes with 4410 */
    } else if (!owner_home || strcmp(home, owner_home) != 0) {
        verdict = 1; /* HostOwner.matches: a different profile home */
    } else if (!ws_truthy(state ? json_obj_get(state, "home_live") : NULL)) {
        verdict = 2; /* HostOwner.admit: the profile home is gone */
    } else if (ws_truthy(state ? json_obj_get(state, "reap_during_validate") : NULL)) {
        verdict = (gone && ws_ident_eq(gone, identity)) ? 3 : 2;
    } else if (!closing &&
               (!ws_truthy(state ? json_obj_get(state, "alive") : NULL) ||
                !ws_truthy(state ? json_obj_get(state, "bridge_alive") : NULL))) {
        verdict = 3; /* _exited: only the shell's own exit leaves a dead one */
    } else if (!closing && ws_truthy(state ? json_obj_get(state, "expired") : NULL)) {
        verdict = 2;
    }
    free(home);

    name = verdict == 0 ? "ok" : verdict == 1 ? "denied"
           : verdict == 2 ? "expired" : "exited";
    {
        json_t *out = json_object();
        json_set(out, "verdict", json_string(name));
        return out;
    }
}

/* ------------------------------------------------------------------ */
/* p1: get_host_terminals                                              */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_get_registry_state(const json_t *state) {
    int present = ws_truthy(state ? json_obj_get(state, "present") : NULL);
    int closed = ws_truthy(state ? json_obj_get(state, "closed") : NULL);
    json_t *out = json_object();
    if (!present || closed) json_set(out, "error", json_string(WS_NOT_RUNNING));
    else json_set(out, "ok", json_bool(true));
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: host_terminal_lifespan teardown                                 */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p1_lifespan_exit(const json_t *state) {
    const json_t *seeded = state ? json_obj_get(state, "seeded") : NULL;
    const char *host_now = ws_str(state, "host_now");
    size_t cap = (seeded && seeded->type == JSON_ARRAY ? json_len(seeded) : 0) + 2;
    const char **ids = (const char **)calloc(cap, sizeof(char *));
    size_t n = 0, i;
    int has_host = 1;
    const char *host = NULL;
    json_t *out, *regs;

    if (!ids) return json_object();
    for (i = 0; seeded && seeded->type == JSON_ARRAY && i < json_len(seeded); i++) {
        const json_t *e = json_get(seeded, i);
        if (e && e->type == JSON_STRING) ids[n++] = e->str_val ? e->str_val : "";
    }
    ids[n++] = "ME";                  /* registries.append(registry) */
    for (i = n; i-- > 0;) {           /* registries.remove(registry) */
        if (strcmp(ids[i], "ME") == 0) {
            memmove(&ids[i], &ids[i + 1], (n - i - 1) * sizeof(char *));
            n--;
            break;
        }
    }
    /* if getattr(app.state, "host_terminals", None) is registry: restore the
     * previous registry (the top of the stack) or delete the attribute. */
    if (!host_now) {
        has_host = 0;
    } else if (strcmp(host_now, "ME") == 0) {
        if (n > 0) host = ids[n - 1];
        else has_host = 0;
    } else {
        host = host_now;
    }

    out = json_object();
    regs = json_array();
    for (i = 0; i < n; i++) json_append(regs, json_string(ids[i]));
    if (n > 0) {
        json_set(out, "registries", regs);
    } else {
        json_free(regs);
        json_set(out, "registries", json_null());
    }
    json_set(out, "host_terminals", has_host && host ? json_string(host) : json_null());
    json_set(out, "has_registries", json_bool(n > 0));
    json_set(out, "has_host_terminals", json_bool(has_host ? true : false));
    free(ids);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: one reap_idle tick                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *token;
    const json_t *ident;
    int live;
    int gone;
} rp_owner;

typedef struct {
    const char *token;
    int alive, bridge_alive, attached, has_det;
    double det;
    int gone;
} rp_sess;

typedef struct {
    char *token;
    json_t *ident;
    double at;
    int gone;
} rp_exit;

static int rp_expired(const rp_sess *s, double now) {
    return !s->attached && s->has_det &&
           now - s->det > W34_WEBHOSTTER_A3F534_RETENTION_SECONDS;
}

static int rp_reapable(const rp_sess *s, double now) {
    return !s->alive || !s->bridge_alive || rp_expired(s, now);
}

static int rp_exited(const rp_sess *s) { return !s->alive || !s->bridge_alive; }

json_t *w34_webhostter_a3f534_p1_reap_plan(const json_t *state) {
    const json_t *owners_j = state ? json_obj_get(state, "owners") : NULL;
    const json_t *sessions_j = state ? json_obj_get(state, "sessions") : NULL;
    const json_t *exits_j = state ? json_obj_get(state, "exits") : NULL;
    double now = 0.0;
    size_t no = 0, ns = 0, ne = 0, i, j, k;
    rp_owner *owners;
    rp_sess *sessions;
    rp_exit *exits;
    json_t *out, *arr;

    if (!ws_num(state, "now", &now)) now = 0.0;
    if (owners_j && owners_j->type == JSON_ARRAY) no = json_len(owners_j);
    if (sessions_j && sessions_j->type == JSON_ARRAY) ns = json_len(sessions_j);
    if (exits_j && exits_j->type == JSON_ARRAY) ne = json_len(exits_j);

    owners = (rp_owner *)calloc(no + 1, sizeof(rp_owner));
    sessions = (rp_sess *)calloc(ns + 1, sizeof(rp_sess));
    /* every owner can append one remembered exit on top of the seeded ones */
    exits = (rp_exit *)calloc(ne + no + 1, sizeof(rp_exit));
    if (!owners || !sessions || !exits) {
        free(owners);
        free(sessions);
        free(exits);
        return json_object();
    }

    for (i = 0; i < no; i++) {
        const json_t *e = json_get(owners_j, i);
        owners[i].token = ws_str(e, "token");
        owners[i].ident = e && e->type == JSON_OBJECT ? json_obj_get(e, "identity") : NULL;
        owners[i].live =
            ws_truthy(e && e->type == JSON_OBJECT ? json_obj_get(e, "live") : NULL);
    }
    for (i = 0; i < ns; i++) {
        const json_t *e = json_get(sessions_j, i);
        double det = 0.0;
        sessions[i].token = ws_str(e, "token");
        sessions[i].alive =
            ws_truthy(e && e->type == JSON_OBJECT ? json_obj_get(e, "alive") : NULL);
        sessions[i].bridge_alive =
            ws_truthy(e && e->type == JSON_OBJECT ? json_obj_get(e, "bridge_alive") : NULL);
        sessions[i].attached =
            ws_truthy(e && e->type == JSON_OBJECT ? json_obj_get(e, "attached") : NULL);
        sessions[i].has_det = ws_num(e, "last_detached_at", &det);
        sessions[i].det = det;
    }
    for (i = 0; i < ne; i++) {
        const json_t *e = json_get(exits_j, i);
        const json_t *tok = e && e->type == JSON_ARRAY ? json_get(e, 0) : NULL;
        const json_t *ident = e && e->type == JSON_ARRAY ? json_get(e, 1) : NULL;
        const json_t *at = e && e->type == JSON_ARRAY ? json_get(e, 2) : NULL;
        exits[i].token = (tok && tok->type == JSON_STRING) ? ws_dup(tok->str_val) : ws_dup("");
        exits[i].ident = ident ? json_copy(ident) : json_null();
        exits[i].at = (at && at->type == JSON_NUMBER) ? at->num_val : 0.0;
        exits[i].gone = 0;
    }

    /* owners snapshot: current() is one filesystem read per owner */
    for (i = 0; i < no; i++) {
        rp_sess *s = NULL;
        if (owners[i].gone) continue;
        for (j = 0; j < ns; j++) {
            if (!sessions[j].gone && owners[i].token && sessions[j].token &&
                strcmp(sessions[j].token, owners[i].token) == 0) {
                s = &sessions[j];
                break;
            }
        }
        if (!s) continue;
        if (!owners[i].live || rp_reapable(s, now)) {
            /* _remember_exit only for a shell that exited by itself under a
             * live owner; a retired profile is not the shell's own exit. */
            if (owners[i].live && rp_exited(s)) {
                int found = 0;
                for (k = 0; k < ne; k++) {
                    if (!exits[k].gone && strcmp(exits[k].token, owners[i].token) == 0) {
                        json_free(exits[k].ident);
                        exits[k].ident =
                            owners[i].ident ? json_copy(owners[i].ident) : json_null();
                        exits[k].at = now;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    exits[ne].token = ws_dup(owners[i].token);
                    exits[ne].ident = owners[i].ident ? json_copy(owners[i].ident) : json_null();
                    exits[ne].at = now;
                    exits[ne].gone = 0;
                    ne++;
                }
                while (ne > (size_t)W34_WEBHOSTTER_A3F534_EXITS_KEPT) {
                    free(exits[0].token);
                    json_free(exits[0].ident);
                    memmove(&exits[0], &exits[1], (ne - 1) * sizeof(rp_exit));
                    ne--;
                }
            }
            s->gone = 1;        /* self._sessions.pop */
            owners[i].gone = 1; /* self.owners.pop */
        }
    }
    /* for token in list(self.owners): if token not in self._sessions: pop */
    for (i = 0; i < no; i++) {
        int has = 0;
        if (owners[i].gone) continue;
        for (j = 0; j < ns; j++)
            if (!sessions[j].gone && owners[i].token && sessions[j].token &&
                strcmp(sessions[j].token, owners[i].token) == 0) {
                has = 1;
                break;
            }
        if (!has) owners[i].gone = 1;
    }
    /* for token, (_, at) in self.exits.items(): if now - at > self._ttl */
    for (k = 0; k < ne; k++)
        if (!exits[k].gone &&
            now - exits[k].at > W34_WEBHOSTTER_A3F534_RETENTION_SECONDS)
            exits[k].gone = 1;

    out = json_object();
    arr = json_array();
    for (j = 0; j < ns; j++)
        if (!sessions[j].gone)
            json_append(arr, json_string(sessions[j].token ? sessions[j].token : ""));
    json_set(out, "sessions", arr);
    arr = json_array();
    for (i = 0; i < no; i++)
        if (!owners[i].gone)
            json_append(arr, json_string(owners[i].token ? owners[i].token : ""));
    json_set(out, "owners", arr);
    arr = json_array();
    for (k = 0; k < ne; k++) {
        json_t *row;
        if (exits[k].gone) continue;
        row = json_array();
        json_append(row, json_string(exits[k].token ? exits[k].token : ""));
        json_append(row, exits[k].ident ? json_copy(exits[k].ident) : json_null());
        json_append(row, json_number(exits[k].at));
        json_append(arr, row);
    }
    json_set(out, "exits", arr);

    for (k = 0; k < ne; k++) {
        free(exits[k].token);
        json_free(exits[k].ident);
    }
    free(owners);
    free(sessions);
    free(exits);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _metadata                                                       */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p2_metadata_payload(const json_t *state) {
    const char *token = ws_str(state, "token");
    const char *shell = ws_str(state, "shell");
    const char *cwd = ws_str(state, "cwd");
    int reconnected = ws_truthy(state ? json_obj_get(state, "reconnected") : NULL);
    int truncated = ws_truthy(state ? json_obj_get(state, "truncated") : NULL);
    double ttl = W34_WEBHOSTTER_A3F534_RETENTION_SECONDS;
    wsbuf b;
    json_t *out;

    ws_num(state, "ttl", &ttl);
    wsb_init(&b);
    wsb_put(&b, WS_META_PREFIX, WS_META_PREFIX_N);
    wsb_puts(&b, "{\"shell\":");
    wsb_jstr(&b, shell);
    wsb_puts(&b, ",\"terminalId\":");
    wsb_jstr(&b, token);
    wsb_puts(&b, ",\"cwd\":");
    wsb_jstr(&b, cwd);
    wsb_puts(&b, ",\"reconnected\":");
    wsb_puts(&b, reconnected ? "true" : "false");
    wsb_puts(&b, ",\"retentionSeconds\":");
    wsb_jnum(&b, ttl);
    wsb_puts(&b, ",\"truncated\":");
    wsb_puts(&b, truncated ? "true" : "false");
    wsb_putc(&b, '}');

    out = json_object();
    json_set(out, "bytes", ws_bytes_json(b.p ? b.p : "", b.n));
    free(b.p);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _send_closed                                                    */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p2_closed_payload(const json_t *state) {
    const char *token = ws_str(state, "token");
    wsbuf b;
    json_t *out, *codes;

    wsb_init(&b);
    wsb_put(&b, WS_META_PREFIX, WS_META_PREFIX_N);
    /* json.dumps without separators: ", " and ": ". */
    wsb_puts(&b, "{\"terminalId\": ");
    wsb_jstr(&b, token);
    wsb_puts(&b, ", \"closed\": true}");

    out = json_object();
    json_set(out, "bytes", ws_bytes_json(b.p ? b.p : "", b.n));
    codes = json_array();
    json_append(codes, json_number(1000.0)); /* await ws.close(code=1000) */
    json_set(out, "codes", codes);
    free(b.p);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _frame_payload / _frame_cost                                    */
/* ------------------------------------------------------------------ */

static void ws_frame_bytes(const json_t *msg, wsbuf *b) {
    const json_t *raw;
    const json_t *text;
    size_t i, n;
    if (!msg || msg->type != JSON_OBJECT) return;
    raw = json_obj_get(msg, "bytes");
    if (raw && raw->type != JSON_NULL) {
        if (raw->type == JSON_ARRAY) {
            n = json_len(raw);
            for (i = 0; i < n; i++) {
                const json_t *e = json_get(raw, i);
                unsigned char c = 0;
                if (e && e->type == JSON_NUMBER)
                    c = (unsigned char)((long long)e->num_val & 0xFF);
                wsb_put(b, (const char *)&c, 1);
            }
        } else if (raw->type == JSON_STRING) {
            wsb_puts(b, raw->str_val ? raw->str_val : "");
        }
        return;
    }
    /* (msg.get("text") or "").encode("utf-8") */
    text = json_obj_get(msg, "text");
    if (text && text->type == JSON_STRING) wsb_puts(b, text->str_val ? text->str_val : "");
}

json_t *w34_webhostter_a3f534_p2_frame_payload(const json_t *state) {
    const json_t *msg = state && state->type == JSON_OBJECT ? json_obj_get(state, "msg") : NULL;
    wsbuf b;
    json_t *out;
    wsb_init(&b);
    ws_frame_bytes(msg, &b);
    out = json_object();
    json_set(out, "bytes", ws_bytes_json(b.p ? b.p : "", b.n));
    free(b.p);
    return out;
}

json_t *w34_webhostter_a3f534_p2_frame_cost(const json_t *state) {
    const json_t *item = state && state->type == JSON_OBJECT ? json_obj_get(state, "item") : NULL;
    wsbuf b;
    double cost;
    /* "the reader's last item: None or the error that ended reading" */
    if (!item || item->type != JSON_OBJECT)
        return json_number((double)W34_WEBHOSTTER_A3F534_FRAME_OVERHEAD_BYTES);
    wsb_init(&b);
    ws_frame_bytes(item, &b);
    cost = (double)b.n + (double)W34_WEBHOSTTER_A3F534_FRAME_OVERHEAD_BYTES;
    free(b.p);
    return json_number(cost);
}

/* ------------------------------------------------------------------ */
/* p2: _InputQueue.full                                                */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p2_queue_full(const json_t *state) {
    double frames = 0.0, queued = 0.0;
    ws_num(state, "frames", &frames);
    ws_num(state, "queued", &queued);
    return json_bool(frames >= (double)W34_WEBHOSTTER_A3F534_INPUT_QUEUE_FRAMES &&
                     queued >= (double)W34_WEBHOSTTER_A3F534_INPUT_QUEUE_BYTES);
}

/* ------------------------------------------------------------------ */
/* p2: _input_runs                                                     */
/* ------------------------------------------------------------------ */

/* _RESIZE_RE.fullmatch(rb"\x1b\[RESIZE:(\d+);(\d+)\]") */
static int ws_resize_match(const char *p, size_t n, long long *cols, long long *rows) {
    static const char kw[] = "RESIZE:";
    size_t i, k;
    long long c = 0, r = 0;

    if (n < 5 || (unsigned char)p[0] != 0x1B || p[1] != '[') return 0;
    i = 2;
    for (k = 0; kw[k]; k++, i++)
        if (i >= n || p[i] != kw[k]) return 0;
    if (i >= n || p[i] < '0' || p[i] > '9') return 0;
    while (i < n && p[i] >= '0' && p[i] <= '9') {
        if (c < 1000000000000000LL) c = c * 10 + (p[i] - '0');
        i++;
    }
    if (i >= n || p[i] != ';') return 0;
    i++;
    if (i >= n || p[i] < '0' || p[i] > '9') return 0;
    while (i < n && p[i] >= '0' && p[i] <= '9') {
        if (r < 1000000000000000LL) r = r * 10 + (p[i] - '0');
        i++;
    }
    if (i >= n || p[i] != ']') return 0;
    i++;
    if (i != n) return 0;
    *cols = c;
    *rows = r;
    return 1;
}

json_t *w34_webhostter_a3f534_p2_input_runs(const json_t *state) {
    const json_t *frames = state && state->type == JSON_OBJECT ? json_obj_get(state, "frames") : NULL;
    json_t *out = json_array();
    wsbuf run;
    size_t i, n = 0;

    wsb_init(&run);
    if (frames && frames->type == JSON_ARRAY) n = json_len(frames);
    for (i = 0; i < n; i++) {
        wsbuf raw;
        long long cols = 0, rows = 0;
        wsb_init(&raw);
        ws_frame_bytes(json_get(frames, i), &raw);
        if (raw.n == 0) { /* an empty frame contributes nothing and splits nothing */
            free(raw.p);
            continue;
        }
        if (ws_resize_match(raw.p, raw.n, &cols, &rows)) {
            if (run.n > 0) {
                json_t *item = json_object();
                json_set(item, "kind", json_string("bytes"));
                json_set(item, "data", ws_bytes_json(run.p, run.n));
                json_append(out, item);
                run.n = 0;
            }
            {
                json_t *item = json_object();
                json_set(item, "kind", json_string("resize"));
                json_set(item, "cols", json_number((double)cols));
                json_set(item, "rows", json_number((double)rows));
                json_append(out, item);
            }
            free(raw.p);
            continue;
        }
        wsb_put(&run, raw.p, raw.n);
        free(raw.p);
    }
    if (run.n > 0) {
        json_t *item = json_object();
        json_set(item, "kind", json_string("bytes"));
        json_set(item, "data", ws_bytes_json(run.p, run.n));
        json_append(out, item);
    }
    free(run.p);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _resize undo bookkeeping                                        */
/* ------------------------------------------------------------------ */

static int ws_pair(const json_t *v, long long out[2]) {
    const json_t *a, *b;
    if (!v || v->type != JSON_ARRAY || json_len(v) != 2) return 0;
    a = json_get(v, 0);
    b = json_get(v, 1);
    if (!a || !b || a->type != JSON_NUMBER || b->type != JSON_NUMBER) return 0;
    out[0] = (long long)a->num_val;
    out[1] = (long long)b->num_val;
    return 1;
}

json_t *w34_webhostter_a3f534_p2_resize_undo(const json_t *state) {
    long long size[2] = {0, 0}, before[2] = {0, 0}, undo_size[2] = {0, 0};
    int has_before = ws_pair(state ? json_obj_get(state, "before") : NULL, before);
    const json_t *undo = state ? json_obj_get(state, "undo") : NULL;
    const json_t *undo_at = undo && undo->type == JSON_ARRAY ? json_get(undo, 0) : NULL;
    const json_t *undo_out = undo && undo->type == JSON_ARRAY ? json_get(undo, 1) : NULL;
    int has_undo = ws_pair(undo_at, undo_size);
    double outputs = 0.0;
    json_t *out, *arr;

    ws_pair(state ? json_obj_get(state, "size") : NULL, size);
    ws_num(state, "outputs", &outputs);
    out = json_object();

    /* if undo is not None and undo[0] == size: wait for the program's answer */
    if (has_undo && undo_size[0] == size[0] && undo_size[1] == size[1] &&
        undo_out && undo_out->type == JSON_NUMBER) {
        json_set(out, "waited_for", json_number(undo_out->num_val));
    } else {
        json_set(out, "waited_for", json_null());
    }
    arr = json_array();
    json_append(arr, json_number((double)size[0]));
    json_append(arr, json_number((double)size[1]));
    json_set(out, "resize", arr);

    /* (before, session.outputs) if before is not None and before != size */
    if (has_before && (before[0] != size[0] || before[1] != size[1])) {
        json_t *pair = json_array();
        json_t *row = json_array();
        json_append(pair, json_number((double)before[0]));
        json_append(pair, json_number((double)before[1]));
        json_append(row, pair);
        json_append(row, json_number(outputs));
        json_set(out, "undo", row);
    } else {
        json_set(out, "undo", json_null());
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _admit                                                          */
/* ------------------------------------------------------------------ */

static json_t *ws_closed_bytes(const char *token) {
    wsbuf b;
    json_t *bytes;
    wsb_init(&b);
    wsb_put(&b, WS_META_PREFIX, WS_META_PREFIX_N);
    wsb_puts(&b, "{\"terminalId\": ");
    wsb_jstr(&b, token);
    wsb_puts(&b, ", \"closed\": true}");
    bytes = ws_bytes_json(b.p ? b.p : "", b.n);
    free(b.p);
    return bytes;
}

json_t *w34_webhostter_a3f534_p2_admit_verdict(const json_t *state) {
    const char *token = ws_str(state, "attach");
    const char *action = ws_str(state, "action");
    const json_t *identity = state ? json_obj_get(state, "identity") : NULL;
    const json_t *sessions = state ? json_obj_get(state, "sessions") : NULL;
    const char *user = ws_str(identity, "user_id");
    const char *provider = ws_str(identity, "provider");
    int closing = action && strcmp(action, "close") == 0;
    int in_sessions = 0;
    size_t i, n = 0;
    json_t *out = json_object();
    json_t *removed = json_array();
    json_t *sent = json_array();
    json_t *codes = json_array();
    const char *result;

    json_set(out, "removed", removed);
    json_set(out, "sent", sent);
    json_set(out, "codes", codes);

    if (sessions && sessions->type == JSON_ARRAY) {
        n = json_len(sessions);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(sessions, i);
            if (token && e && e->type == JSON_STRING && e->str_val &&
                strcmp(e->str_val, token) == 0) {
                in_sessions = 1;
                break;
            }
        }
    }

    /* identity, then the action shape, then the token shape: _request_identity
     * raises TerminalDenied before any token is looked at. */
    if (!user || !user[0] || !provider || !provider[0]) {
        result = "denied";
    } else if (action && !closing) {
        result = "denied";
    } else if (closing && token == NULL) {
        result = "denied";
    } else if (token && !ws_token_ok(token)) {
        result = "expired";
    } else if (closing && !in_sessions) {
        /* Missing is idempotent, but never bypass auth/host policy. */
        result = "none";
        json_append(sent, ws_closed_bytes(token));
        json_append(codes, json_number(1000.0));
    } else if (token == NULL) {
        result = "created";
    } else if (closing) {
        result = "none";
        json_append(removed, json_string(token));
        json_append(sent, ws_closed_bytes(token));
        json_append(codes, json_number(1000.0));
    } else {
        result = "resolved";
    }
    json_set(out, "result", json_string(result));
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: TOKEN_RE.fullmatch                                              */
/* ------------------------------------------------------------------ */

json_t *w34_webhostter_a3f534_p2_token_ok(const json_t *state) {
    const json_t *token = state && state->type == JSON_OBJECT ? json_obj_get(state, "token") : NULL;
    if (!token || token->type != JSON_STRING) return json_bool(false);
    return json_bool(ws_token_ok(token->str_val) ? true : false);
}
