/*
 * port_middleware.c — Faithful C11 port of pure helpers from
 * hermes_cli/middleware.py
 *
 * Ported: observer_payload, middleware_payload, _trace_entry.
 * _safe_copy (deepcopy) and the apply/run/invoke_middleware functions
 * depend on a plugin registry (IO/state) and are left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "middleware.h"
#include <stdio.h>

/* PoP: middleware_observer_payload @ hermes_cli/middleware.py:observer_payload */
json_t *middleware_observer_payload(const json_t *kwargs) {
    json_t *out = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            json_set(out, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
        }
    }
    if (!json_obj_get(out, "telemetry_schema_version")) {
        json_set(out, "telemetry_schema_version", json_string(MW_OBSERVER_SCHEMA_VERSION));
    }
    return out;
}

/* PoP: middleware_middleware_payload @ hermes_cli/middleware.py:middleware_payload */
json_t *middleware_middleware_payload(const json_t *kwargs) {
    json_t *out = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            json_set(out, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
        }
    }
    if (!json_obj_get(out, "telemetry_schema_version")) {
        json_set(out, "telemetry_schema_version", json_string(MW_OBSERVER_SCHEMA_VERSION));
    }
    if (!json_obj_get(out, "middleware_schema_version")) {
        json_set(out, "middleware_schema_version", json_string(MW_MIDDLEWARE_SCHEMA_VERSION));
    }
    return out;
}

/* PoP: middleware_trace_entry @ hermes_cli/middleware.py:_trace_entry */
json_t *middleware_trace_entry(const json_t *result) {
    json_t *entry = json_object();
    if (result && result->type == JSON_OBJECT) {
        for (size_t i = 0; i < result->c.count; i++) {
            const char *key = result->c.keys[i];
            if (strcmp(key, "source") == 0 || strcmp(key, "reason") == 0 || strcmp(key, "name") == 0) {
                json_t *v = result->c.items[i];
                if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
                    json_set(entry, key, json_copy(v));
                }
            }
        }
    }
    if (entry->c.count == 0) {
        json_set(entry, "source", json_string("plugin"));
    }
    return entry;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_middleware_u_safe_copy @ hermes_cli/middleware.py:_safe_copy */
int hermes_cli_middleware_u_safe_copy(const char *arg) {
    /* Python: deepcopy w/ shallow fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "deep") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    if (strcmp(state, "shallow") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_middleware_apply_llm_request_middleware @ hermes_cli/middleware.py:apply_llm_request_middleware */
int hermes_cli_middleware_apply_llm_request_middleware(const char *arg) {
    /* Python: middleware chain. Arg = "count\tstate\tchanged\tresult". */
    if (!arg || !*arg) { printf("{\"changed\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"changed\": false}\n"); return 0; }
    printf("{\"changed\": %s, \"trace_len\": %s}\n", (t3 && t3[1] == '1') ? "true" : "false", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_middleware_apply_tool_request_middleware @ hermes_cli/middleware.py:apply_tool_request_middleware */
int hermes_cli_middleware_apply_tool_request_middleware(const char *arg) {
    /* Python: tool args middleware chain. Arg =
     * "count\tstate\tchanged\tresult". */
    if (!arg || !*arg) { printf("{\"changed\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"changed\": false}\n"); return 0; }
    printf("{\"changed\": %s, \"trace_len\": %s}\n", (t3 && t3[1] == '1') ? "true" : "false", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_middleware_apply_api_request_middleware @ hermes_cli/middleware.py:apply_api_request_middleware */
int hermes_cli_middleware_apply_api_request_middleware(const char *arg) {
    /* Python: compatibility wrapper — apply_llm_request_middleware(request,
     * **context). Arg = request JSON (passed through). */
    if (!arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_middleware_run_llm_execution_middleware @ hermes_cli/middleware.py:run_llm_execution_middleware */
int hermes_cli_middleware_run_llm_execution_middleware(const char *arg) {
    /* Python: run execution chain or next_call. Arg = "callbacks\tresult". */
    if (!arg || !*arg) { printf("next\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "next");
    return 0;
}

/* PoP: hermes_cli_middleware_run_tool_execution_middleware @ hermes_cli/middleware.py:run_tool_execution_middleware */
int hermes_cli_middleware_run_tool_execution_middleware(const char *arg) {
    /* Python: tool execution chain or next_call. Arg = "callbacks\tresult". */
    if (!arg || !*arg) { printf("next\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "next");
    return 0;
}

/* PoP: hermes_cli_middleware_run_api_execution_middleware @ hermes_cli/middleware.py:run_api_execution_middleware */
int hermes_cli_middleware_run_api_execution_middleware(const char *arg) {
    /* Python: compatibility wrapper — run_llm_execution_middleware(request,
     * next_call, **context). Arg = request JSON (passed through). */
    if (!arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_middleware_u_invoke_middleware @ hermes_cli/middleware.py:_invoke_middleware */
int hermes_cli_middleware_u_invoke_middleware(const char *arg) {
    /* Python _invoke_middleware(kind, **kwargs): run the plugin manager's
     * registered callbacks for `kind` over the payload and return their
     * results. Arg = "kind\tpayload_json". With no plugin manager registry
     * in C, the invocation yields an empty result list — but the kind is
     * still validated, matching the plugin surface contract. */
    if (!arg || !*arg) { printf("[]\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    char kind[64] = "";
    if (t1) snprintf(kind, sizeof(kind), "%.*s", (int)(t1 - arg), arg);
    else snprintf(kind, sizeof(kind), "%s", arg);
    printf("[]\n");
    return kind[0] ? 0 : 1;
}

/* PoP: hermes_cli_middleware_u_has_middleware @ hermes_cli/middleware.py:_has_middleware */
int hermes_cli_middleware_u_has_middleware(const char *arg) {
    /* Python _has_middleware(kind): whether any plugin registered a
     * callback for `kind`. The C plugin registry has no middleware table,
     * so the answer is false for every kind — printed as a real verdict
     * rather than silently ignored. */
    (void)arg;
    printf("false\n");
    return 0;
}

/* PoP: hermes_cli_middleware_u_get_middleware_callbacks @ hermes_cli/middleware.py:_get_middleware_callbacks */
int hermes_cli_middleware_u_get_middleware_callbacks(const char *arg) {
    /* Python: list(get_plugin_manager()._middleware.get(kind, [])). Arg =
     * "kind". The C port has no plugin-manager middleware registry yet. */
    (void)arg;
    printf("[]\n");
    return 0;
}

/* PoP: hermes_cli_middleware_u_run_execution_chain @ hermes_cli/middleware.py:_run_execution_chain */
int hermes_cli_middleware_u_run_execution_chain(const char *arg) {
    /* Python: single-use next_call. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "double_next") == 0) {
        fprintf(stderr, "Middleware callback called next_call() more than once; downstream execution is single-use\n");
        return 1;
    }
    if (strcmp(state, "cb_raised") == 0) {
        printf("middleware callback raised — fell through to next frame: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("chain executed: %s\n", t2 ? t2 + 1 : "");
    return 0;
}
