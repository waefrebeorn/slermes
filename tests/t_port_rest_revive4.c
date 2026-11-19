/* t_port_rest_revive4.c — harness for the rest group-4 deep revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (parsed JSON; {"__raise__": ...} on exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the input object minus "io" (these ports do not mutate inputs)
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The fixed stub signatures take their Python arguments from the serialized
 * work object (char* params) and the module-static context slots installed
 * below. Exceptions ride the per-file pending-raise channels; BaseException
 * kinds pass through every except-Exception site inside the ports.
 *
 * Seam args are OWNED by the port (freed after the call); the dispatcher
 * borrows them for the trace copy. Seam results are NEW nodes owned by the
 * port.
 *
 * Gate (explicit objs — the harness #includes both port sources, which are
 * libjson-only, so only libjson is linked):
 *   bash tests/run_one_oracle.sh rest_revive4 oracle_rest_revive4.py "" "lib/libjson/json.o"
 */
#include "gateway/platforms/api_server_gaps.c"
#include "gateway/hosted_room_discussion_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── scripted seam machinery (shared by both port io channels) ── */
static json_t *g_r4_io_map;   /* borrowed in.io */
static json_t *g_r4_cursors;  /* owned per-seam queue cursors */
static json_t *g_r4_trace;    /* owned ordered event array */

static json_t *r4_raise_spec(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *r4_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT &&
        json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT)
            return r4_raise_spec(json_get_str(raise, "type", "RuntimeError"),
                                 json_get_str(raise, "message", ""));
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *r4_script_next(const char *name) {
    json_t *entry = g_r4_io_map ? json_obj_get(g_r4_io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return r4_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(g_r4_cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return r4_raise_spec("RuntimeError", msg);
        }
        json_set(g_r4_cursors, name, json_number(cur + 1));
        return r4_entry_result(json_get(entry, idx));
    }
    return r4_entry_result(entry);
}

/* Records {ev, args, ret} in order; args borrowed (port frees), ret NEW. */
static json_t *r4_dispatch(void *io, const char *name, json_t *args) {
    (void)io;
    json_t *ret = r4_script_next(name);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", json_copy(ret));
    json_append(g_r4_trace, ev);
    return ret;
}

static asg_io_t g_asg_chan = { r4_dispatch };
static hrd_io_t g_hrd_chan = { r4_dispatch };

static json_t *r4_take_pending(void) {
    json_t **slots[] = { &g_asg_pending_raise, &g_hrd_pending_raise };
    for (size_t i = 0; i < sizeof slots / sizeof slots[0]; i++) {
        if (*slots[i]) {
            json_t *r = *slots[i];
            *slots[i] = NULL;
            return r;
        }
    }
    return NULL;
}

/* Collect the port result: pending raise wins; else parse the JSON text. */
static json_t *r4_collect(char *s) {
    json_t *pending = r4_take_pending();
    if (pending) { free(s); return pending; }
    if (!s) return json_null();
    json_t *r = json_parse(s, NULL);
    free(s);
    return r ? r : json_null();
}

static char *r4_ser(const json_t *n) {
    if (!n) return strdup("null");
    char *s = json_serialize(n);
    return s ? s : strdup("null");
}

/* ── per-case wrappers ── */
typedef json_t *(*r4_fn_t)(json_t *work);

#define R4_WRAP_VOID(NAME, FN) \
    static json_t *wrap_##NAME(json_t *work) { \
        (void)work; \
        return r4_collect(FN()); \
    }

R4_WRAP_VOID(declared_conversation_session, declared_conversation_session)
R4_WRAP_VOID(run_idempotency_scope, run_idempotency_scope)
R4_WRAP_VOID(room_grant_token, room_grant_token)
R4_WRAP_VOID(room_grant_secret, room_grant_secret)
R4_WRAP_VOID(room_grant_claims, room_grant_claims)
R4_WRAP_VOID(normalize_room_dispatch, normalize_room_dispatch)
R4_WRAP_VOID(durable_run_status, durable_run_status)
R4_WRAP_VOID(handle_room_member_capabilities, handle_room_member_capabilities)
R4_WRAP_VOID(handle_room_member_grant_revoke, handle_room_member_grant_revoke)
R4_WRAP_VOID(handle_room_member_grant_refresh, handle_room_member_grant_refresh)
R4_WRAP_VOID(handle_room_member_invitation, handle_room_member_invitation)

