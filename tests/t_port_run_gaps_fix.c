/*
 * t_port_run_gaps_fix.c — leaf differential oracle harness for the re-ported
 * placeholder bodies in src/gateway/run_gaps.c (linked as an OBJECT, not
 * #included).  The 12 "void f(void *gateway)" placeholders carry no seam
 * parameter, so this harness passes the grg_seams_t context itself as the
 * gateway pointer (the universal-seam convention used across the file).
 *
 * Reads JSON-lines cases from stdin (or argv[1]): {"func":..., "in":{...}}.
 * One recording seam serves every ported function: it looks up in.io[<name>],
 * records {"ev":name,"args":...,"ret":value} in call order, and returns the
 * scripted value.  Seams prefixed "param." inject function arguments and are
 * NOT recorded (the live Python receives those as ordinary parameters):
 *   - plain JSON value: returned every call
 *   - {"__seq__":[...]}: consumed per call, last repeats after exhaustion
 *   - {"__raise__":kind,"msg":...}: raised seam (returned as a raise marker)
 *   - absent / JSON null: NULL pointer (Python None)
 *
 * Emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}; the twin
 * tests/oracle_run_gaps_fix.py drives the LIVE Python bodies with the same
 * scripts and diffs the ordered trace.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"

/* run_gaps.o extern symbol (telegram topic normalization lives elsewhere). */
char *normalize_telegram_topic_profile_name(const char *profile_name) {
    return strdup(profile_name ? profile_name : "default");
}

/* Must mirror grg_seams_t in src/gateway/run_gaps.c exactly. */
typedef struct {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} grg_seams_t;

/* Prototypes for the re-ported bodies (all preserve the void(void*) shape). */
void start_startup_warmup(void *gateway);
void warm_turn_prerequisites(void *gateway);
void track_deferred_agent_worker(void *worker);
void start_hosted_room_worker_sync(void *gateway);
void ensure_hosted_room_worker(void *gateway);
void hosted_room_worker_watcher(void *gateway);
void stop_hosted_room_worker(void *gateway);
void model_catalog_refresh_watcher(void *gateway);
void shutdown_mcp_servers_nonblocking(void *gateway);

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
    /* Invisible argument-injection seams: do not record. */
    if (strncmp(name, "param.", 6) == 0)
        return (val && val->type != JSON_NULL) ? json_copy(val) : NULL;
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    grg_seams_t s = { &ctx, script_call };
    void *gw = &s;

    if (strcmp(func, "start_startup_warmup") == 0) {
        start_startup_warmup(gw);
    } else if (strcmp(func, "ensure_hosted_room_worker") == 0) {
        ensure_hosted_room_worker(gw);
    } else if (strcmp(func, "stop_hosted_room_worker") == 0) {
        stop_hosted_room_worker(gw);
    } else if (strcmp(func, "hosted_room_worker_watcher") == 0) {
        hosted_room_worker_watcher(gw);
    } else if (strcmp(func, "warm_turn_prerequisites") == 0) {
        warm_turn_prerequisites(gw);
    } else if (strcmp(func, "track_deferred_agent_worker") == 0) {
        track_deferred_agent_worker(gw);
    } else if (strcmp(func, "start_hosted_room_worker_sync") == 0) {
        start_hosted_room_worker_sync(gw);
    } else if (strcmp(func, "model_catalog_refresh_watcher") == 0) {
        model_catalog_refresh_watcher(gw);
    } else if (strcmp(func, "shutdown_mcp_servers_nonblocking") == 0) {
        shutdown_mcp_servers_nonblocking(gw);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", json_null());
    json_set(out, "trace", ctx.trace);
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
