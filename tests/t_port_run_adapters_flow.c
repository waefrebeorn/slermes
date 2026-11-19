/*
 * t_port_run_adapters_flow.c — oracle harness for the gateway/run_adapters.py
 * adapter-lifecycle FLOW cores ported in src/gateway/run_adapters_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves every flow: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - a plain JSON value: returned on EVERY call (ret marker = the value)
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; after exhaustion the
 *     LAST element repeats (stable clocks / repeated probes)
 *   - {"__raise__": kind, "msg": text}: returned as-is; the port detects it
 *   - absent: NULL (Python None), ret marker null
 * A scripted JSON null returns a NULL pointer (not a json_null node), so
 * truthiness-tested seam results short-circuit exactly like Python None.
 *
 * Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...],
 * "state": ...}}; tests/oracle_run_adapters_flow.py drives the LIVE Python
 * with the same scripts and diffs the ordered trace + ret + mutated state.
 * Sets (_background_tasks / track_task_in) normalize to sorted id lists.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "run_adapters_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *seq_pos;   /* seam name -> next sequence index */
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
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static const json_t *node(const json_t *in, const char *k) {
    return json_obj_get(in, k);
}
static bool in_bool(const json_t *in, const char *k) {
    return json_get_bool(in, k, false);
}
static json_t *self_copy(const json_t *in) {
    const json_t *s = node(in, "self");
    return s ? json_copy(s) : json_object();
}
static int cmp_str_item(const void *a, const void *b) {
    const json_t *x = *(const json_t *const *)a;
    const json_t *y = *(const json_t *const *)b;
    const char *xs = (x && x->type == JSON_STRING && x->str_val) ? x->str_val : "";
    const char *ys = (y && y->type == JSON_STRING && y->str_val) ? y->str_val : "";
    return strcmp(xs, ys);
}
static void sort_str_array(json_t *arr) {
    if (arr && arr->type == JSON_ARRAY && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), cmp_str_item);
}
/* normalize the tagged set under key to a sorted id list (in place). */
static void norm_set_tag(json_t *obj, const char *key) {
    json_t *tag = obj ? json_obj_get(obj, key) : NULL;
    if (tag && tag->type == JSON_OBJECT) {
        json_t *items = json_obj_get(tag, "set");
        if (items && items->type == JSON_ARRAY) sort_str_array(items);
    }
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    raf_seams_t s = { &ctx, script_call };
    json_t *r = NULL;
    json_t *state = NULL;
    const json_t *platform = node(in, "platform");
    const json_t *adapter = node(in, "adapter");

    if (strcmp(func, "env_timeout_override") == 0) {
        r = raf_env_timeout_override(json_get_str(in, "name", ""), &s);
    } else if (strcmp(func, "wait_or_detach") == 0) {
        r = raf_wait_or_detach(json_get_str(in, "task", ""), json_get_num(in, "timeout", 0.0), &s);
    } else if (strcmp(func, "connect_adapter_bounded") == 0) {
        r = raf_connect_adapter_bounded(node(in, "self"), adapter, platform,
                                        in_bool(in, "is_reconnect"), in_bool(in, "initial"), &s);
    } else if (strcmp(func, "connect_adapter_with_timeout") == 0) {
        r = raf_connect_adapter_with_timeout(node(in, "self"), adapter, platform,
                                             in_bool(in, "is_reconnect"), in_bool(in, "initial"), &s);
    } else if (strcmp(func, "extend_systemd_boot_for") == 0) {
        r = raf_extend_systemd_boot_for(node(in, "self"), platform, &s);
    } else if (strcmp(func, "handle_reaction_event") == 0) {
        r = raf_handle_reaction_event(node(in, "self"), node(in, "ctx"), &s);
    } else if (strcmp(func, "on_all_adapters_down") == 0) {
        r = raf_on_all_adapters_down(node(in, "self"));
    } else if (strcmp(func, "adapter_listener_claim") == 0) {
        r = raf_adapter_listener_claim(platform, adapter);
    } else if (strcmp(func, "adapter_credential_claim") == 0) {
        r = raf_adapter_credential_claim(platform, adapter, &s);
    } else if (strcmp(func, "reconnect_queue_entry") == 0) {
        r = raf_reconnect_queue_entry(node(in, "self"), platform, adapter,
                                      node(in, "platform_config"),
                                      json_get_num(in, "attempts", 0.0),
                                      json_get_num(in, "delay", 0.0),
                                      json_get_bool(in, "queued", true), &s);
    } else if (strcmp(func, "queue_retryable_best_effort") == 0) {
        r = raf_queue_retryable_best_effort(node(in, "self"), adapter,
                                            json_get_str(in, "why", ""), &s);
    } else if (strcmp(func, "retain_background_task") == 0) {
        json_t *self = self_copy(in);
        r = raf_retain_background_task(self, json_get_str(in, "task", ""), &s);
        norm_set_tag(self, "_background_tasks");
        state = self;
    } else if (strcmp(func, "track_task_in") == 0) {
        const json_t *t = node(in, "tasks");
        json_t *tasks = t ? json_copy(t) : json_array();
        r = raf_track_task_in(tasks, json_get_str(in, "task", ""), &s);
        sort_str_array(tasks);
        state = json_object();
        json_set(state, "tasks", tasks);
    } else if (strcmp(func, "request_clean_exit") == 0) {
        json_t *self = self_copy(in);
        r = raf_request_clean_exit(self, json_get_str(in, "reason", ""), &s);
        state = self;
    } else if (strcmp(func, "flag_reconnect_needs_attention") == 0) {
        const json_t *i = node(in, "info");
        json_t *info = i ? json_copy(i) : json_object();
        r = raf_flag_reconnect_needs_attention(node(in, "self"), platform, info,
                                               json_get_num(in, "now", 0.0),
                                               json_get_str(in, "status_key", NULL), &s);
        state = json_object();
        json_set(state, "info", info);
    } else if (strcmp(func, "bump_reconnect_backoff") == 0) {
        const json_t *i = node(in, "info");
        json_t *info = i ? json_copy(i) : json_object();
        r = raf_bump_reconnect_backoff(node(in, "self"), platform, info,
                                       json_get_num(in, "attempt", 0.0),
                                       node(in, "error_code"), node(in, "error_message"), &s);
        state = json_object();
        json_set(state, "info", info);
    } else if (strcmp(func, "adapter_may_heal") == 0) {
        r = raf_adapter_may_heal(node(in, "self"), platform, node(in, "platform_config"),
                                 node(in, "_BUILTIN_ADAPTERS"), &s);
    } else if (strcmp(func, "reconnect_failed_platform") == 0) {
        json_t *self = self_copy(in);
        r = raf_reconnect_failed_platform(self, platform, json_get_num(in, "now", 0.0),
                                          node(in, "_BUILTIN_ADAPTERS"), &s);
        state = self;
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