#define R4_WRAP_WORK(NAME, FN) \
    static json_t *wrap_##NAME(json_t *work) { \
        char *s = r4_ser(work); \
        json_t *r = r4_collect(FN(s)); \
        free(s); \
        return r; \
    }

R4_WRAP_WORK(resolve_mentions, resolve_mentions)
R4_WRAP_WORK(unaddressed_member_mentions, unaddressed_member_mentions)
R4_WRAP_WORK(derive_member_watermarks, derive_member_watermarks)
R4_WRAP_WORK(make_task_plan, make_task_plan)
R4_WRAP_WORK(plan_next_task, plan_next_task)
R4_WRAP_WORK(reconstruct_task_plan, reconstruct_task_plan)
R4_WRAP_WORK(plan_publication, plan_publication)

static json_t *wrap_validate_member_target(json_t *work) {
    char *v = r4_ser(json_obj_get(work, "value"));
    const char *profile = json_get_str(work, "profile", "");
    json_t *r = r4_collect(validate_member_target(v, profile));
    free(v);
    return r;
}

static json_t *wrap_member_by_id(json_t *work) {
    char *room = r4_ser(json_obj_get(work, "room"));
    char *mid = r4_ser(json_obj_get(work, "member_id"));
    json_t *r = r4_collect(member_by_id(room, mid));
    free(room); free(mid);
    return r;
}

typedef struct { const char *func; r4_fn_t fn; } r4_case_t;
static const r4_case_t R4_CASES[] = {
    { "declared_conversation_session", wrap_declared_conversation_session },
    { "run_idempotency_scope", wrap_run_idempotency_scope },
    { "room_grant_token", wrap_room_grant_token },
    { "room_grant_secret", wrap_room_grant_secret },
    { "room_grant_claims", wrap_room_grant_claims },
    { "normalize_room_dispatch", wrap_normalize_room_dispatch },
    { "durable_run_status", wrap_durable_run_status },
    { "handle_room_member_capabilities", wrap_handle_room_member_capabilities },
    { "handle_room_member_grant_revoke", wrap_handle_room_member_grant_revoke },
    { "handle_room_member_grant_refresh", wrap_handle_room_member_grant_refresh },
    { "handle_room_member_invitation", wrap_handle_room_member_invitation },
    { "validate_member_target", wrap_validate_member_target },
    { "resolve_mentions", wrap_resolve_mentions },
    { "unaddressed_member_mentions", wrap_unaddressed_member_mentions },
    { "member_by_id", wrap_member_by_id },
    { "derive_member_watermarks", wrap_derive_member_watermarks },
    { "make_task_plan", wrap_make_task_plan },
    { "plan_next_task", wrap_plan_next_task },
    { "reconstruct_task_plan", wrap_reconstruct_task_plan },
    { "plan_publication", wrap_plan_publication },
};
#define R4_CASE_COUNT (sizeof(R4_CASES) / sizeof(R4_CASES[0]))

static char *r4_read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = r4_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const r4_case_t *tc = NULL;
        for (size_t i = 0; i < R4_CASE_COUNT; i++)
            if (strcmp(R4_CASES[i].func, func) == 0) { tc = &R4_CASES[i]; break; }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            g_r4_io_map = json_obj_get(work, "io");
            g_r4_cursors = json_object();
            g_r4_trace = json_array();
            g_asg_io = &g_asg_chan;
            g_asg_ctx = work;
            g_hrd_io = &g_hrd_chan;
            g_hrd_ctx = work;
            json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
            json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
            json_t *r = tc->fn(work);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(g_r4_trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(g_r4_trace);
            json_free(g_r4_cursors);
            g_asg_io = NULL; g_asg_ctx = NULL;
            g_hrd_io = NULL; g_hrd_ctx = NULL;
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}
