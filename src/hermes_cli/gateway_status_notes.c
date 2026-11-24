/*
 * gateway_status_notes.c - C11 port of upstream hermes_cli/gateway_status_notes.py
 * (gate w24_gatewaysta_605a76).
 *
 * PoP: w24_gatewaysta_605a76_verdict @ hermes_cli/gateway_status_notes.py:_incompatible_owner_verdict
 * PoP: w24_gatewaysta_605a76_report_lines @ hermes_cli/gateway_status_notes.py:report_incompatible_owner
 * PoP: w24_gatewaysta_605a76_standalone_note @ hermes_cli/gateway_status_notes.py:print_standalone_by_config
 *
 * Ported: the three notes' text and their conditions.  The discovery probe (and
 * its 1.5s timeout, its OSError/ValueError on a bad home), the Hermes home, the
 * profile roster and the process scan are seams: the observed verdict, the pids
 * and the two profile sets arrive as values.  ``print`` itself is the caller's;
 * these return the lines that would be printed, or null where the upstream
 * function prints nothing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_gatewaysta_605a76.h"

/* The literals the module prints, kept in one place. */
#define GS_VERDICT_PREFIX   "incompatible: "
#define GS_UNKNOWN_REASON   "unknown"
#define GS_RUNNING_PREFIX   "Gateway is running (PID: "
#define GS_RUNNING_SERVICE  "The running gateway"
#define GS_CANNOT_SERVE     " but cannot serve this Hermes ("
#define GS_STANDALONE       "standalone by config (temporary compatibility shim): "

static int gs_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len((json_t *)v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value) for the scalars these notes interpolate (pids, a reason code). */
static void gs_str(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:   snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else
            snprintf(buf, n, "%.17g", v->num_val);
        return;
    case JSON_STRING: snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); return;
    default:          snprintf(buf, n, "%s", "None"); return;
    }
}

/* f"incompatible: {reason_code or 'unknown'}", or null when the state is not
 * incompatible (or discovery raised, which the module swallows). */
static char *gs_verdict_text(const json_t *observed, int raises) {
    const json_t *state, *reason;
    char reason_buf[512], *out;
    size_t need;

    if (raises || !observed || observed->type != JSON_OBJECT) return NULL;
    state = json_obj_get(observed, "state");
    if (!state || state->type != JSON_STRING || !state->str_val ||
        strcmp(state->str_val, "incompatible") != 0)
        return NULL;
    reason = json_obj_get(observed, "reason_code");
    if (gs_truthy(reason)) gs_str(reason, reason_buf, sizeof(reason_buf));
    else snprintf(reason_buf, sizeof(reason_buf), "%s", GS_UNKNOWN_REASON);
    need = strlen(GS_VERDICT_PREFIX) + strlen(reason_buf) + 1;
    out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s%s", GS_VERDICT_PREFIX, reason_buf);
    return out;
}

json_t *w24_gatewaysta_605a76_verdict(const json_t *observed, int raises) {
    char *text = gs_verdict_text(observed, raises);
    json_t *out;
    if (!text) return json_null();
    out = json_string(text);
    free(text);
    return out;
}

