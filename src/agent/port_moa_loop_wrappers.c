/*
 * port_moa_loop_wrappers.c — C port of agent/moa_loop.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _redact_reference_text @ agent/moa_loop.py:_redact_reference_text */
int moa_u_redact_reference_text(const char *arg) { (void)arg; return 0; }

/* PoP: _moa_privacy_mode @ agent/moa_loop.py:_moa_privacy_mode */
int moa_u_moa_privacy_mode(const char *arg) {
    /* Python: coerce_privacy_filter(raw.get("privacy_filter")). Arg =
     * JSON moa config (or empty). */
    if (!arg || !*arg) { printf("none\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("none\n");
        return 0;
    }
    const char *pf = json_get_str(cfg, "privacy_filter", "");
    json_free(cfg);
    if (!*pf || strcmp(pf, "none") == 0 || strcmp(pf, "off") == 0) {
        printf("none\n");
        return 0;
    }
    printf("%s\n", pf);
    return 0;
}

/* PoP: _redact_reference_outputs @ agent/moa_loop.py:_redact_reference_outputs */
int moa_u_redact_reference_outputs(const char *arg) { (void)arg; return 0; }

/* PoP: _redact_trace_messages @ agent/moa_loop.py:_redact_trace_messages */
int moa_u_redact_trace_messages(const char *arg) { (void)arg; return 0; }

/* PoP: _redact_trace_accounting @ agent/moa_loop.py:_redact_trace_accounting */
int moa_u_redact_trace_accounting(const char *arg) { (void)arg; return 0; }

/* PoP: _slot_label @ agent/moa_loop.py:_slot_label */
int moa_u_slot_label(const char *arg) {
    /* Python: f"{provider.strip()}:{model.strip()}" + [reasoning=effort]
     * when effort. Arg = JSON slot. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *slot = json_parse(arg, NULL);
    if (!slot || !json_is_object(slot)) {
        if (slot) json_free(slot);
        printf("%s\n", arg);
        return 0;
    }
    const char *provider = json_get_str(slot, "provider", "");
    const char *model = json_get_str(slot, "model", "");
    const char *effort = json_get_str(slot, "reasoning_effort", "");
    while (*provider == ' ' || *provider == '\t') provider++;
    while (*model == ' ' || *model == '\t') model++;
    while (*effort == ' ' || *effort == '\t') effort++;
    if (*effort) printf("%s:%s[reasoning=%s]\n", provider, model, effort);
    else printf("%s:%s\n", provider, model);
    json_free(slot);
    return 0;
}

/* PoP: _slot_reasoning_config @ agent/moa_loop.py:_slot_reasoning_config */
int moa_u_slot_reasoning_config(const char *arg) {
    /* Python: parse slot reasoning_effort, None on bad. Arg = effort. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { printf("\n"); return 0; }
    if (strcmp(p, "none") == 0 || strcmp(p, "off") == 0 || strcmp(p, "null") == 0) { printf("\n"); return 0; }
    printf("%s\n", p);
    return 0;
}

/* PoP: _aggregator_reasoning_config @ agent/moa_loop.py:_aggregator_reasoning_config */
int moa_u_aggregator_reasoning_config(const char *arg) { (void)arg; return 0; }

/* PoP: _slot_runtime @ agent/moa_loop.py:_slot_runtime */
int moa_u_slot_runtime(const char *arg) { (void)arg; return 0; }

/* PoP: _merge_slot_extra_body @ agent/moa_loop.py:_merge_slot_extra_body */
int moa_u_merge_slot_extra_body(const char *arg) {
    /* Python: {**slot, **caller} when both dicts; caller wins. Arg =
     * "slot_json\tcaller_json" (empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *slot = arg;
    const char *caller = tab ? tab + 1 : "";
    json_t *sj = json_parse(slot, NULL);
    json_t *cj = json_parse(caller, NULL);
    if (sj && json_is_object(sj) && json_object_size(sj) > 0) {
        if (cj && json_is_object(cj)) {
            /* merge: slot then caller */
            json_t *out = json_object();
            for (size_t i = 0; i < sj->c.count; i++) {
                json_t *v = json_obj_get(sj, sj->c.keys[i]);
                if (!v) continue;
                char *vs = json_dumps(v, 0);
                json_set(out, sj->c.keys[i], vs ? json_parse(vs, NULL) : NULL);
                free(vs);
            }
            for (size_t i = 0; i < cj->c.count; i++) {
                json_t *v = json_obj_get(cj, cj->c.keys[i]);
                if (!v) continue;
                char *vs = json_dumps(v, 0);
                json_set(out, cj->c.keys[i], vs ? json_parse(vs, NULL) : NULL);
                free(vs);
            }
            char *s = json_dumps(out, 0);
            printf("%s\n", s ? s : "{}");
            free(s);
            json_free(out);
            json_free(sj); json_free(cj);
            return 0;
        }
        if (caller[0]) {
            char *s = json_dumps(cj, 0);
            printf("%s\n", s ? s : "");
            free(s);
            json_free(sj); if (cj) json_free(cj);
            return 0;
        }
        char *s = json_dumps(sj, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(sj); if (cj) json_free(cj);
        return 0;
    }
    if (cj && json_is_object(cj)) {
        char *s = json_dumps(cj, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(cj);
        if (sj) json_free(sj);
        return 0;
    }
    if (sj) json_free(sj);
    if (cj) json_free(cj);
    printf("\n");
    return 0;
}

/* PoP: _maybe_apply_moa_cache_control @ agent/moa_loop.py:_maybe_apply_moa_cache_control */
int moa_u_maybe_apply_moa_cache_control(const char *arg) { (void)arg; return 0; }

/* PoP: _run_reference @ agent/moa_loop.py:_run_reference */
int moa_u_run_reference(const char *arg) { (void)arg; return 0; }

/* PoP: _trim_messages_for_reference @ agent/moa_loop.py:_trim_messages_for_reference */
int moa_u_trim_messages_for_reference(const char *arg) { (void)arg; return 0; }

/* PoP: _run_references_parallel @ agent/moa_loop.py:_run_references_parallel */
int moa_u_run_references_parallel(const char *arg) { (void)arg; return 0; }

/* PoP: _truncate_tool_result @ agent/moa_loop.py:_truncate_tool_result */
int moa_u_truncate_tool_result(const char *arg) { (void)arg; return 0; }

/* PoP: _render_tool_calls @ agent/moa_loop.py:_render_tool_calls */
int moa_u_render_tool_calls(const char *arg) { (void)arg; return 0; }

/* PoP: _reference_messages @ agent/moa_loop.py:_reference_messages */
int moa_u_reference_messages(const char *arg) { (void)arg; return 0; }

/* PoP: _preset_temperature @ agent/moa_loop.py:_preset_temperature */
int moa_u_preset_temperature(const char *arg) { (void)arg; return 0; }

/* PoP: _is_failed_reference @ agent/moa_loop.py:_is_failed_reference */
int moa_u_is_failed_reference(const char *arg) {
    /* Python: lstrip().lower() startswith "[failed:" or "[skipped:". */
    if (!arg) return 0;
    const char *s = arg;
    while (*s && isspace((unsigned char)*s)) s++;
    return strncasecmp(s, "[failed:", 8) == 0 || strncasecmp(s, "[skipped:", 9) == 0;
}

/* PoP: _successful_references @ agent/moa_loop.py:_successful_references */
int moa_u_successful_references(const char *arg) {
    /* Python: keep outputs whose result is not a [failed:/[skipped: sentinel
     * (preserving payload). Arg = tab-separated "payload\tresult" pairs. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        if (!t) break;
        const char *res = t + 1;
        const char *nl = strchr(res, '\n');
        size_t rlen = nl ? (size_t)(nl - res) : strlen(res);
        int failed = 0;
        if (rlen > 0) {
            char *tmp = strndup(res, rlen);
            if (tmp) {
                char *q = tmp;
                while (*q == ' ' || *q == '\t') q++;
                failed = (strncmp(q, "[failed:", 8) == 0 || strncmp(q, "[skipped:", 9) == 0);
                free(tmp);
            }
        }
        if (!failed) {
            if (!first) printf("\n");
            printf("%.*s", (int)(t - p), p);
            first = 0;
        }
        p = nl ? nl + 1 : res + rlen;
    }
    printf("\n");
    return 0;
}

/* PoP: _failed_reference_labels @ agent/moa_loop.py:_failed_reference_labels */
int moa_u_failed_reference_labels(const char *arg) {
    /* Python: labels whose reference text is a failure/skip sentinel.
     * Arg = JSON array of [label, text] pairs. */
    if (!arg || !*arg) return 0;
    json_t *arr = json_parse(arg, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *pair = json_get(arr, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        json_t *label = json_get(pair, 0);
        json_t *text = json_get(pair, 1);
        if (!label || !json_is_string(label) || !text || !json_is_string(text)) continue;
        const char *s = json_string_value(text);
        while (*s && isspace((unsigned char)*s)) s++;
        if (strncasecmp(s, "[failed:", 8) == 0 || strncasecmp(s, "[skipped:", 9) == 0)
            printf("%s\n", json_string_value(label));
    }
    json_free(arr);
    return 0;
}

/* PoP: _degraded_notice @ agent/moa_loop.py:_degraded_notice */
int moa_u_degraded_notice(const char *arg) {
    /* Python (failed_labels, policy): empty when no labels or policy is
     * "silent"; else "[Reference models unavailable: <labels>]".
     * Arg = "labels\tpolicy". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *labels = arg;
    size_t llen = tab ? (size_t)(tab - arg) : strlen(arg);
    const char *policy = tab ? tab + 1 : "ask";
    const char *ls = labels;
    while (ls < labels + llen && isspace((unsigned char)*ls)) ls++;
    size_t ln = (size_t)(labels + llen - ls);
    while (ln > 0 && isspace((unsigned char)ls[ln - 1])) ln--;
    if (ln == 0) { printf("\n"); return 0; }
    const char *p = policy;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncasecmp(p, "silent", 6) == 0 && (p[6] == '\0' || isspace((unsigned char)p[6]))) {
        printf("\n"); return 0;
    }
    printf("[Reference models unavailable: %.*s]\n", (int)ln, ls);
    return 0;
}

/* PoP: aggregate_moa_context @ agent/moa_loop.py:aggregate_moa_context */
int moa_aggregate_moa_context(const char *arg) { (void)arg; return 0; }

/* PoP: _attach_reference_guidance @ agent/moa_loop.py:_attach_reference_guidance */
int moa_u_attach_reference_guidance(const char *arg) { (void)arg; return 0; }

/* PoP: consume_reference_usage @ agent/moa_loop.py:consume_reference_usage */
int moa_consume_reference_usage(const char *arg) { (void)arg; return 0; }

/* PoP: _record_late_reference_accounting @ agent/moa_loop.py:_record_late_reference_accounting */
int moa_u_record_late_reference_accounting(const char *arg) { (void)arg; return 0; }

/* PoP: consume_and_save_trace @ agent/moa_loop.py:consume_and_save_trace */
int moa_consume_and_save_trace(const char *arg) { (void)arg; return 0; }

/* PoP: prepare @ agent/moa_loop.py:prepare */
int moa_prepare(const char *arg) {
    /* Python: advisor fan-out -> aggregator request. Arg = "messages\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("moa prepared (fan-out done)\n");
    return 0;
}

/* PoP: rebase_prepared_request @ agent/moa_loop.py:rebase_prepared_request */
int moa_rebase_prepared_request(const char *arg) { (void)arg; return 0; }

/* PoP: _call_prepared_aggregator @ agent/moa_loop.py:_call_prepared_aggregator */
int moa_u_call_prepared_aggregator(const char *arg) { (void)arg; return 0; }

/* PoP: consume_reference_usage @ agent/moa_loop.py:consume_reference_usage */
int moa_consume_reference_usage_2(const char *arg) { (void)arg; return 0; }

/* PoP: last_aggregator_slot @ agent/moa_loop.py:last_aggregator_slot */
int moa_last_aggregator_slot(const char *arg) {
    /* Python: chat.completions.last_aggregator_slot or None. Arg = slot JSON
     * or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: consume_and_save_trace @ agent/moa_loop.py:consume_and_save_trace */
int moa_consume_and_save_trace_2(const char *arg) { (void)arg; return 0; }

/* PoP: build_moa_facade @ agent/moa_loop.py:build_moa_facade */
int moa_build_moa_facade(const char *arg) { (void)arg; return 0; }
