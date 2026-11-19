/* t_port_runtime_provider_flow.c — harness for the runtime-provider FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
 * ("ret"), and — for the state-mutating _tag flow — the mutated runtime
 * ("state"). The echoed "in" is the ORIGINAL input (the oracle re-drives the
 * live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim), a LIST consumed in call order (a
 * queue), or {"__script__": true, "ret": <value>, "raise": {type,message,errno?}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 */
#include "hermes_json.h"
#include "port_runtime_provider_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io_map;
    json_t *cursors;
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER) json_set(e, "errno", json_copy(en));
    return e;
}

static json_t *entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(rpf_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        json_t *e = json_object();
        json_set_hdr(e, "__raise__", "RuntimeError");
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set_hdr(e, "message", msg);
        return e;
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            json_t *e = json_object();
            json_set_hdr(e, "__raise__", "RuntimeError");
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            json_set_hdr(e, "message", msg);
            return e;
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return entry_result(json_get(entry, idx));
    }
    return entry_result(entry);
}

typedef json_t *(*rpf_fn_t)(rpf_io_t *, json_t *);
typedef struct { const char *func; rpf_fn_t fn; bool stateful; } rpf_case_t;
static const rpf_case_t CASES[] = {
    { "_resolves_to_custom", rpf_resolves_to_custom, false },
    { "_cfg_provider", rpf_cfg_provider, false },
    { "_same_registered_provider", rpf_same_registered_provider, false },
    { "_config_base_url_for_provider", rpf_config_base_url_for_provider, false },
    { "_on_declared_anthropic_endpoint", rpf_on_declared_anthropic_endpoint, false },
    { "_azure_inferred_api_mode", rpf_azure_inferred_api_mode, false },
    { "_runtime", rpf_runtime, false },
    { "_creds_fallback", rpf_creds_fallback, false },
    { "_azure_anthropic_env_key", rpf_azure_anthropic_env_key, false },
    { "openai_env_key_authoritative_for", rpf_openai_env_key_authoritative_for, false },
    { "_host_gated_env_key_candidates", rpf_host_gated_env_key_candidates, false },
    { "_openrouter_should_use_pool", rpf_openrouter_should_use_pool, false },
    { "_raise_if_provider_disabled", rpf_raise_if_provider_disabled, false },
    { "_raise_for_credentialless_bare_custom", rpf_raise_for_credentialless_bare_custom, false },
    { "_tag", rpf_tag, true },
    { "_local_endpoint_bypass", rpf_local_endpoint_bypass, false },
    { "_resolve_requested_shortcuts", rpf_resolve_requested_shortcuts, false },
    { "_refresh_nous_pool_entry", rpf_refresh_nous_pool_entry, false },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
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
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const rpf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            rpf_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            if (tc->stateful) {
                json_t *state = json_object();
                json_t *rt = json_obj_get(pin, "runtime");
                json_set(state, "runtime", rt ? json_copy(rt) : json_null());
                json_set(out, "state", state);
            }
            json_free(pin);
            json_free(io.trace);
            json_free(sc.cursors);
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