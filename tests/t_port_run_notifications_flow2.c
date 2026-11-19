/*
 * t_port_run_notifications_flow2.c — oracle harness for the
 * gateway/run_notifications.py UNCLAIMED-remainder FLOW cores ported in
 * src/gateway/run_notifications_flow2.c.
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
 * "state": ...}}; tests/oracle_run_notifications_flow2.py drives the LIVE
 * Python with the same scripts and diffs the ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/run_notifications_flow2.c"

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
/* state echo for the mutating completion flows: evt + warned (when created). */
static json_t *completion_state(json_t *selfc, json_t *evtc) {
    json_t *st = json_object();
    json_set(st, "evt", evtc);
    json_t *warned = json_obj_get(selfc, "_completion_no_transport_warned");
    if (warned && warned->type == JSON_ARRAY) {
        json_t *w = json_copy(warned);
        if (w->c.count > 1)
            qsort(w->c.items, w->c.count, sizeof(json_t *), rn2_warned_cmp);
        json_set(st, "warned", w);
    }
    return st;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    rn2_seams_t s = { &ctx, script_call };
    json_t *r = NULL;
    json_t *state = NULL;
    const json_t *self = node(in, "self");

    if (strcmp(func, "redacted_output_tail") == 0) {
        r = rn2_redacted_output_tail(self, node(in, "session"),
                                     (long long)json_get_num(in, "limit", 1000), &s);
    } else if (strcmp(func, "build_process_completion_event") == 0) {
        r = rn2_build_process_completion_event(node(in, "watcher"), node(in, "session"),
                                               json_get_str(in, "session_id", ""), &s);
    } else if (strcmp(func, "format_process_final_message") == 0) {
        r = rn2_format_process_final_message(self, json_get_str(in, "session_id", ""),
                                             node(in, "session"),
                                             json_get_str(in, "notify_mode", ""), &s);
    } else if (strcmp(func, "format_process_running_message") == 0) {
        r = rn2_format_process_running_message(self, node(in, "session"), &s);
    } else if (strcmp(func, "format_coalesced_process_completions") == 0) {
        r = rn2_format_coalesced_process_completions(node(in, "entries"), &s);
    } else if (strcmp(func, "launching_turn_active") == 0) {
        r = rn2_launching_turn_active(self, json_get_str(in, "platform_name", ""),
                                      node(in, "watcher"), &s);
    } else if (strcmp(func, "send_watcher_message") == 0) {
        r = rn2_send_watcher_message(self, json_get_str(in, "platform_name", ""),
                                     node(in, "chat_id"), node(in, "thread_id"),
                                     json_get_str(in, "message_text", ""),
                                     node(in, "watcher"), &s);
    } else if (strcmp(func, "each_secondary_ledger") == 0) {
        r = rn2_each_secondary_ledger(self, node(in, "profile_homes"),
                                      json_get_str(in, "verb", ""), "ledger.fn", NULL, &s);
    } else if (strcmp(func, "restore_secondary_completion_ledgers") == 0) {
        r = rn2_restore_secondary_completion_ledgers(self, node(in, "profile_homes"), &s);
    } else if (strcmp(func, "sweep_orphaned_completion_ledgers") == 0) {
        r = rn2_sweep_orphaned_completion_ledgers(self, &s);
    } else if (strcmp(func, "classify_completion_target") == 0) {
        r = rn2_classify_completion_target(self, json_get_str(in, "parent_session_id", ""),
                                           json_get_str(in, "session_key", ""), &s);
    } else if (strcmp(func, "classify_completion_owner") == 0) {
        r = rn2_classify_completion_owner(self, json_get_str(in, "parent_session_id", ""),
                                          json_get_str(in, "session_key", ""), &s);
    } else if (strcmp(func, "completion_delivery_ready") == 0) {
        json_t *selfc = copy_node(in, "self");
        json_t *evtc = copy_node(in, "evt");
        r = rn2_completion_delivery_ready(selfc, evtc, &s);
        state = completion_state(selfc, evtc);
        json_free(selfc);
    } else if (strcmp(func, "preflight_completion_delivery") == 0) {
        json_t *selfc = copy_node(in, "self");
        json_t *evtc = copy_node(in, "evt");
        r = rn2_preflight_completion_delivery(selfc, evtc, &s);
        state = completion_state(selfc, evtc);
        json_free(selfc);
    } else if (strcmp(func, "resolve_injection_adapter") == 0) {
        r = rn2_resolve_injection_adapter(self, json_get_str(in, "platform_name", ""),
                                          node(in, "source"), &s);
    } else if (strcmp(func, "served_api_server_wake_profile") == 0) {
        r = rn2_served_api_server_wake_profile(self, node(in, "evt"),
                                               json_get_str(in, "raw_sid", ""), &s);
    } else if (strcmp(func, "adapter_by_platform_value") == 0) {
        r = rn2_adapter_by_platform_value(self, json_get_str(in, "platform_name", ""));
    } else if (strcmp(func, "home_channel_transports") == 0) {
        r = rn2_home_channel_transports(self, &s);
    } else if (strcmp(func, "served_home_channel_configs") == 0) {
        r = rn2_served_home_channel_configs(self);
    } else if (strcmp(func, "served_home_channel_transports") == 0) {
        r = rn2_served_home_channel_transports(self, &s);
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