/*
 * conversation_compression_deadline.c — C11 port of upstream Hermes
 * agent/conversation_compression_deadline.py (gate w25_conversati_616874).
 * STATUS: real behaviour port of the value core; the config file, the
 *         auxiliary client and the commit fence are the seam.
 * PoP: w25_conversati_616874_resolve_timeouts @ agent/conversation_compression_deadline.py:resolve_context_compression_timeouts
 * PoP: w25_conversati_616874_effective_ceiling @ agent/conversation_compression_deadline.py:effective_ceiling
 * PoP: w25_conversati_616874_constants @ agent/conversation_compression_deadline.py:DEFAULT_CONTEXT_TIMEOUT_SECONDS
 *
 * Compression waits a total time limit for its summary.  An idle limit <= 0
 * disables the progress-aware wrapper; the ceiling is raised to at least one
 * idle window, and the idle window is raised to the effective aux budget so
 * prep/chunking work that streams no token yet is not cut off.  Only raises:
 * an explicit config value above the aux budget is kept, and idle never
 * exceeds the ceiling.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_conversati_616874.h"

/* DEFAULT_CONTEXT_TIMEOUT_SECONDS / DEFAULT_CONTEXT_TOTAL_CEILING_SECONDS */
#define CD_DEFAULT_IDLE 120.0
#define CD_DEFAULT_CEILING 600.0

static int cd_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* float(value): numbers and bools convert, strings parse, containers raise. */
static int cd_py_float(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 0; /* TypeError */
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        double d;
        size_t n;
        while (*s && cd_ws(*s)) s++;
        n = strlen(s);
        while (n > 0 && cd_ws(s[n - 1])) n--;
        if (n == 0) return 0;
        d = strtod(s, &end);
        if (!end || *end == '\0') { *out = d; return 1; }
        /* strtod stopped early; tolerate trailing whitespace we just walked */
        {
            const char *p = end;
            while (*p && cd_ws(*p)) p++;
            if (*p == '\0') { *out = d; return 1; }
        }
        return 0; /* ValueError */
    }
    return 0; /* TypeError */
}

json_t *w25_conversati_616874_resolve_timeouts(const json_t *cfg, const json_t *aux_budget) {
    double idle = CD_DEFAULT_IDLE;
    double ceiling = CD_DEFAULT_CEILING;
    json_t *out;
    const json_t *raw_idle, *raw_ceiling;
    double v;

    if (cfg && cfg->type == JSON_OBJECT) {
        /* Explicit 0/negative idle disables; a non-positive ceiling is ignored. */
        raw_idle = json_obj_get(cfg, "context_timeout_seconds");
        if (raw_idle && raw_idle->type != JSON_NULL && cd_py_float(raw_idle, &v))
            idle = v;
        raw_ceiling = json_obj_get(cfg, "context_total_ceiling_seconds");
        if (raw_ceiling && raw_ceiling->type != JSON_NULL && cd_py_float(raw_ceiling, &v) &&
            v > 0)
            ceiling = v;
    }
    if (idle > 0) {
        double aux;
        if (ceiling < idle) ceiling = idle;
        /* the aux summary attempt legitimately needs up to its own budget */
        if (cd_py_float(aux_budget, &aux) && aux > 0) {
            if (ceiling < aux) ceiling = aux;
            if (aux < ceiling) {
                if (idle < aux) idle = aux;
            } else {
                if (idle < ceiling) idle = ceiling;
            }
        }
    }
    out = json_array();
    json_append(out, json_number(idle));
    json_append(out, json_number(ceiling));
    return out;
}

json_t *w25_conversati_616874_effective_ceiling(const json_t *fence_deadline,
                                                const json_t *ceiling,
                                                const json_t *wait_started) {
    double limit = 0.0, started = 0.0, deadline = 0.0;

    if (!cd_py_float(ceiling, &limit)) limit = 0.0;
    if (!cd_py_float(wait_started, &started)) started = 0.0;
    if (!fence_deadline || fence_deadline->type == JSON_NULL)
        return json_number(limit); /* deadline is None */
    if (!cd_py_float(fence_deadline, &deadline)) return json_number(limit);
    return json_number(limit > deadline - started ? limit : deadline - started);
}

json_t *w25_conversati_616874_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_context_timeout_seconds", json_number(CD_DEFAULT_IDLE));
    json_set(o, "default_context_total_ceiling_seconds", json_number(CD_DEFAULT_CEILING));
    json_set(o, "idle_key", json_string("context_timeout_seconds"));
    json_set(o, "ceiling_key", json_string("context_total_ceiling_seconds"));
    json_set(o, "aux_task", json_string("compression"));
    return o;
}
