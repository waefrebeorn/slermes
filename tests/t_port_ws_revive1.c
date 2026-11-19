/* t_port_ws_revive1.c — differential harness for the ws_revive1 wave.
 *
 * Reads one JSON case per stdin line: {"func": "<c_name>", "in": {...}}.
 * Emits one JSON line per case: {"func", "in", "out", "trace", "st"}:
 *   out   — the port's return (dict/list/array-tuple/bool/null) or a raise
 *           spec {"__raise__": cls, "msg": str(exc)} for propagated exceptions.
 *   trace — ordered seam events {"ev", "args", "ret"} (ret is the raw
 *           scripted value; a scripted null records null and returns a
 *           NULL pointer to the port).
 *   st    — post-call state ({} for stateless ports; the module-global
 *           dicts for maybe_auto_archive_for_profile / spawn_gateway_restart).
 * The oracle (tests/oracle_ws_revive1.py) recomputes the live Python
 * reference from the echoed "in" with identical scripted seams.
 *
 * Seam scripts ("in".io): a bare JSON array is a per-call queue; any
 * other value is a constant return; {"__script__": true, "ret": X}
 * scripts a constant X (required for list-valued returns). {"__raise__":
 * cls, "msg": m} scripts an exception. Missing/exhausted scripts fail
 * closed with RuntimeError. The seam OWNS args — the port never frees them.
 *
 * Gate (code-only wave; run later from the repo root, explicit objs —
 * the harness #includes the port .c, so libjson is the only object):
 *
 *   bash tests/run_one_oracle.sh ws_revive1 oracle_ws_revive1.py "" \
 *        "lib/libjson/json.o"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* Area-qualified include: -I src resolves this to the port file. */
#include "hermes_cli/ws_revive1.c"

/* ---- scripted seam recorder ---- */

typedef struct {
    const json_t *io;      /* borrowed per-case script table */
    json_t *trace;         /* owned ordered event array */
    json_t *counts;        /* owned per-name queue counters */
} w1_ctx;

static json_t *w1_raise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg));
    return o;
}

static json_t *w1_seam(void *vctx, const char *name, const json_t *args)
{
    w1_ctx *c = (w1_ctx *)vctx;
    json_t *script = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *ret = NULL;
    if (!script) {
        char msg[160];
        snprintf(msg, sizeof msg, "unscripted seam: %s", name);
        ret = w1_raise("RuntimeError", msg);
    } else if (script->type == JSON_ARRAY) {
        json_t *cnt = json_obj_get(c->counts, name);
        size_t idx = cnt ? (size_t)cnt->num_val : 0;
        if (idx >= script->c.count) {
            char msg[160];
            snprintf(msg, sizeof msg, "script exhausted: %s", name);
            ret = w1_raise("RuntimeError", msg);
        } else {
            ret = json_copy(script->c.items[idx]);
            json_set(c->counts, name, json_number((double)(idx + 1)));
        }
    } else if (script->type == JSON_OBJECT && json_obj_get(script, "__script__")
               && json_obj_get(script, "__script__")->type == JSON_BOOL
               && json_obj_get(script, "__script__")->bool_val) {
        json_t *r = json_obj_get(script, "ret");
        ret = r ? json_copy(r) : NULL;
    } else {
        ret = json_copy(script);
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_array());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(c->trace, ev);
    /* scripted JSON null must reach the port as a NULL pointer */
    if (ret && ret->type == JSON_NULL) {
        json_free(ret);
        return NULL;
    }
    return ret;
}

/* ---- case I/O ---- */

static char *w1_read_line(FILE *f)
{
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 2 > cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = 0;
    return buf;
}

static void w1_emit(const char *func, const json_t *in, json_t *out,
                    json_t *trace, json_t *st)
{
    json_t *env = json_object();
    json_set(env, "func", json_string(func));
    json_set(env, "in", in ? json_copy(in) : json_null());
    json_set(env, "out", out ? out : json_null());
    json_set(env, "trace", trace ? trace : json_array());
    json_set(env, "st", st ? st : json_object());
    char *s = json_serialize(env);
    printf("%s\n", s);
    free(s);
    json_free(env);
}

