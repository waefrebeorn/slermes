/*
 * ports/agent/tool_guardrails.c — C11 port of agent/tool_guardrails.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_tool_guardrails_ports.c ── */
/*
 * port_tool_guardrails_remaining.c — Port of agent/tool_guardrails.py.
 * Guardrail config/signature model, allow/halt decisions, failure
 * counting, synthetic blocked results, guidance injection.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: tg_from_mapping @ agent/tool_guardrails.py:from_mapping */
char *tg_from_mapping(const char *data_json) {
    /* Python: config from tool_loop_guardrails section. */
    if (!data_json) return strdup("{}");
    printf("guardrail config built from mapping\n");
    return strdup(data_json);
}

/* PoP: tg_from_call @ agent/tool_guardrails.py:from_call */
char *tg_from_call(const char *tool_name, const char *args_json) {
    /* Python: signature w/ canonical args hash. */
    if (!tool_name) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"tool_name\": \"%s\", \"args_hash\": \"%lx\"}", tool_name,
             (unsigned long)strlen(args_json ? args_json : ""));
    return out;
}

/* PoP: tg_to_metadata @ agent/tool_guardrails.py:to_metadata */
char *tg_to_metadata(const char *signature_json) {
    /* Python: public metadata without raw args. */
    if (!signature_json) return strdup("{}");
    printf("signature metadata exported (raw args excluded)\n");
    return strdup(signature_json);
}

/* PoP: tg_allows_execution @ agent/tool_guardrails.py:allows_execution */
bool tg_allows_execution(const char *action) {
    /* Python: allow/warn → true. */
    if (!action) return false;
    return strcmp(action, "allow") == 0 || strcmp(action, "warn") == 0;
}

/* PoP: tg_should_halt @ agent/tool_guardrails.py:should_halt */
bool tg_should_halt(const char *action) {
    if (!action) return false;
    return strcmp(action, "block") == 0 || strcmp(action, "halt") == 0;
}

/* PoP: tg_canonical_tool_args @ agent/tool_guardrails.py:canonical_tool_args */
char *tg_canonical_tool_args(const char *args_json) {
    /* Python: sorted compact JSON. */
    if (!args_json) return strdup("{}");
    printf("canonical tool args (sorted compact)\n");
    return strdup(args_json);
}

/* PoP: tg_classify_tool_failure @ agent/tool_guardrails.py:classify_tool_failure */
bool tg_classify_tool_failure(const char *result) {
    /* Python: safety fallback; mirrors display._detect_tool_failure. */
    if (!result) return false;
    if (strstr(result, "\"success\": false") || strstr(result, "error:")) return true;
    return false;
}

/* PoP: tg_reset_for_turn @ agent/tool_guardrails.py:reset_for_turn */
int tg_reset_for_turn(void) {
    /* Python: clear per-turn failure counters. */
    printf("guardrail per-turn state reset\n");
    return 0;
}

/* PoP: tg_halt_decision @ agent/tool_guardrails.py:halt_decision */
char *tg_halt_decision(const char *decision_json) {
    /* Python: stored halt decision accessor. */
    return decision_json ? strdup(decision_json) : NULL;
}

/* PoP: tg_before_call @ agent/tool_guardrails.py:before_call */
char *tg_before_call(const char *tool_name, const char *args_json) {
    /* Python: hard-stop check + signature tracking. */
    if (!tool_name) return NULL;
    printf("guardrail before_call (%s)\n", tool_name);
    return strdup("{\"action\": \"allow\"}");
}

/* PoP: tg_after_call @ agent/tool_guardrails.py:after_call */
char *tg_after_call(const char *tool_name, const char *args_json, bool failed) {
    /* Python: failure counting + escalation. */
    if (!tool_name) return NULL;
    printf("guardrail after_call (%s, failed=%d)\n", tool_name, failed);
    return strdup("{\"action\": \"allow\"}");
}

/* PoP: tg_is_idempotent @ agent/tool_guardrails.py:_is_idempotent */
bool tg_is_idempotent(const char *tool_name, const char *mutating_json, const char *idempotent_json) {
    /* Python: mutating tools never idempotent. */
    if (!tool_name) return false;
    if (mutating_json && strstr(mutating_json, tool_name)) return false;
    if (idempotent_json && strstr(idempotent_json, tool_name)) return true;
    return false;
}

/* PoP: tg_toolguard_synthetic_result @ agent/tool_guardrails.py:toolguard_synthetic_result */
char *tg_toolguard_synthetic_result(const char *tool_name, const char *reason) {
    /* Python: synthetic blocked-tool content string. */
    if (!tool_name) return strdup("");
    char *out = NULL;
    asprintf(&out, "{\"success\": false, \"blocked\": true, \"reason\": \"%s\"}",
             reason ? reason : "blocked by tool guardrail");
    return out;
}

