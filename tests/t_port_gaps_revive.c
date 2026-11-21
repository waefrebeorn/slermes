/*
 * t_port_gaps_revive.c — ONE leaf differential oracle harness for the
 * re-ported placeholders in src/gateway/platforms/base_gaps.c and
 * src/cli/cli_gaps.c.  Includes both .c files directly so the exact source
 * under test is compiled, wires a scripted recording seam identical to the
 * Python twin, and emits JSON-lines {"func","in","out":{...}} for
 * tests/oracle_gaps_revive.py to compare against LIVE Python truth.
 *
 * Fixture: tests/fixtures/gaps_revive.jsonl (fed on stdin by run_one_oracle.sh).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/platforms/base_gaps.c"
#include "cli/cli_gaps.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counts;
} sc_ctx_t;

static json_t *sc_call(void *ctx, const char *name, json_t *args) {
    sc_ctx_t *c = (sc_ctx_t *)ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = NULL;
    int fail_closed = 0;
    if (!script) {
        fail_closed = 1;
    } else if (script->type == JSON_OBJECT && json_get_bool(script, "__script__", false)) {
        val = json_obj_get(script, "ret");
    } else if (script->type == JSON_ARRAY) {
        double cnt = json_get_num(c->counts, name, 0);
        size_t idx = (size_t)cnt;
        if (script->c.count) {
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
        }
        json_set(c->counts, name, json_number(cnt + 1));
    } else {
        val = script;
    }
    json_t *fc = NULL;
    if (fail_closed) {
        char m[256];
        snprintf(m, sizeof m, "unscripted seam: %s", name);
        fc = json_object();
        json_set_hdr(fc, "__raise__", "RuntimeError");
        json_set_hdr(fc, "message", m);
        val = fc;
    }
    /* bm_seam()/cg2_seam() record the {ev,args,ret} event; do NOT record here. */
    if (fail_closed) return fc;
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* Adapters for the two seam function-pointer layouts: pass the embedded ctx. */
static json_t *sc_call_bm(struct bm_io *io, const char *name, json_t *args) {
    return sc_call(io->ctx, name, args);
}
static json_t *sc_call_cg2(struct cg2_io *io, const char *name, json_t *args) {
    return sc_call(io->ctx, name, args);
}

int main(void) {
    char line[65536];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) continue;
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) { json_free(rec); continue; }
        /* Preserve the ORIGINAL case input for the twin; stateful ports mutate
         * `in` in place, so echo the post-state separately in out["state"]. */
        json_t *orig_in = json_copy(in);

        sc_ctx_t ctx;
        ctx.in = in;
        ctx.trace = json_array();
        ctx.counts = json_object();
        bm_io_t bio; bio.call = sc_call_bm; bio.trace = ctx.trace; bio.ctx = &ctx;
        cg2_io_t cio; cio.call = sc_call_cg2; cio.trace = ctx.trace; cio.ctx = &ctx;

        json_t *ret = NULL;
        if (!strcmp(func, "set_owner_profile"))      ret = set_owner_profile(&bio, in);
        else if (!strcmp(func, "consume_detached_handler_exception")) ret = consume_detached_handler_exception(&bio, in);
        else if (!strcmp(func, "set_platform_event_handler")) ret = set_platform_event_handler(&bio, in);
        else if (!strcmp(func, "send_path_degraded")) ret = send_path_degraded(&bio, in);
        else if (!strcmp(func, "gateway_trust_env"))  ret = gateway_trust_env(&bio, in);
        else if (!strcmp(func, "warn_unresolved_docker_media")) ret = warn_unresolved_docker_media(&bio, in);
        else if (!strcmp(func, "plugin_blueprints"))  ret = plugin_blueprints(&cio, in);
        else if (!strcmp(func, "load_settings"))      ret = load_settings(&cio, in);
        else if (!strcmp(func, "cmd_delete"))         ret = cmd_delete(&cio, in);
        else { json_free(rec); continue; }

        json_t *out = json_object();
        json_set(out, "ret", ret ? json_copy(ret) : json_null());
        json_t *exc = NULL;
        if (ret && ret->type == JSON_OBJECT && json_obj_get(ret, "__raise__")) {
            exc = json_copy(ret);
            json_set(out, "ret", json_null());
        }
        json_set(out, "exc", exc ? exc : json_null());
        json_set(out, "trace", json_copy(ctx.trace));
        if (!strcmp(func, "cmd_delete")) {
            const json_t *cfg = json_obj_get(in, "cfg");
            json_set(out, "state", cfg ? json_copy(cfg) : json_null());
        }

        json_t *emit = json_object();
        json_set_hdr(emit, "func", func);
        json_set(emit, "in", orig_in);
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        printf("%s\n", s ? s : "{}");
        free(s);

        json_free(emit);
        json_free(ret);
        json_free(ctx.trace);
        json_free(ctx.counts);
        json_free(rec);
    }
    return 0;
}