static json_t *w1_state_seed(const json_t *in, const char *key, int as_array)
{
    json_t *state = in ? json_obj_get(in, "state") : NULL;
    json_t *v = state ? json_obj_get(state, key) : NULL;
    if (v) return json_copy(v);
    return as_array ? json_null() : json_object();
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = w1_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) {
            fprintf(stderr, "ws_revive1 harness: bad case line\n");
            return 2;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        w1_ctx ctx = { in ? json_obj_get(in, "io") : NULL,
                       json_array(), json_object() };
        ws1_seams_install(w1_seam, &ctx);
        json_t *out = NULL;
        json_t *st = json_object();
        if (strcmp(func, "ws_u_maybe_auto_archive_for_profile") == 0) {
            json_set(st, "last_auto_archive_check",
                     w1_state_seed(in, "last_auto_archive_check", 0));
            ws1_state_install(st);
        } else if (strcmp(func, "ws_u_spawn_gateway_restart") == 0) {
            json_set(st, "last_gateway_restart",
                     w1_state_seed(in, "last_gateway_restart", 1));
            json_set(st, "action_procs", w1_state_seed(in, "action_procs", 0));
            json_set(st, "action_commands", w1_state_seed(in, "action_commands", 0));
            ws1_state_install(st);
        } else {
            ws1_state_install(NULL);
        }

        if (strcmp(func, "ws_u_normalize_memory_provider_schema") == 0) {
            out = ws_u_normalize_memory_provider_schema(in);
        } else if (strcmp(func, "ws_u_read_memory_provider_existing_values") == 0) {
            out = ws_u_read_memory_provider_existing_values(in);
        } else if (strcmp(func, "ws_u_discover_memory_provider_statuses") == 0) {
            out = ws_u_discover_memory_provider_statuses(in);
        } else if (strcmp(func, "ws_u_profile_platform_ports") == 0) {
            out = ws_u_profile_platform_ports(in);
        } else if (strcmp(func, "ws_u_custom_provider_options") == 0) {
            out = ws_u_custom_provider_options(in);
        } else if (strcmp(func, "ws_u_schema_with_dynamic_provider_options") == 0) {
            out = ws_u_schema_with_dynamic_provider_options(in);
        } else if (strcmp(func, "ws_u_normalize_mcp_server_create") == 0) {
            out = ws_u_normalize_mcp_server_create(in);
        } else if (strcmp(func, "ws_u_denormalize_config_from_web") == 0) {
            out = ws_u_denormalize_config_from_web(in);
        } else if (strcmp(func, "ws_u_resolve_provider_status") == 0) {
            out = ws_u_resolve_provider_status(in);
        } else if (strcmp(func, "ws_u_maybe_auto_archive_for_profile") == 0) {
            out = ws_u_maybe_auto_archive_for_profile(in);
        } else if (strcmp(func, "ws_u_spawn_gateway_restart") == 0) {
            out = ws_u_spawn_gateway_restart(in);
        } else if (strcmp(func, "ws_u_custom_endpoint_response") == 0) {
            out = ws_u_custom_endpoint_response(in);
        } else if (strcmp(func, "ws_u_get_models_analytics") == 0) {
            out = ws_u_get_models_analytics(in);
        } else if (strcmp(func, "ws_u_telegram_onboarding_request_sync") == 0) {
            out = ws_u_telegram_onboarding_request_sync(in);
        } else if (strcmp(func, "ws_get_recommended_default_model") == 0) {
            out = ws_get_recommended_default_model(in);
        } else if (strcmp(func, "ws_u_discover_dashboard_plugins") == 0) {
            out = ws_u_discover_dashboard_plugins(in);
        } else if (strcmp(func, "ws_u_catalog_provider_env_metadata") == 0) {
            out = ws_u_catalog_provider_env_metadata(in);
        } else {
            fprintf(stderr, "ws_revive1 harness: unknown func %s\n", func);
            json_free(rec);
            json_free(ctx.trace);
            json_free(ctx.counts);
            json_free(st);
            return 2;
        }
        w1_emit(func, in, out, ctx.trace, st);
        json_free(ctx.counts);
        json_free(rec);
    }
    return 0;
}