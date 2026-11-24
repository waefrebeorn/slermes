/*
 * delegate_tool_continue.c - C home for upstream Hermes tools/delegate_tool_continue.py.
 * PoP: w16_delegateto_f07f8d_refusal @ tools/delegate_tool_continue.py:_refusal
 * PoP: w16_delegateto_f07f8d_claim_resume @ tools/delegate_tool_continue.py:_claim_resume
 * PoP: w16_delegateto_f07f8d_validate @ tools/delegate_tool_continue.py:continue_subagent
 * PoP: w16_delegateto_f07f8d_not_found @ tools/delegate_tool_continue.py:_NOT_FOUND
 *
 * Deterministic decision logic only. Process-global facts (worker process, spawn pause,
 * registry locks, durable DB) are injected through these `_ex`-style JSON seams; tool_error's
 * JSON envelope belongs to tools/registry.py and is not duplicated here. The oracle supplies
 * a tool_error stub returning the message itself so these ports stay comparable.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_delegateto_f07f8d.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *strip(const char *s) {
    if (!s) return dupstr("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = 0; }
    return p;
}
static json_t *fmtstr(const char *fmt, ...) {
    char b[4096]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    return json_string(b);
}
static const json_t *obj_or_null(const json_t *o) { return (o && o->type == JSON_OBJECT) ? o : NULL; }
static int present(const json_t *v) { return v && v->type != JSON_NULL; }

static const char *NOT_FOUND_TMPL =
    "No finished subagent '%s' in this conversation's spawn tree can be continued "
    "(unknown id, not yours, or no longer retained). Spawn a new task instead.";
static const char *VALIDATION_MSG =
    "action='continue' requires subagent_id (a finished child of yours) and a non-empty "
    "'message' (its next instruction).";

struct json_t *w16_delegateto_f07f8d_refusal(const struct json_t *opts) {
    if (json_get_bool(opts, "oneshot", false))
        return json_string("action='continue' is unavailable in one-shot sessions. Spawn a new task instead.");
    if (json_get_bool(opts, "worker", false) || json_get_bool(opts, "runtime_db", false))
        return json_string("action='continue' is unavailable in managed worker sessions. Spawn a new task instead.");
    if (json_get_bool(opts, "paused", false))
        return json_string("Delegation spawning is paused. Clear the pause via the TUI (`p` in /agents) "
                           "or the `delegation.pause` RPC before retrying.");
    long long depth = (long long)json_get_num(opts, "depth", 0);
    long long max_spawn = (long long)json_get_num(opts, "max_spawn", 0);
    if (depth >= max_spawn) return fmtstr("Delegation depth limit reached (depth=%lld, max_spawn_depth=%lld).", depth, max_spawn);
    return json_null();
}

struct json_t *w16_delegateto_f07f8d_claim_resume(const struct json_t *state) {
    json_t *out = json_object();
    const char *sid = json_get_str(state, "sid", "");
    const json_t *active = obj_or_null(json_obj_get(state, "active"));
    const json_t *recent = obj_or_null(json_obj_get(state, "recent"));
    int own_live = json_get_bool(state, "own_live", false);
    int own_recent = json_get_bool(state, "own_recent", false);
    const json_t *live_rec = active ? json_obj_get(active, sid) : NULL;
    const json_t *rec = recent ? json_obj_get(recent, sid) : NULL;
    if (present(live_rec) && own_live) {
        json_set(out, "spec", json_null());
        json_set(out, "error", fmtstr("Subagent '%s' is still running: use action='steer' to send it this message.", sid));
        return out;
    }
    if (!present(rec) || !own_recent || !recent || json_obj_get(recent, sid) != rec) {
        json_set(out, "spec", json_null());
        json_set(out, "error", fmtstr(NOT_FOUND_TMPL, sid));
        return out;
    }
    const char *st = json_get_str(rec, "state", "");
    if (strcmp(st, "resumable")) {
        json_set(out, "spec", json_null());
        json_set(out, "error", fmtstr("Subagent '%s' is still stopping: its previous run has not exited yet. "
                                      "Try again shortly; if this persists, spawn a new task.", sid));
        return out;
    }
    const json_t *spec = json_obj_get(rec, "resume");
    if (!present(spec)) {
        json_set(out, "spec", json_null());
        json_set(out, "error", fmtstr(NOT_FOUND_TMPL, sid));
        return out;
    }
    json_set(out, "spec", json_copy(spec));
    json_set(out, "error", json_null());
    return out;
}

struct json_t *w16_delegateto_f07f8d_validate(const struct json_t *args) {
    char *sid = strip(json_get_str(args, "subagent_id", ""));
    char *msg = strip(json_get_str(args, "message", ""));
    json_t *out = (!sid[0] || !msg[0]) ? json_string(VALIDATION_MSG) : json_null();
    free(sid); free(msg);
    return out;
}

struct json_t *w16_delegateto_f07f8d_not_found(const char *sid) {
    return fmtstr(NOT_FOUND_TMPL, sid ? sid : "");
}