json_t *w24_gatewaysta_605a76_report_lines(const json_t *observed, int raises,
                                           const json_t *pids) {
    char *stale = gs_verdict_text(observed, raises);
    json_t *lines;
    char *buf;
    size_t need;

    if (!stale) return json_null();                 /* nothing printed */
    lines = json_array();

    /* running = f"Gateway is running (PID: {', '.join(map(str, pids))})" if pids
     *           else "The running gateway" */
    if (pids && pids->type == JSON_ARRAY && json_len((json_t *)pids) > 0) {
        size_t i, n = json_len((json_t *)pids);
        need = strlen(GS_RUNNING_PREFIX) + 1;
        for (i = 0; i < n; i++) need += 64 + 2;
        buf = (char *)malloc(need);
        if (!buf) { free(stale); json_free(lines); return json_null(); }
        snprintf(buf, need, "%s", GS_RUNNING_PREFIX);
        for (i = 0; i < n; i++) {
            char one[64];
            gs_str(json_get(pids, i), one, sizeof(one));
            if (i) strncat(buf, ", ", need - strlen(buf) - 1);
            strncat(buf, one, need - strlen(buf) - 1);
        }
        strncat(buf, ")", need - strlen(buf) - 1);
    } else {
        buf = (char *)malloc(strlen(GS_RUNNING_SERVICE) + 1);
        if (!buf) { free(stale); json_free(lines); return json_null(); }
        snprintf(buf, strlen(GS_RUNNING_SERVICE) + 1, "%s", GS_RUNNING_SERVICE);
    }

    /* ✗ {running} but cannot serve this Hermes ({stale}) */
    {
        size_t n2 = 4 + strlen(buf) + strlen(GS_CANNOT_SERVE) + strlen(stale) + 2;
        char *line1 = (char *)malloc(n2);
        if (line1) {
            snprintf(line1, n2, "\xe2\x9c\x97 %s%s%s)", buf, GS_CANNOT_SERVE, stale);
            json_append(lines, json_string(line1));
            free(line1);
        }
    }
    free(buf);
    free(stale);

    json_append(lines, json_string(
        "  It was started by another Hermes version; TUI, Desktop and chat cannot attach to it."));
    json_append(lines, json_string("  Replace it with this version:"));
    json_append(lines, json_string("    hermes gateway restart"));
    return lines;
}

/* Sorted, de-duplicated roster minus served minus {"default"}. */
static char **gs_names(const json_t *roster, const json_t *served, size_t *out_n) {
    size_t cap, n = 0, i, j;
    char **names;

    *out_n = 0;
    if (!roster || roster->type != JSON_ARRAY) return NULL;
    cap = json_len((json_t *)roster);
    if (cap == 0) return NULL;
    names = (char **)calloc(cap, sizeof(char *));
    if (!names) return NULL;

    for (i = 0; i < cap; i++) {
        const json_t *v = json_get(roster, i);
        int skip = 0;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        if (strcmp(v->str_val, "default") == 0) continue;   /* {"default"} */
        for (j = 0; j < n; j++)
            if (strcmp(names[j], v->str_val) == 0) { skip = 1; break; }
        if (skip) continue;
        if (served && served->type == JSON_ARRAY) {
            size_t k, m = json_len((json_t *)served);
            for (k = 0; k < m; k++) {
                const json_t *s = json_get(served, k);
                if (s && s->type == JSON_STRING && s->str_val &&
                    strcmp(s->str_val, v->str_val) == 0) { skip = 1; break; }
            }
        }
        if (skip) continue;
        names[n++] = strdup(v->str_val);
    }
    /* sorted(names) */
    for (i = 1; i < n; i++) {
        char *key = names[i];
        j = i;
        while (j > 0 && strcmp(names[j - 1], key) > 0) {
            names[j] = names[j - 1];
            j--;
        }
        names[j] = key;
    }
    *out_n = n;
    return names;
}

json_t *w24_gatewaysta_605a76_standalone_note(const json_t *active,
                                              const json_t *roster,
                                              const json_t *served) {
    char **names;
    size_t n = 0, i, need;
    json_t *out;
    char *buf;

    /* (get_active_profile_name() or "default") != "default" -> silent */
    if (active && active->type == JSON_STRING && active->str_val &&
        active->str_val[0] != '\0' && strcmp(active->str_val, "default") != 0)
        return json_null();
    if (active && active->type != JSON_STRING && gs_truthy(active))
        return json_null();                          /* a name that is not "default" */

    names = gs_names(roster, served, &n);
    if (n == 0) { free(names); return json_null(); }
    need = strlen(GS_STANDALONE) + 1;
    for (i = 0; i < n; i++) need += strlen(names[i]) + 2;
    buf = (char *)malloc(need);
    if (!buf) {
        for (i = 0; i < n; i++) free(names[i]);
        free(names);
        return json_null();
    }
    snprintf(buf, need, "%s", GS_STANDALONE);
    for (i = 0; i < n; i++) {
        if (i) strncat(buf, ", ", need - strlen(buf) - 1);
        strncat(buf, names[i], need - strlen(buf) - 1);
        free(names[i]);
    }
    free(names);
    out = json_string(buf);
    free(buf);
    return out;
}
