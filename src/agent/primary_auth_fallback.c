/*
 * primary_auth_fallback.c — faithful C11 port of agent/primary_auth_fallback.py.
 * Gate w14_primaryaut_c69abc.
 *
 * PoP: w14_primaryaut_c69abc_route_snapshot @ agent/primary_auth_fallback.py:route_snapshot
 * PoP: w14_primaryaut_c69abc_failure_for_route @ agent/primary_auth_fallback.py:failure_for_route
 * PoP: w14_primaryaut_c69abc_primary_auth_lead @ agent/primary_auth_fallback.py:primary_auth_lead
 *
 * The three imported copy helpers are deterministic injection seams (documented in
 * the header); no process global/contextvar state is used.
 */
#include "noc_w14_primaryaut_c69abc.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const ROUTE_FIELDS[] = {"provider", "base_url", "api_key", NULL};

struct json_t *w14_primaryaut_c69abc_route_snapshot(const struct json_t *runtime_kwargs) {
    json_t *out = json_array();
    for (int i = 0; ROUTE_FIELDS[i]; i++) {
        const json_t *v = (runtime_kwargs && runtime_kwargs->type == JSON_OBJECT)
            ? json_obj_get(runtime_kwargs, ROUTE_FIELDS[i]) : NULL;
        json_append(out, v ? json_copy((json_t *)v) : json_null());
    }
    return out;
}

static int json_equal(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    char *sa = json_dumps(a, 0), *sb = json_dumps(b, 0);
    int eq = sa && sb && !strcmp(sa, sb);
    free(sa); free(sb);
    return eq;
}

struct json_t *w14_primaryaut_c69abc_failure_for_route(const struct json_t *failure,
        const struct json_t *resolved, const struct json_t *runtime_kwargs) {
    json_t *snap = w14_primaryaut_c69abc_route_snapshot(runtime_kwargs);
    int same = json_equal(snap, resolved);
    json_free(snap);
    if (same && failure && failure->type != JSON_NULL) return json_copy((json_t *)failure);
    return json_null();
}

static const char *backup_problem(const char *reason) {
    static const struct { const char *code; const char *text; } MAP[] = {
        {"billing", "is out of credits (or over its usage limit)"},
        {"billing_unverified", "reported it is out of credits (or over its usage limit)"},
        {"auth", "rejected its sign-in as well"},
        {"auth_permanent", "rejected its sign-in as well"},
        {"rate_limit", "is rate-limiting requests"},
        {"upstream_rate_limit", "is rate-limiting requests"},
        {"overloaded", "is overloaded"},
        {"server_error", "returned a server error"},
        {"timeout", "did not respond in time"},
        {"model_not_found", "does not have the configured model"},
        {NULL, NULL}
    };
    if (!reason) return NULL;
    for (int i = 0; MAP[i].code; i++) if (!strcmp(reason, MAP[i].code)) return MAP[i].text;
    return NULL;
}

/* Python str(v) for the scalar reasons used by fixtures (None/falsy -> ""). */
static void py_str_of(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { buf[0] = '\0'; return; }
    if (v->type == JSON_BOOL) { snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_STRING) { snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); return; }
    char *s = json_dumps(v, 0);
    snprintf(buf, n, "%s", s ? s : "");
    free(s);
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

const char *w14_primaryaut_c69abc_primary_auth_lead(const struct json_t *failure,
        const char *backup, const struct json_t *failure_reason, const struct json_t *error) {
    char provider[1024] = "your main AI provider";
    char relogin[1024] = "";
    char why[2048] = "";
    if (failure && failure->type == JSON_OBJECT) {
        const json_t *pv = json_obj_get(failure, "provider");
        if (pv && pv->type == JSON_STRING && pv->str_val && pv->str_val[0])
            snprintf(provider, sizeof(provider), "provider:%s", pv->str_val);  /* provider_label_for seam */
        const json_t *lv = json_obj_get(failure, "relogin");
        if (lv && lv->type == JSON_STRING && lv->str_val)
            snprintf(relogin, sizeof(relogin), "%s", lv->str_val);
        const json_t *ev = json_obj_get(failure, "error");
        if (ev && ev->type == JSON_STRING && ev->str_val)
            snprintf(why, sizeof(why), "%s", ev->str_val);  /* redact_sensitive_text identity seam */
    }
    char reason[1024] = "";
    py_str_of(failure_reason, reason, sizeof(reason));  /* str(failure_reason or "") */
    const char *problem = backup_problem(reason);
    char problem_buf[2300];
    if (problem) {
        snprintf(problem_buf, sizeof(problem_buf), "%s", problem);
    } else if (py_truthy(error)) {
        char detail[160 + 1];
        py_str_of(error, detail, sizeof(detail));  /* short_detail(error, 160) seam */
        snprintf(problem_buf, sizeof(problem_buf), "failed too (%s)", detail);
    } else {
        snprintf(problem_buf, sizeof(problem_buf), "failed too (failed)");
    }
    const char *backup_out = (backup && backup[0]) ? backup : "backup";
    static char out[8192];
    snprintf(out, sizeof(out),
        "\xe2\x9a\xa0\xef\xb8\x8f %s needs you to sign in again: run `%s`. Its login failed "
        "(%s), so Hermes switched to the backup provider (%s), and the backup %s. "
        "Re-authenticating %s fixes this; topping up the backup only keeps the fallback running.",
        provider, relogin, why, backup_out, problem_buf, provider);
    return out;
}
