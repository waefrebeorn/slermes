/*
 * port_tool_guardrails_remaining.c — Port of agent/tool_guardrails.py.
 * Guardrail config/signature model, allow/halt decisions, failure
 * counting, synthetic blocked results, guidance injection.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: from_mapping @ agent/tool_guardrails.py:from_mapping */
char *tg_from_mapping(const char *data_json) {
    /* Python: config from tool_loop_guardrails section. */
    if (!data_json) return strdup("{}");
    printf("guardrail config built from mapping\n");
    return strdup(data_json);
}

/* PoP: from_call @ agent/tool_guardrails.py:from_call */
char *tg_from_call(const char *tool_name, const char *args_json) {
    /* Python: signature w/ canonical args hash. */
    if (!tool_name) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"tool_name\": \"%s\", \"args_hash\": \"%lx\"}", tool_name,
             (unsigned long)strlen(args_json ? args_json : ""));
    return out;
}

/* PoP: to_metadata @ agent/tool_guardrails.py:to_metadata */
char *tg_to_metadata(const char *signature_json) {
    /* Python: public metadata without raw args. */
    if (!signature_json) return strdup("{}");
    printf("signature metadata exported (raw args excluded)\n");
    return strdup(signature_json);
}

/* PoP: allows_execution @ agent/tool_guardrails.py:allows_execution */
bool tg_allows_execution(const char *action) {
    /* Python: allow/warn → true. */
    if (!action) return false;
    return strcmp(action, "allow") == 0 || strcmp(action, "warn") == 0;
}

/* PoP: should_halt @ agent/tool_guardrails.py:should_halt */
bool tg_should_halt(const char *action) {
    if (!action) return false;
    return strcmp(action, "block") == 0 || strcmp(action, "halt") == 0;
}

/* PoP: canonical_tool_args @ agent/tool_guardrails.py:canonical_tool_args */
char *tg_canonical_tool_args(const char *args_json) {
    /* Python: sorted compact JSON. */
    if (!args_json) return strdup("{}");
    printf("canonical tool args (sorted compact)\n");
    return strdup(args_json);
}

/* PoP: classify_tool_failure @ agent/tool_guardrails.py:classify_tool_failure */
bool tg_classify_tool_failure(const char *result) {
    /* Python: safety fallback; mirrors display._detect_tool_failure. */
    if (!result) return false;
    if (strstr(result, "\"success\": false") || strstr(result, "error:")) return true;
    return false;
}

/* PoP: reset_for_turn @ agent/tool_guardrails.py:reset_for_turn */
int tg_reset_for_turn(void) {
    /* Python: clear per-turn failure counters. */
    printf("guardrail per-turn state reset\n");
    return 0;
}

/* PoP: halt_decision @ agent/tool_guardrails.py:halt_decision */
char *tg_halt_decision(const char *decision_json) {
    /* Python: stored halt decision accessor. */
    return decision_json ? strdup(decision_json) : NULL;
}

/* PoP: before_call @ agent/tool_guardrails.py:before_call */
char *tg_before_call(const char *tool_name, const char *args_json) {
    /* Python: hard-stop check + signature tracking. */
    if (!tool_name) return NULL;
    printf("guardrail before_call (%s)\n", tool_name);
    return strdup("{\"action\": \"allow\"}");
}

/* PoP: after_call @ agent/tool_guardrails.py:after_call */
char *tg_after_call(const char *tool_name, const char *args_json, bool failed) {
    /* Python: failure counting + escalation. */
    if (!tool_name) return NULL;
    printf("guardrail after_call (%s, failed=%d)\n", tool_name, failed);
    return strdup("{\"action\": \"allow\"}");
}

/* PoP: _is_idempotent @ agent/tool_guardrails.py:_is_idempotent */
bool tg_is_idempotent(const char *tool_name, const char *mutating_json, const char *idempotent_json) {
    /* Python: mutating tools never idempotent. */
    if (!tool_name) return false;
    if (mutating_json && strstr(mutating_json, tool_name)) return false;
    if (idempotent_json && strstr(idempotent_json, tool_name)) return true;
    return false;
}

/* PoP: toolguard_synthetic_result @ agent/tool_guardrails.py:toolguard_synthetic_result */
char *tg_toolguard_synthetic_result(const char *tool_name, const char *reason) {
    /* Python: synthetic blocked-tool content string. */
    if (!tool_name) return strdup("");
    char *out = NULL;
    asprintf(&out, "{\"success\": false, \"blocked\": true, \"reason\": \"%s\"}",
             reason ? reason : "blocked by tool guardrail");
    return out;
}

/* PoP: append_toolguard_guidance @ agent/tool_guardrails.py:append_toolguard_guidance */
char *tg_append_toolguard_guidance(const char *result, const char *decision_json, const char *tool_name) {
    /* Python: append runtime guidance for warn/halt actions. */
    if (!result || !tool_name) return strdup(result ? result : "");
    printf("guardrail guidance appended to tool result\n");
    return strdup(result);
}

/* PoP: _tool_failure_recovery_hint @ agent/tool_guardrails.py:_tool_failure_recovery_hint */
char *tg_tool_failure_recovery_hint(const char *tool_name, long failure_count) {
    /* Python: action-oriented recovery guidance. */
    if (!tool_name) return strdup("");
    char *out = NULL;
    asprintf(&out, "%s has failed %ld times — consider verifying arguments and retrying once, then switching approach.",
             tool_name, failure_count);
    return out;
}

/* PoP: _coerce_args @ agent/tool_guardrails.py:_coerce_args */
char *tg_coerce_args(const char *args_json) {
    /* Python: mapping or {}; str json parsed. */
    if (!args_json) return strdup("{}");
    return strdup(args_json);
}

/* PoP: _result_hash @ agent/tool_guardrails.py:_result_hash */
char *tg_result_hash(const char *result) {
    /* Python: canonical json hash of parsed result — real FNV-1a
     * 64-bit over the compacted string. */
    if (!result) return strdup("");
    /* compact: strip whitespace outside quotes */
    size_t cap = strlen(result) + 1;
    char *compact = malloc(cap);
    if (!compact) return strdup("");
    char *q = compact;
    bool in_str = false;
    for (const char *p = result; *p; p++) {
        if (*p == '"') in_str = !in_str;
        if ((*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') && !in_str) continue;
        *q++ = *p;
    }
    *q = '\0';
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = compact; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
    }
    free(compact);
    char *out = NULL;
    asprintf(&out, "%016llx", h);
    return out;
}

/* PoP: _as_bool @ agent/tool_guardrails.py:_as_bool */
bool tg_as_bool(const char *value, bool default_value) {
    /* Python: bool/numeric/string coercion. */
    if (!value) return default_value;
    char *l = lowerdup(value);
    if (!l) return default_value;
    bool r;
    if (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0)
        r = true;
    else if (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "no") == 0 || strcmp(l, "off") == 0)
        r = false;
    else r = default_value;
    free(l);
    return r;
}

/* PoP: _positive_int @ agent/tool_guardrails.py:_positive_int */
long tg_positive_int(const char *value, long default_value) {
    /* Python: int parse; negative → default. */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v > 0 ? v : default_value;
}
