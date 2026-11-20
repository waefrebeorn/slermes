/*
 * t_port_run_notifications_flow3.c — oracle harness for the
 * gateway/run_notifications.py UNCLAIMED-remainder FLOW cores (wave 3) ported
 * in src/gateway/run_notifications_flow3.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves every flow: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - a plain JSON value: returned on EVERY call
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; after exhaustion the
 *     LAST element repeats; empty -> None
 *   - {"__raise__": kind, "msg": text}: returned as-is; the port detects it
 *   - a scripted JSON null: returns a NULL pointer (Python None)
 *   - ABSENT key: FAIL-CLOSED — returns {"__raise__":"RuntimeError",
 *     "msg":"no script for <name>"} (recorded as ret).
 *
 * Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...],
 * "state": ...}}; tests/oracle_run_notifications_flow3.py drives the LIVE
 * Python with the same scripts and diffs the ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/run_notifications_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *seq_pos;
} script_ctx_t;

static json_t *script_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = (script_ctx_t *)ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = script;
    if (script && script->type == JSON_OBJECT) {
        const json_t *seq = json_obj_get(script, "__seq__");
        if (seq && seq->type == JSON_ARRAY) {
            const json_t *pos = json_obj_get(c->seq_pos, name);
            size_t idx = (pos && pos->type == JSON_NUMBER) ? (size_t)pos->num_val : 0;
            size_t n = json_len(seq);
            if (n == 0) {
                val = NULL;
            } else {
                if (idx >= n) idx = n - 1;
                val = json_get(seq, idx);
                json_set(c->seq_pos, name, json_number((double)(idx + 1)));
            }
        }
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    if (!script) {
        json_t *rs = json_object();
        json_set(rs, "__raise__", json_string("RuntimeError"));
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        json_set(rs, "msg", json_string(m));
        json_set(e, "ret", json_copy(rs));
        json_append(c->trace, e);
        return rs;
    }
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static const json_t *node(const json_t *in, const char *k) {
    return json_obj_get(in, k);
}
static json_t *copy_node(const json_t *in, const char *k) {
    const json_t *v = json_obj_get(in, k);
    return v ? json_copy(v) : json_object();
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    rn3_seams_t s = { &ctx, script_call };
    json_t *r = NULL;
    json_t *state = NULL;
    const json_t *self = node(in, "self");

    if (strcmp(func, "send_queued_final_text") == 0) {
        r = rn3_send_queued_final_text(self, node(in, "adapter"), node(in, "source"),
                                       json_get_str(in, "text_content", ""),
                                       node(in, "metadata"), node(in, "event_message_id"),
                                       node(in, "session_key"), node(in, "inbound_message_id"), &s);
    } else if (strcmp(func, "send_home_channel_message") == 0) {
        r = rn3_send_home_channel_message(self, json_get_str(in, "platform", ""),
                                          node(in, "home"), node(in, "transport"),
                                          json_get_str(in, "message", ""),
                                          json_get_str(in, "failure_fmt", ""), &s);
    } else if (strcmp(func, "replay_pending_planned_restart_notification") == 0) {
        json_t *selfc = copy_node(in, "self");
        r = rn3_replay_pending_planned_restart_notification(selfc, &s);
        state = json_object();
        const json_t *lock = json_obj_get(selfc, "_planned_restart_notice_lock");
        json_set(state, "_planned_restart_notice_lock", lock ? json_copy(lock) : json_null());
        json_free(selfc);
    } else if (strcmp(func, "send_session_db_warning_notifications") == 0) {
        r = rn3_send_session_db_warning_notifications(self, &s);
    } else if (strcmp(func, "self_post_api_server") == 0) {
        r = rn3_self_post_api_server(self, node(in, "adapter"),
                                     json_get_str(in, "synth_text", ""),
                                     json_get_str(in, "raw_sid", ""), node(in, "evt"), &s);
    } else if (strcmp(func, "committed_relay_adapter") == 0) {
        r = rn3_committed_relay_adapter(self, node(in, "source"), &s);
    } else if (strcmp(func, "completion_event_scope") == 0) {
        r = rn3_completion_event_scope(self, node(in, "evt"), &s);
    } else if (strcmp(func, "served_api_server_event_scope") == 0) {
        json_t *scope = NULL;
        json_t *er = rn3_served_api_server_event_scope_enter(self, node(in, "evt"),
                                                             json_get_str(in, "raw_sid", ""),
                                                             &s, &scope);
        if (rn3_raised(er)) {
            r = er;
        } else {
            json_free(er);
            json_t *ex = rn3_served_api_server_event_scope_exit(scope, &s);
            if (rn3_raised(ex)) r = ex;
            else { json_free(ex); r = NULL; }
        }
        json_free(scope);
    } else if (strcmp(func, "deliver_completion_notification_scoped") == 0) {
        json_t *selfc = copy_node(in, "self");
        r = rn3_deliver_completion_notification_scoped(selfc, json_get_str(in, "synth_text", ""),
                                                       node(in, "evt"), node(in, "sibling_claims"), &s);
        const json_t *inflight = json_obj_get(selfc, "_completion_deliveries_inflight");
        if (inflight) {
            state = json_object();
            json_set(state, "inflight", rn3_set_sorted(inflight));
        }
        json_free(selfc);
    } else if (strcmp(func, "settle_batch_waiters") == 0) {
        json_t *entriesc = copy_node(in, "entries");
        r = rn3_settle_batch_waiters(entriesc, node(in, "result"), &s);
        state = json_object();
        json_set(state, "entries", entriesc);
    } else if (strcmp(func, "deliver_async_delegation_group") == 0) {
        r = rn3_deliver_async_delegation_group(self, node(in, "group"), &s);
    } else if (strcmp(func, "deliver_async_delegation_groups") == 0) {
        r = rn3_deliver_async_delegation_groups(self, node(in, "groups"), node(in, "pr"), &s);
    } else if (strcmp(func, "arm_process_watcher") == 0) {
        r = rn3_arm_process_watcher(self, node(in, "watcher"), &s);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state ? state : json_null());
    json_free(ctx.seq_pos);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