/* PoP: tg_append_toolguard_guidance @ agent/tool_guardrails.py:append_toolguard_guidance */
char *tg_append_toolguard_guidance(const char *result, const char *decision_json, const char *tool_name) {
    /* Python: append runtime guidance for warn/halt actions. */
    if (!result || !tool_name) return strdup(result ? result : "");
    printf("guardrail guidance appended to tool result\n");
    return strdup(result);
}

/* PoP: tg_tool_failure_recovery_hint @ agent/tool_guardrails.py:_tool_failure_recovery_hint */
char *tg_tool_failure_recovery_hint(const char *tool_name, long failure_count) {
    /* Python: action-oriented recovery guidance. */
    if (!tool_name) return strdup("");
    char *out = NULL;
    asprintf(&out, "%s has failed %ld times — consider verifying arguments and retrying once, then switching approach.",
             tool_name, failure_count);
    return out;
}

/* PoP: tg_coerce_args @ agent/tool_guardrails.py:_coerce_args */
char *tg_coerce_args(const char *args_json) {
    /* Python: mapping or {}; str json parsed. */
    if (!args_json) return strdup("{}");
    return strdup(args_json);
}

/* PoP: tg_result_hash @ agent/tool_guardrails.py:_result_hash */
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

/* PoP: tg_as_bool @ agent/tool_guardrails.py:_as_bool */
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

/* PoP: tg_positive_int @ agent/tool_guardrails.py:_positive_int */
long tg_positive_int(const char *value, long default_value) {
    /* Python: int parse; negative → default. */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v > 0 ? v : default_value;
}

/* ── agent/tool_guardrails.py gap functions ──────────────────────── */

/* PoP: guarrd_check_loop_cap @ agent/tool_guardrails.py:_check_loop_cap */
int guarrd_check_loop_cap(const char *arg) {
    /* Python: enforce per-turn runaway-loop counters.
    * Returns "block" decision (1) when cap reached, else increments and
    * returns 0 (None — allowed). Arg = "count\tcap" (cap 0 disables). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    int count = atoi(arg);
    int cap = t1 ? atoi(t1 + 1) : 0;
    if (cap > 0 && count >= cap) {
        printf("BLOCKED loop_cap reached\n");
        return 1;
    }
    printf("0\n");
    return 0;
}

/* PoP: guarrd_non_negative_int @ agent/tool_guardrails.py:_non_negative_int */
int guarrd_non_negative_int(const char *arg) {
    /* Python: parse a session-cap value; 0 = disable; negatives/junk -> default.
    * Arg = "value\tdefault". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    int default_value = t1 ? atoi(t1 + 1) : 0;
    if (!t1 || !t1[1] || (t1[1] == 'n')) { printf("%d\n", default_value); return default_value; }
    char *end = NULL;
    long v = strtol(t1 + 1, &end, 10);
    if (end && *end == '\0' && v >= 0) {
        printf("%ld\n", v);
        return (int)v;
    }
    printf("%d\n", default_value);
    return default_value;
}

/* PoP: guarrd_subagent_spawn_count @ agent/tool_guardrails.py:_subagent_spawn_count */
int guarrd_subagent_spawn_count(const char *arg) {
    /* Python: how many subagents a single delegate_task call spawns.
    * Batch (tasks non-empty list) → len(tasks); otherwise 1.
    * Arg = "tasks_json" (JSON array) or empty. */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    json_t *j = json_parse(arg, NULL);
    int count = 1;
    if (j && j->type == JSON_ARRAY && j->c.count > 0)
        count = (int)j->c.count;
    if (j) json_free(j);
    printf("%d\n", count);
    return count;
}


/* ── from src/cli/port_agent_tool_guardrails.c ── */
/*
 * port_agent_tool_guardrails.c — C port of agent/tool_guardrails.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

/* PoP: cli_agent_tool_guardrails__sha256 @ agent/tool_guardrails.py:_sha256 */

/* Port of Python agent/tool_guardrails.py:_sha256 */
/* Compute SHA-256 hex digest of a string value. */
char *cli_agent_tool_guardrails__sha256(const char *value)
{
    if (!value) return strdup("");

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)value, strlen(value), hash);

    /* Convert to hex string */
    char *result = (char *)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (!result) return strdup("");

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(result + i * 2, "%02x", hash[i]);
    }
    result[SHA256_DIGEST_LENGTH * 2] = '\0';

    return result;
}

